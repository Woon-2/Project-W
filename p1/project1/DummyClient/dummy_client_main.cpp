#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

#include "../server/protocol.hpp"
#include <iostream>
#include <system_error>
#include <string>
#include <cstdint>

int main( )
{
	WSADATA wsaData;
	if ( ::WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) {
		std::cout << "WSAStartup failed.\n";
		return -1;
	}

	auto clientSock = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( clientSock == INVALID_SOCKET ) {
		std::cout << "WSASocket failed.\n";
		::WSACleanup( );
		return -1;
	}

	auto serverAddr = SOCKADDR_IN{
		.sin_family = AF_INET,
		.sin_port = ::htons( 7777 )
	};
	::inet_pton( AF_INET, "127.0.0.1", &serverAddr.sin_addr );

	if( ::connect( clientSock, reinterpret_cast<SOCKADDR*>( &serverAddr ), sizeof( serverAddr ) ) == SOCKET_ERROR ) {
		std::cout << "Connect failed with error: " 
			<< std::system_category( ).message( ::WSAGetLastError( ) ) << "\n";

		::closesocket( clientSock );
		::WSACleanup( );
		return -1;
	}

	while ( true ) {
		int num{ };
		std::cout << "회원가입(0) / 로그인(1) : ";
		std::cin >> num;

		std::string id, pw;
		std::cout << "ID: "; std::cin >> id;
		std::cout << "PW: "; std::cin >> pw;

		if ( id.size( ) > 20 || pw.size( ) > 20 ) {
			std::cout << "ID와 PW는 최대 20자까지 가능합니다.\n";
			continue;
		}

		Packet packet{ };
		if ( num == 0 ) {
			packet.header.size = sizeof( PacketHeader ) + sizeof( CSSignupPacket );
			packet.header.id = static_cast<std::uint16_t>( PacketType::csSignup );
			::memcpy( packet.csSignup.id.data( ), id.c_str( ), id.size( ) );
			::memcpy( packet.csSignup.pw.data( ), pw.c_str( ), pw.size( ) );
		}
		else if ( num == 1 ) {
			packet.header.size = sizeof( PacketHeader ) + sizeof( CSLoginPacket );
			packet.header.id = static_cast<std::uint16_t>( PacketType::csLogin );
			::memcpy( packet.csLogin.id.data( ), id.c_str( ), id.size( ) );
			::memcpy( packet.csLogin.pw.data( ), pw.c_str( ), pw.size( ) );
		}

		std::int32_t packetSize = sizeof( Packet );
		if ( ::send( clientSock, reinterpret_cast<const char*>( &packet ), packetSize, 0 ) == SOCKET_ERROR ) {
			std::cout << "Send failed with error: " 
				<< std::system_category( ).message( ::WSAGetLastError( ) ) << "\n";
			break;
		}

		Packet recvPacket{ };
		std::cout << "recvPacket size: " << sizeof( recvPacket ) << "\n";
		auto recvLen = ::recv( clientSock, reinterpret_cast<char*>( &recvPacket ), sizeof( recvPacket ), 0 );
		std::cout << "recvLen: " << recvLen << "\n";
		/*if( ::recv( clientSock, reinterpret_cast<char*>( &recvPacket ), sizeof( packetSize ), 0 ) <= 0 ) {
			std::cout << "Recv failed with error: " 
				<< std::system_category( ).message( ::WSAGetLastError( ) ) << "\n";
			break;
		}*/

		if ( num == 0 ) {
			if ( recvPacket.scSignup.isOk ) {
				std::cout << "회원가입 성공!\n";
			}
			else {
				std::cout << "회원가입 실패: " << recvPacket.scSignup.reason.data( ) << "\n";
			}
		}
		else if ( num == 1 ) {
			if( recvPacket.scLogin.isOk ) {
				std::cout << "로그인 성공!\n";
			}
			else {
				std::cout << "로그인 실패: " << recvPacket.scLogin.reason.data( ) << "\n";
			}
		}
	}




	::closesocket( clientSock );
	::WSACleanup( );
}