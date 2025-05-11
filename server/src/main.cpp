#include "stdafx.hpp"

#include "net/netInclude.hpp"
#include "OverlappedEx.hpp"
#include "Session.hpp"

#include "game/physicsSystem.hpp"
#include "resourcePath.hpp"

void doAccept( SOCKET, OverlappedEx* );
void worker( );

HANDLE gIocpHandle;
SOCKET listenSocket;
OverlappedEx gAcceptOver{ IO_OP::IO_ACCEPT };
std::array<char, (sizeof(sockaddr) + 16u) * 2u> acceptBuffer_;

ccMap<std::uint16_t, Session> gUsers;
ccQueue<ecs::Entity::ID> gReservedEntities;
std::atomic_int16_t gId;

std::uniform_real_distribution<float> gDist( -1.f, 1.f );
std::mt19937 gRng( std::random_device{}( ) );

void processPacket(Packet& packet, Session& session);
void processCSInput(CSInput& csInput, Session& session);

int main( ) {
	using Clock = std::chrono::high_resolution_clock;
	using Seconds = std::chrono::duration<float>;
	static constexpr auto frameRate = 33_ms;	// 30 FPS
	static constexpr auto directionChangeRate = 2.f;
	float directionChangeCounter = 0.f;

	ecs::init( ecs::InitDesc{ .threadCnt = 1u, .entityPoolSize = 0x160u } );

	// initialize windows socket api ================================================
	WSADATA wsaData{ };
	if ( ::WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) {
		errorDisplay( "WSAStartup", WSAGetLastError( ) );
	}

	listenSocket = ::WSASocket( AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED );
	if ( listenSocket == INVALID_SOCKET ) {
		errorDisplay( "WSASocket(listenSocket)", WSAGetLastError( ) );
	}

	auto serverAddr = sockaddr_in{
		.sin_family = AF_INET,
		.sin_port = ::htons( PORT )
	};
	serverAddr.sin_addr.s_addr = ::htonl( INADDR_ANY );

	if ( ::bind( listenSocket, reinterpret_cast<sockaddr*>( &serverAddr ), sizeof( serverAddr ) ) == SOCKET_ERROR ) {
		errorDisplay( "bind", WSAGetLastError( ) );
	}

	if ( ::listen( listenSocket, SOMAXCONN ) == SOCKET_ERROR ) {
		errorDisplay( "listen", WSAGetLastError( ) );
	}
	//===============================================================================

	auto physicsSystem = PhysicsSystem( );
	auto coordRoot = gameEngine::CoordRoot( );

	coordRoot.update( );

	auto lastTp = Clock::now( );
	
	// create IOCP handle & register listen socket & do accept ======================
	gIocpHandle = ::CreateIoCompletionPort( INVALID_HANDLE_VALUE, nullptr, 0, 0 );
	if ( gIocpHandle == nullptr ) {
		errorDisplay( "CreateIoCompletionPort", GetLastError( ) );
	}
	::CreateIoCompletionPort( reinterpret_cast<HANDLE>( listenSocket ), gIocpHandle, gId.load( ), 0 );
	gId.fetch_add( 1 );

	doAccept( listenSocket, &gAcceptOver );
	//===============================================================================

	// create worker threads ========================================================
	auto threadCnt = std::thread::hardware_concurrency( );
	auto threads = std::vector<std::thread>( );
	threads.reserve( threadCnt );
	for ( auto i = 0u; i < threadCnt; ++i ) {
		threads.emplace_back( worker );
	}
	//===============================================================================

	// game logic loop ==============================================================
	while ( true ) {
		auto tp = Clock::now( );
		auto elapsed = std::chrono::duration_cast<MilliSeconds>( tp - lastTp );

		if ( elapsed < frameRate / 2.f ) {
			std::this_thread::sleep_for( std::chrono::duration<float>( frameRate - elapsed ) );
			elapsed = frameRate;
		}

		ecs::Entity::ID entityId{-1u};

		while ( gReservedEntities.try_pop(entityId) ) {
			auto entity = ecs::Entity(entityId);
			if ( entity.valid() ) {
				coordRoot.addEntity(entity);
				physicsSystem.addEntity(entity);
				entityId = -1u;
				entity.release();
			}
		}

		// physically update
		physicsSystem.update( elapsed );

		SCMove curPacket{};

		for ( auto& [id, user] : gUsers ) {
			if ( !user.accessReady( ) ) {
				continue;
			}
			const auto eid = user.getEntityId();

			if (auto pCoord = gameEngine::Coord::at(eid)) {
				const auto cdp = pCoord->compressedDeltaPos();
				const auto cdr = pCoord->compressedDeltaRot();
				pCoord->resetDeltaPos();
				pCoord->resetDeltaRot();

				curPacket.moves[curPacket.moveCnt++] = SCMove::Value{
					.netId = eid,
					.compressedDeltaPos = cdp,
					.compressedDeltaRot = cdr
				};

				if (curPacket.moveCnt == SCMove::maxMoveCnt) {
					Session::enqueueBroadcastPacket(
						Packet{
							.size = calcPacketSize<SCMove>(SCMove::maxMoveCnt),
							.type = PacketType::SCMove,
							.scMove = curPacket
						}
					);
					curPacket.moveCnt = 0;
				}

				const auto dp = pCoord->decodeDeltaPos(cdp);
				const auto dr = pCoord->decodeDeltaRot(cdr);

				pCoord->get() << mu::translate(dp);
				if (auto pModel = DummyModel::at(eid)) {
					pModel->coord() << mu::Mat4x4(dr);
				}
			}
		}

		if (curPacket.moveCnt > 0) {
			Session::enqueueBroadcastPacket(
				Packet{
					.size = calcPacketSize<SCMove>(curPacket.moveCnt),
					.type = PacketType::SCMove,
					.scMove = curPacket
				}
			);
		}

		coordRoot.update( );

		auto lUsers = pmr::vector<Session*>();
		for (auto& [id, session] : gUsers) {
			if (session.accessReady()) {
				session.doSend();
				lUsers.push_back(&session);
			}
		}
		Session::doBroadcast(std::move(lUsers));

		lastTp = tp;
	}
	//===============================================================================

	for ( auto& t : threads ) {
		t.join( );
	}

	::closesocket( listenSocket );
	::WSACleanup( );
}

