#ifndef NET_ADDRESS_HPP
#define NET_ADDRESS_HPP

#include "windows.hpp"
#include "types.hpp"
#include <string>

class NetAddress {
public:
	NetAddress( ) : sockAddr_( ) {}
	NetAddress( uint16 port ) : sockAddr_( ) {
		sockAddr_.sin_family = AF_INET;
		sockAddr_.sin_port = ::htons( port );
		sockAddr_.sin_addr.s_addr = ::htonl( INADDR_ANY );
	}
	NetAddress( const std::string& ip, uint16 port ) : sockAddr_( ) {
		sockAddr_.sin_family = AF_INET;
		sockAddr_.sin_port = ::htons( port );
		::inet_pton( AF_INET, ip.c_str( ), &sockAddr_.sin_addr );
	}
	NetAddress( const SOCKADDR_IN& sockAddr ) : sockAddr_( sockAddr ) {}

	const SOCKADDR_IN& getSockAddr( ) const {
		return sockAddr_;
	}

	void getIp( std::string& ip ) const {
		::inet_ntop( AF_INET, &sockAddr_.sin_addr, ip.data( ), ip.size( ) );
	}

	uint16 getPort( ) const {
		return ::ntohs( sockAddr_.sin_port );
	}

private:
	SOCKADDR_IN sockAddr_;
};

#endif // NET_ADDRESS_HPP