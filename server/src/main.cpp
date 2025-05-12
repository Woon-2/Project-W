#include "stdafx.hpp"

#include "net/netInclude.hpp"
#include "OverlappedEx.hpp"
#include "Session.hpp"

#include "game/physicsSystem.hpp"
#include "resourcePath.hpp"

#include "FreeImage.h"

class Bitmap {
public:
	Bitmap()
		: pBitmap_(nullptr), width_(0u), height_(0u), bits_(nullptr) {}

	Bitmap( const std::filesystem::path& path )
		: pBitmap_(nullptr), width_(0u), height_(0u), bits_(nullptr) {
		load( path );
	}

	Bitmap(const Bitmap&) = delete;
	Bitmap& operator=(const Bitmap&) = delete;
	Bitmap(Bitmap&& other) noexcept;
	Bitmap& operator=(Bitmap&& other) noexcept;

	~Bitmap() {
		unload();
	}

	void load( const std::filesystem::path& path );
	BYTE getGreyscalePixel( size_t x, size_t y ) const;
	void unload();

	std::size_t width() const noexcept {
		return width_;
	}

	std::size_t height() const noexcept {
		return height_;
	}

private:
	FIBITMAP* pBitmap_;
	std::size_t width_;
	std::size_t height_;
	unsigned char* bits_;
};

Bitmap::Bitmap(Bitmap&& other) noexcept
    : pBitmap_(std::exchange(other.pBitmap_, nullptr)),
    width_(std::exchange(other.width_, 0)),
    height_(std::exchange(other.height_, 0)),
    bits_(std::exchange(other.bits_, nullptr)) {}

Bitmap& Bitmap::operator=(Bitmap&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    pBitmap_ = std::exchange(other.pBitmap_, nullptr);
    width_ = std::exchange(other.width_, 0);
    height_ = std::exchange(other.height_, 0);
    bits_ = std::exchange(other.bits_, nullptr);

    return *this;
}

void Bitmap::load( const std::filesystem::path& path ) {
    auto strFileName = path.string();
    auto cstrFileName = strFileName.c_str();

    FREE_IMAGE_FORMAT format = FreeImage_GetFileType( cstrFileName );

    if ( format == -1 )
    {
        std::cerr << "Could not find image: \"" << path << "\"\n";
        return;
    }
    else if ( format == FIF_UNKNOWN )
    {
        std::cerr << "Couldn't determine file format - attempting to get from file extension...\n";
        format = FreeImage_GetFIFFromFilename( cstrFileName );

        if ( !FreeImage_FIFSupportsReading( format ) )
        {
            std::cerr << "Detected image format cannot be read!\n";
            return;
        }
    }

    FIBITMAP* bitmap = FreeImage_Load( format, cstrFileName );
    int bits_per_pixel = FreeImage_GetBPP( bitmap );

    if ( bits_per_pixel == 32 )
    {
        pBitmap_ = bitmap;
    }
    else
    {
        pBitmap_ = FreeImage_ConvertTo32Bits( bitmap );
    }

    width_ = FreeImage_GetWidth( pBitmap_ );
    height_ = FreeImage_GetHeight( pBitmap_ );
    bits_ = FreeImage_GetBits( pBitmap_ );
}

BYTE Bitmap::getGreyscalePixel( size_t x, size_t y ) const {
    RGBQUAD ret{};
    if ( !FreeImage_GetPixelColor( pBitmap_, static_cast<unsigned int>(x), 
        static_cast<unsigned int>(y), &ret 
    ) ) {
        std::cerr << "Failed to get pixel\n";
    }
    return ret.rgbRed;
}

void Bitmap::unload() {
    if ( pBitmap_ )
    {
        FreeImage_Unload( pBitmap_ );
        pBitmap_ = nullptr;
        width_ = 0;
        height_ = 0;
        bits_ = nullptr;
    }
}

void doAccept( SOCKET, OverlappedEx* );
void worker( );

HANDLE gIocpHandle;
SOCKET listenSocket;
OverlappedEx gAcceptOver{ IO_OP::IO_ACCEPT };
std::array<char, (sizeof(sockaddr) + 16u) * 2u> acceptBuffer_;