void doAccept( SOCKET listenSocket, OverlappedEx* acceptOver ) {
	acceptOver->acceptSocket_ = ::WSASocket( AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED );
	if ( acceptOver->acceptSocket_ == INVALID_SOCKET ) {
		errorDisplay( "WSASocket(acceptSocket)", WSAGetLastError( ) );
	}
	
	auto ret = ::AcceptEx( listenSocket, acceptOver->acceptSocket_, acceptBuffer_.data( ), 
		0, sizeof( sockaddr_in ) + 16, sizeof( sockaddr_in ) + 16, nullptr, &acceptOver->over_ );
	if ( !ret ) {
		if ( WSAGetLastError( ) != WSA_IO_PENDING ) {
			errorDisplay( "AcceptEx", WSAGetLastError( ) );
		}
	}
}

void worker( ) {
	while ( true ) {
		DWORD bytesTransferred{ };
		ULONG_PTR completionKey{ };
		OVERLAPPED* pOver = nullptr;
		auto ret = GetQueuedCompletionStatus( gIocpHandle, &bytesTransferred, &completionKey, &pOver, INFINITE );
		auto overEx = reinterpret_cast<OverlappedEx*>( pOver );

		switch ( overEx->type_ ) {
		case IO_OP::IO_ACCEPT: {
			auto id = gId++;

			::CreateIoCompletionPort( reinterpret_cast<HANDLE>( overEx->acceptSocket_ ), gIocpHandle, id, 0 );

			auto& initializingSession = gUsers[id].init( overEx->acceptSocket_, id );
			// socket error check
			if (!initializingSession.valid()) {
				errorDisplay( "Session creation", WSAGetLastError( ) );
				initializingSession.close();
				doAccept( listenSocket, &gAcceptOver );
				break;
			}

			initializingSession.setPacketProcessor( processPacket );

			auto entity = ecs::Entity( );
			entity.createComponent<gameEngine::Coord>();
			entity.createComponent<RigidBody>();
			auto& rb = entity.as<RigidBody>();
			rb.setInvMass( 1.f / 50.f );
			rb.setKFriction( 0.5f );
			rb.disableGravity( );
			entity.createComponent<DummyModel>( entity.as<gameEngine::Coord>() );

			initializingSession.setEntityId( entity.id( ).value( ) );

			gReservedEntities.push(entity.id().value());

			entity.release();

			// add intialization packets
			for (auto& [id, user] : gUsers) {
				if (!user.accessReady()) {
					continue;
				}


				auto trs = mu::Vec3();
				auto rot = mu::NQuat();

				if (auto pCoord = gameEngine::Coord::at(user.getEntityId())) {
					// we endure data race of translation
					trs = pCoord->get().localXform().row(3);
				}
				if (auto pModel = DummyModel::at(user.getEntityId())) {
					rot = mu::quatRotMat(pModel->coord().localXform());
					// if rotation is not near identity, recalculate rotation
				}

				// enqueue SCEnter packet
				initializingSession.enqueuePacket(
					Packet{
						.size = calcPacketSize<SCEnter>(),
						.type = PacketType::SCEnter,
						.scEnter = SCEnter{
							.netId = user.getEntityId(),
							.xform = RigidXform{
								.translation = { trs.x(), trs.y(), trs.z() },
								.rotation = { rot.x(), rot.y(), rot.z(), rot.w() }
							},
							.objType = ObjectType::Character
						}
					}
				);

				// the session may be destroyed in the process of enqueueing by other threads,
				// so we need to check if the session is still accessible
				if (!user.accessReady()) {
					initializingSession.revertEnqueuePacket();
				}
			}

			initializingSession.enqueuePacket(
				Packet{
					.size = calcPacketSize<SCAssign>(),
					.type = PacketType::SCAssign,
					.scAssign = SCAssign{
						.netId = initializingSession.getEntityId()
					}
				}
			);

			Session::enqueueBroadcastPacket(
				Packet{
					.size = calcPacketSize<SCEnter>(),
					.type = PacketType::SCEnter,
					.scEnter = SCEnter{
						.netId = initializingSession.getEntityId(),
						.xform = RigidXform{
							.translation = { 0.f, 0.f, 0.f },
							.rotation = { 0.f, 0.f, 0.f, 1.f }
						},
						.objType = ObjectType::Character
					}
				}
			);

			// set accept flag
			initializingSession.setAcceptFlag( );
			initializingSession.doRecv();

			doAccept( listenSocket, &gAcceptOver );
			break;
		}

		case IO_OP::IO_RECV: {
			auto id = static_cast<std::uint16_t>( completionKey );
			if (gUsers[id].accessReady()) {
				gUsers[id].interpretData(bytesTransferred);
				gUsers[id].doRecv();
			}
			break;
		}

		case IO_OP::IO_SEND:
			delete overEx;
			break;
		}
	}
}

