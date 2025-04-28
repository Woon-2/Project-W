#ifndef __OVERLAPPED_EX_HPP
#define __OVERLAPPED_EX_HPP

#include "netInclude.hpp"
#include "protocol.hpp"

enum class IO_OP : std::uint8_t {
	IO_ACCEPT,
	IO_RECV,
	IO_SEND,
};

class OverlappedEx {
public:
	OverlappedEx( IO_OP type ) 
		: over_{ }, type_{ type }, 
		acceptSocket_{ INVALID_SOCKET }, buffer_( ) {
		wsaBuf_.len = bufferSize;
		wsaBuf_.buf = buffer_.data( );
	}

	WSAOVERLAPPED over_;
	IO_OP type_;
	SOCKET acceptSocket_;
	std::array<char, bufferSize> buffer_;
	WSABUF wsaBuf_;
};

#endif	// __OVERLAPPED_EX_HPP