ccMap<std::uint16_t, Session> gUsers;
ccQueue<ecs::Entity::ID> gReservedEntities;
ccQueue<u16t> gIdPool;
std::vector<std::vector<Bitmap>> gHeightmaps;

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

	for ( auto i = 0u; i < 0xFFFF; ++i ) {
		gIdPool.push( i );
	}

	ecs::init( ecs::InitDesc{ .threadCnt = 1u, .entityPoolSize = 0x160u } );

	gHeightmaps.resize( 3u );
	for ( auto i = 0u; i < gHeightmaps.size( ); ++i ) {
		gHeightmaps[ i ].reserve( 3u );
		for ( auto j = 0u; j < 3u; ++j ) {
			gHeightmaps[ i ].emplace_back( resourcePath/ ("terrains/HeightMaps/Terrain_"+std::to_string( i )+"_"+std::to_string( j )+"_HeightMapGrey.dds") );
		}
	}

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
	::CreateIoCompletionPort( reinterpret_cast<HANDLE>( listenSocket ), gIocpHandle, -1, 0 );
	doAccept( listenSocket, &gAcceptOver );
	//===============================================================================

	// create worker threads ========================================================
	auto threadCnt = std::thread::hardware_concurrency( ) / 2;
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
			Sleep( static_cast<DWORD>( frameRate.count() / 2.f - elapsed.count() ) );
			// std::cout << "sleep\n";
			// std::this_thread::sleep_for( std::chrono::duration<float>( frameRate / 2.f - elapsed ) );
			elapsed = frameRate / 2.f;
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
				auto cdp = pCoord->compressedDeltaPos();
				const auto cdr = pCoord->compressedDeltaRot();
				pCoord->resetDeltaPos();
				pCoord->resetDeltaRot();

				const auto trs = pCoord->get().xform().row(3);

				auto heightmapRow = std::clamp(
					static_cast<std::size_t>(trs.x() / 100.f),
					0ull, 3ull
				);
				auto heightmapCol = std::clamp(
					static_cast<std::size_t>(trs.z() / 100.f),
					0ull, 3ull
				);

				auto dp = pCoord->decodeDeltaPos(cdp);

				auto y00 = gHeightmaps[heightmapRow][heightmapCol].getGreyscalePixel(
					static_cast<std::size_t>(trs.x()) % 100u * gHeightmaps[heightmapRow][heightmapCol].width() / 100u,
					static_cast<std::size_t>(trs.z()) % 100u * gHeightmaps[heightmapRow][heightmapCol].height() / 100u
				) * 200.f / 255.f - 25.f - trs.y();

				auto y01 = 0.f;
				if ((static_cast<std::size_t>(trs.z()) % 100u + 1u) >= 100u) {
					y01 = y00;
				}
				else {
					y01 = gHeightmaps[heightmapRow][heightmapCol].getGreyscalePixel(
						static_cast<std::size_t>(trs.x()) % 100u * gHeightmaps[heightmapRow][heightmapCol].width() / 100u,
						(static_cast<std::size_t>(trs.z()) % 100u + 1u) * gHeightmaps[heightmapRow][heightmapCol].height() / 100u
					) * 200.f / 255.f - 25.f - trs.y();
				}

				auto y10 = 0.f;
				if ((static_cast<std::size_t>(trs.x()) % 100u + 1u) >= 100u) {
					y10 = y00;
				}
				else {
					y10 = gHeightmaps[heightmapRow][heightmapCol].getGreyscalePixel(
						(static_cast<std::size_t>(trs.x()) % 100u + 1u) * gHeightmaps[heightmapRow][heightmapCol].width() / 100u,
						static_cast<std::size_t>(trs.z()) % 100u * gHeightmaps[heightmapRow][heightmapCol].height() / 100u
					) * 200.f / 255.f - 25.f - trs.y();
				}

				auto y11 = 0.f;
				if ((static_cast<std::size_t>(trs.x()) % 100u + 1u) == 100u &&
					(static_cast<std::size_t>(trs.z()) % 100u + 1u) == 100u) {
					y11 = y00;
				}
				else if ((static_cast<std::size_t>(trs.x()) % 100u + 1u) >= 100u) {
					y11 = y01;
				}
				else if ((static_cast<std::size_t>(trs.z()) % 100u + 1u) >= 100u) {
					y11 = y10;
				}
				else {
					y11 = gHeightmaps[heightmapRow][heightmapCol].getGreyscalePixel(
						(static_cast<std::size_t>(trs.x()) % 100u + 1u) * gHeightmaps[heightmapRow][heightmapCol].width() / 100u,
						(static_cast<std::size_t>(trs.z()) % 100u + 1u) * gHeightmaps[heightmapRow][heightmapCol].height() / 100u
					) * 200.f / 255.f - 25.f - trs.y();
				}

				auto u = trs.x() - static_cast<std::size_t>(trs.x()) % 100u;
				auto v = trs.z() - static_cast<std::size_t>(trs.z()) % 100u;
				auto h00 = y00 * (1.f - u) + y10 * u;
				auto h01 = y01 * (1.f - u) + y11 * u;
				auto h = h00 * (1.f - v) + h01 * v;

				dp = mu::Vec3(dp.x(),
					h,
					dp.z()
				);
				pCoord->accTranslation(dp);
				cdp = pCoord->compressedDeltaPos();
				pCoord->resetDeltaPos();

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
				// std::cout << "Send!\n";
				session.doSend();
				lUsers.push_back(&session);
			}
		}
		//std::cout << lUsers.size( ) << " users\n";
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

		if ( !ret ) {
			auto err = GetLastError( );
			if ( err != WAIT_TIMEOUT ) {
				//errorDisplay( "GetQueuedCompletionStatus", err );

				auto eId = gUsers[ static_cast<u16t>( completionKey ) ].getEntityId( );
				auto packet = Packet{
					.size = calcPacketSize<SCLeave>( ),
					.type = PacketType::SCLeave,
					.scLeave = SCLeave{
						.leaveCnt = 1u,
						.leavedIds = { eId }
					}
				};

				Session::enqueueBroadcastPacket( packet );

				if ( gUsers[ static_cast<u16t>( completionKey ) ].close( ) ) {
					gIdPool.push( static_cast<u16t>( completionKey ) );
				}
			}
			continue;
		}

		if ( ( overEx->type_ == IO_OP::IO_RECV || overEx->type_ == IO_OP::IO_SEND )
			&& bytesTransferred == 0 ) {
			auto eId = gUsers[ static_cast<u16t>( completionKey ) ].getEntityId( );
			auto packet = Packet{
				.size = calcPacketSize<SCLeave>( ),
				.type = PacketType::SCLeave,
				.scLeave = SCLeave{
					.leaveCnt = 1u,
					.leavedIds = { eId }
				}
			};

			Session::enqueueBroadcastPacket( packet );

			if ( gUsers[ static_cast<u16t>( completionKey ) ].close( ) ) {
				gIdPool.push( static_cast<u16t>( completionKey ) );
			}

			if ( overEx->type_ == IO_OP::IO_SEND ) {
				delete overEx;
			}
			continue;
		}

		switch ( overEx->type_ ) {
		case IO_OP::IO_ACCEPT: {
			u16t id{ };
			if ( !gIdPool.try_pop( id ) ) {
				std::cerr << "Failed to pop id from pool\n";
				closesocket( overEx->acceptSocket_ );
				doAccept( listenSocket, &gAcceptOver );
				break;
			}

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
			entity.as<gameEngine::Coord>().get() << mu::translate( 0.f, -25.f, 0.f );
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
							.translation = { 0.f, -25.f, 0.f },
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

	case PacketType::CSLeave: {
		auto eId = session.getEntityId( );
		auto packet = Packet{
			.size = calcPacketSize<SCLeave>( ),
			.type = PacketType::SCLeave,
			.scLeave = SCLeave{
				.leaveCnt = 1u,
				.leavedIds = { eId }
			}
		};

		Session::enqueueBroadcastPacket( packet );

		if ( session.close( ) ) {
			gIdPool.push( session.getEntityId( ) );
		}
		break;
	}

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