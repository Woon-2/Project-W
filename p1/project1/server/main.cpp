#include "pch.hpp"
#include "Service.hpp"
#include "IocpCore.hpp"
#include "Session.hpp"

class GameSession : public Session {
public:
	GameSession( ) : Session( ) {}
	virtual ~GameSession( ) { }

	virtual void onConnected( ) override {
		std::cout << "GameSession connected.\n";
	}

	virtual void onDisconnected( ) override {
		std::cout << "GameSession disconnected.\n";
	}

	virtual int32 onRecv( char* buffer, int32 len ) override {
		std::cout << "GameSession received " << len << " bytes.\n";
		return len;
	}
};

int main( )
{
	SocketUtils::init( );

	auto service = std::make_shared<ServerService>(
		NetAddress( serverIp, serverPort ),
		std::make_shared<IocpCore>( ), nullptr, 100
	);

	// temporary
	service->setSessionFactory( []( ) {
		return std::make_shared<GameSession>( );
	} );

	ASSERT_CRASH( service->start( ) );

	std::thread t1( [ &service ]( ) {
		while ( true ) {
			service->getIocpCore( )->dispatch( );
		}
	} );
	t1.join( );

	SocketUtils::rel( );
}