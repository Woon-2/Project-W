#ifndef __OVERLAPPED_EX_HPP
#define __OVERLAPPED_EX_HPP

#include "net/netInclude.hpp"
#include "net/protocol.hpp"

#include <utility>
#include <deque>
#include <vector>

enum class IO_OP : std::uint8_t {
	IO_ACCEPT,
	IO_RECV,
	IO_SEND,
};

class OverlappedEx {
public:
	OverlappedEx( IO_OP type )
		: over_{ }, type_{ type }, acceptSocket_{ INVALID_SOCKET },
		sendQueue_( ), wsaBufs_( ) {}

	OverlappedEx( IO_OP type, std::deque<Packet>&& sendQueue ) 
		: over_{ }, type_{ type }, acceptSocket_{ INVALID_SOCKET }, 
		sendQueue_( std::move( sendQueue ) ), wsaBufs_( ) {
		sendQueue.clear( );
		wsaBufs_.resize( sendQueue_.size( ) );

		for ( std::size_t i = 0u; i < sendQueue_.size( ); ++i ) {
			wsaBufs_[ i ].len = sendQueue_[ i ].size;
			wsaBufs_[ i ].buf = reinterpret_cast<char*>( &sendQueue_[ i ] );
		}
	}

	WSAOVERLAPPED over_;
	IO_OP type_;
	SOCKET acceptSocket_;
	std::deque<Packet> sendQueue_;
	std::vector<WSABUF> wsaBufs_;
};

#endif	// __OVERLAPPED_EX_HPP