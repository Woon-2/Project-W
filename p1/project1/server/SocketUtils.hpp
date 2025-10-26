#ifndef SOCKET_UTILS_HPP
#define SOCKET_UTILS_HPP

#include "pch.hpp"

class NetAddress;

class SocketUtils {
public:
	static void init( );
	static void rel( );

	static SOCKET createSocket( );
	static void closeSocket( SOCKET& sock );

	static void setReuseAddr( SOCKET sock, bool reuse );
	static void setTcpNoDelay( SOCKET sock, bool noDelay );
	static void setUpdateAcceptContext( SOCKET sock, SOCKET listenSock );

	static void bind( SOCKET sock, const NetAddress& address );
	static void bindAnyAddr( SOCKET sock, uint16 port );
	static void listen( SOCKET sock, int backlog = SOMAXCONN );

private:
	template<class T>
	static bool setSockOpt( SOCKET sock, int32 level, int32 optName, const T& optVal ) {
		return ::setsockopt( sock, level, optName,
			reinterpret_cast<const char*>( &optVal ), sizeof( optVal ) ) != SOCKET_ERROR;
	}
};

#endif // SOCKET_UTILS_HPP