#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "mswsock.lib" )

#include <iostream>

int main( )
{
	WSAData wsaData;
	if ( ::WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 )
		return 0;

	SOCKET clientSocket = ::socket( AF_INET, SOCK_STREAM, 0 );
	if ( clientSocket == INVALID_SOCKET )
		return 0;

	SOCKADDR_IN serverAddr;
	::memset( &serverAddr, 0, sizeof( serverAddr ) );
	serverAddr.sin_family = AF_INET;
	::inet_pton( AF_INET, "127.0.0.1", &serverAddr.sin_addr );
	serverAddr.sin_port = ::htons( 7777 );

	int check = 0;
	if ( ::connect( clientSocket,
		reinterpret_cast<SOCKADDR*>( &serverAddr ),
		sizeof( serverAddr ) ) != SOCKET_ERROR ) {
		check = 1;
	}

	while ( true ) {
		if ( check == 1 ) {
			break;
		}
	}

	std::cout << "Connected to Server!" << std::endl;

	while ( true );

	::closesocket( clientSocket );
	::WSACleanup( );
}