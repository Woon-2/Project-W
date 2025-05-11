#ifndef __OVERLAPPED_EX_HPP
#define __OVERLAPPED_EX_HPP

#include "stdafx.hpp"

#include "net/netInclude.hpp"
#include "net/protocol.hpp"

enum class IO_OP : std::uint8_t {
	IO_ACCEPT,
	IO_RECV,
	IO_SEND,
};

class OverlappedEx {
public:
	OverlappedEx( IO_OP type )
		: over_{ }, type_{ type }, acceptSocket_{ INVALID_SOCKET },
		pSendQueue_( nullptr ), wsaBufs_( ) {}

	OverlappedEx( IO_OP type, std::shared_ptr< std::deque<Packet> > pSendQueue ) 
		: over_{ }, type_{ type }, acceptSocket_{ INVALID_SOCKET }, 
		pSendQueue_( std::move( pSendQueue ) ), wsaBufs_( ) {
		auto& sendQueue = *pSendQueue_;

		wsaBufs_.resize( sendQueue.size( ) );

		for ( std::size_t i = 0u; i < sendQueue.size( ); ++i ) {
			wsaBufs_[ i ].len = sendQueue[ i ].size;
			wsaBufs_[ i ].buf = reinterpret_cast<char*>( &sendQueue[ i ] );
		}
	}

	WSAOVERLAPPED over_;
	IO_OP type_;
	SOCKET acceptSocket_;
	std::shared_ptr< std::deque<Packet> > pSendQueue_;
	std::vector<WSABUF> wsaBufs_;
};

#endif	// __OVERLAPPED_EX_HPP