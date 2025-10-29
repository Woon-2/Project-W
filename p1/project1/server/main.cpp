#include "pch.hpp"
#include "Service.hpp"
#include "IocpCore.hpp"
#include "Session.hpp"

int main( )
{
	SocketUtils::init( );

	auto service = std::make_shared<ServerService>(
		NetAddress( serverIp, serverPort ),
		std::make_shared<IocpCore>( ), nullptr, 100
	);

	// temporary
	service->setSessionFactory( []( ) {
		return std::make_shared<Session>( );
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