void processPacket(Packet& packet, Session& session) {
	switch ( packet.type ) {
	case PacketType::CSInput:
		processCSInput( packet.csInput, session );
		break;
	default:
		break;
	}
}

void processCSInput(CSInput& csInput, Session& session) {
	static constexpr auto forceStep = 600.f;

	for ( std::uint8_t i = 0u; i < csInput.eventCnt; ++i ) {
		auto& ev = csInput.events[ i ];
		switch ( ev.type ) {
		case InputEventType::MoveForward:
			if (auto pRigidBody = RigidBody::at( session.getEntityId() )) {
				auto pModel = DummyModel::at( session.getEntityId() );
				pRigidBody->accMomentum( forceStep * ev.floatVal0 * mu::Vec3(
					pModel->coord().xform().row(2u)
				) );
			}
			break;

		case InputEventType::MoveBackward:
			if (auto pRigidBody = RigidBody::at( session.getEntityId() )) {
				auto pModel = DummyModel::at( session.getEntityId() );
				pRigidBody->accMomentum( -forceStep * ev.floatVal0 * mu::Vec3(
					pModel->coord().xform().row(2u)
				) );
			}
			break;

		case InputEventType::MoveLeft:
			if (auto pRigidBody = RigidBody::at( session.getEntityId() )) {
				auto pModel = DummyModel::at( session.getEntityId() );
				pRigidBody->accMomentum( -forceStep * ev.floatVal0 * mu::Vec3(
					pModel->coord().xform().row(0u)
				) );
			}
			break;

		case InputEventType::MoveRight:
			if (auto pRigidBody = RigidBody::at( session.getEntityId() )) {
				auto pModel = DummyModel::at( session.getEntityId() );
				pRigidBody->accMomentum( forceStep * ev.floatVal0 * mu::Vec3(
					pModel->coord().xform().row(0u)
				) );
			}
			break;

		case InputEventType::Rotation:
			if (auto pCoord = gameEngine::Coord::at( session.getEntityId() )) {
				pCoord->accRotation( mu::quatRPY(0.f, 0.f, ev.floatVal0) );
			}
			break;

		default:
			break;
		}
	}
}