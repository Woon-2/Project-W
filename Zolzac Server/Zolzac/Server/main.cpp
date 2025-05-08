#define NOMINMAX

#include "sNetEx.hpp"
#include "netInclude.hpp"
#include "OverlappedEx.hpp"
#include "Session.hpp"

#include "physicsSystem.hpp"

#include <unordered_map>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

void doAccept( SOCKET, OverlappedEx* );
void worker( );

HANDLE gIocpHandle;
SOCKET listenSocket;
OverlappedEx gAcceptOver{ IO_OP::IO_ACCEPT };
std::array<char, bufferSize> acceptBuffer_;

std::unordered_map<std::uint16_t, Session> gUsers;
std::atomic_int16_t gId;

SNetExSystem netSystem;

// dummy model to store rotation coordinate system
class DummyModel : public ecs::Component {
public:
	ENABLE_COMPONENT( DummyModel )

	DummyModel( const ecs::Entity& entity, gameEngine::Coord& coordComp )
	: Component( entity ), coord_( ) {
		coord_.setParent( &coordComp.get( ) );
	}

	const gfx::coord::System& coord( ) const NOEXCEPT {
		return coord_;
	}

	gfx::coord::System& coord( ) NOEXCEPT {
		return coord_;
	}

private:
	gfx::coord::System coord_;
};

std::uniform_real_distribution<float> gDist( -1.f, 1.f );
std::mt19937 gRng( std::random_device{}( ) );

int main( ) {
	using Clock = std::chrono::high_resolution_clock;
	using Seconds = std::chrono::duration<float>;
	static constexpr auto frameRate = 0.016f;	// 60 FPS
	static constexpr auto directionChangeRate = 2.f;
	float directionChangeCounter = 0.f;

	ecs::init( ecs::InitDesc{ .threadCnt = 1u, .entityPoolSize = 0x160u } );
	IDPool::initList( );

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
		.sin_port = ::htons( serverPort )
	};
	serverAddr.sin_addr.s_addr = ::htonl( INADDR_ANY );

	if ( ::bind( listenSocket, reinterpret_cast<sockaddr*>( &serverAddr ), sizeof( serverAddr ) ) == SOCKET_ERROR ) {
		errorDisplay( "bind", WSAGetLastError( ) );
	}

	if ( ::listen( listenSocket, SOMAXCONN ) == SOCKET_ERROR ) {
		errorDisplay( "listen", WSAGetLastError( ) );
	}
	//===============================================================================

	auto levelEntity = gameEngine::LevelRegion( "LevelGraph.bin" );

	auto physicsSystem = PhysicsSystem( );
	auto coordRoot = gameEngine::CoordRoot( );

	auto entities = levelEntity.instantiateAllObjects( coordRoot.get( ) );
	auto directions = std::vector<mu::Vec3>( entities.size( ) - 5u );
	for ( auto& dir : directions ) {
		dir = mu::Vec3( gDist( gRng ), 0.f, gDist( gRng ) );
	}

	for ( std::size_t i = 0u; i < entities.size( ); ++i ) {
		auto& entity = entities[ i ];
		if ( i < 5u ) {
			entity.createComponent<NetEx>( std::make_unique<SNetExHelicopter>( entity.id( ).value( ) ) );
			entity.as<NetEx>( ).addCategory( NetExCategory::Player );
			entity.as<NetEx>( ).addCategory( NetExCategory::Character );
		}
		else {
			entity.createComponent<NetEx>( std::make_unique<SNetExAI>( entity.id( ).value( ) ) );
			entity.as<NetEx>( ).addCategory( NetExCategory::AI );
			entity.as<NetEx>( ).addCategory( NetExCategory::Character );
		}
		entity.createComponent<RigidBody>( );
		entity.createComponent<DummyModel>( entity.as<gameEngine::Coord>( ) );

		netSystem.addEntity( entity );
		physicsSystem.addEntity( entity );
		coordRoot.addEntity( entity );
	}

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
		auto elapsed = std::chrono::duration_cast<Seconds>( tp - lastTp ).count( );

		if ( elapsed < frameRate / 2.f ) {
			std::this_thread::sleep_for( std::chrono::duration<float>( frameRate - elapsed ) );
			elapsed = frameRate;
		}

		netSystem.preUpdate( );

		// physically update
		const float forceStep_ = 800.f;
		for ( std::size_t i = 5u; i < entities.size( ); ++i ) {
			auto& entity = entities[ i ];
			auto& coord = entity.as<gameEngine::Coord>( );
			auto& rigidBody = entity.as<RigidBody>( );
			auto& netEx = entity.as<NetEx>( );

			rigidBody.addForce( directions[ i - 5u ] * forceStep_ * elapsed );
		}

		physicsSystem.update( elapsed );

		for ( std::size_t i = 5u; i < entities.size( ); ++i ) {
			auto& entity = entities[ i ];
			auto& coord = entity.as<gameEngine::Coord>( );
			auto& rigidBody = entity.as<RigidBody>( );
			coord.get( ) << mu::translate( rigidBody.deltaPosition( ) );
		}

		directionChangeCounter += elapsed;
		if ( directionChangeCounter > directionChangeRate ) {
			directionChangeCounter = 0.f;
			for ( auto& dir : directions ) {
				dir = mu::Vec3( gDist( gRng ), 0.f, gDist( gRng ) );
			}
		}

		coordRoot.update( );

		netSystem.postUpdate( );
		netSystem.doSend( );

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

			if ( !gUsers.try_emplace( id, overEx->acceptSocket_, id ).second ) {
				errorDisplay( "Session creation", WSAGetLastError( ) );
				::closesocket( overEx->acceptSocket_ );
			}
			else {
				netSystem.addSession( gUsers[ id ] );
				gUsers[ id ].setAcceptFlag( );
			}

			doAccept( listenSocket, &gAcceptOver );
			break;
		}

		case IO_OP::IO_RECV: {
			auto id = static_cast<std::uint16_t>( completionKey );
			gUsers[ id ].interpretData( bytesTransferred );
			gUsers[ id ].doRecv( );
			break;
		}

		case IO_OP::IO_SEND:
			delete overEx;
			break;
		}
	}
}