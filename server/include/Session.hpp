#ifndef __SESSION_HPP
#define __SESSION_HPP

#include "net/netInclude.hpp"
#include "OverlappedEx.hpp"

#include <optional>
#include <forward_list>
#include <set>
#include <deque>
#include <atomic>
#include <array>
#include <string_view>
#include <iostream>

void errorDisplay( std::string_view where, int error );

class SNetExSystem;
// Session ============================================================
class Session {
public:
    static constexpr std::size_t recvBufSize = 40960u;

	Session( ) : recvOver_( IO_OP::IO_RECV ) {
		std::cout << "Session default constructor called\n";
		exit( -1 );
	}

	~Session( ) {
		::closesocket( clientSocket_ );
	}

	Session( SOCKET socket, std::int16_t id )
		: clientSocket_{ socket }, id_{ id }, recvOver_( IO_OP::IO_RECV ),
		recvBytesRemain_{ 0 }, sendQueue_( ), recvBuffer_( ) {
		recvOver_.wsaBufs_.resize( 1 );
		doRecv( );
	}

	Session( const Session& ) = delete;
	Session& operator=( const Session& ) = delete;

	Session( Session&& ) noexcept;
	Session& operator=( Session&& ) noexcept;

	void doRecv( );
    void doSend( );
	void interpretData( DWORD bytesTransferred );

    void setNetSystem( SNetExSystem* netSystem );
    void enqueuePacket( const Packet& packet ) {
		sendQueue_.push_back( packet );
    }

    bool getAcceptFlag( ) const {
		return completedAccept.load( );
    }
    void setAcceptFlag( ) {
        completedAccept.store( true );
    }

private:
	SOCKET clientSocket_;
	std::int16_t id_;

    OverlappedEx recvOver_;
    std::array<char, recvBufSize> recvBuffer_;
	std::uint16_t recvBytesRemain_;

    std::deque<Packet> sendQueue_;

    SNetExSystem* netSystem_ = nullptr;
	std::atomic_bool completedAccept{ false };

	void processPacket( const Packet& packet );
};
//=====================================================================

#endif	// __SESSION_HPP