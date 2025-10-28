#include "pch.hpp"
//#include "IocpCore.hpp"
#include "Listener.hpp"

//IocpCore gIocpCore;

int main( )
{
	SocketUtils::init( );

	auto listener = std::make_shared<Listener>( );
	listener->startAccept( NetAddress( "127.0.0.1", 7777 ) );

	std::thread t1( worker );
	t1.join( );

	SocketUtils::rel( );
}