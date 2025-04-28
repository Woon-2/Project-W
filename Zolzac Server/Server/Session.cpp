#include "Session.hpp"

#include <utility>
#include <ranges>
#include <numeric>

// IDPool ==========================================
std::forward_list<std::uint16_t> IDPool::idList_;

void IDPool::initList( ) {
	idList_.resize( 0xffff );
	std::ranges::iota( idList_, 0u );
}

std::optional<std::int16_t> IDPool::allocID( ) {
	if ( idList_.empty( ) ) {
		return std::nullopt;
	}

	auto id = idList_.front( );
	idList_.pop_front( );

	return id;
}

void IDPool::deallocID( std::int16_t id ) {
	idList_.push_front( id );
}
//==================================================

// Session ==========================================================================
Session::Session( Session&& rhs ) noexcept
	: clientSocket_{ std::exchange( rhs.clientSocket_, INVALID_SOCKET ) },
	id_{ std::exchange( rhs.id_, -1 ) }, recvOver_{ std::move( rhs.recvOver_ ) },
	recvBytesRemain_{ std::exchange( rhs.recvBytesRemain_, 0 ) } {
}

Session& Session::operator=( Session&& other ) noexcept {
	if ( this == &other ) {
		return *this;
	}

	clientSocket_ = std::exchange( other.clientSocket_, INVALID_SOCKET );
	id_ = std::exchange( other.id_, -1 );
	recvOver_ = std::move( other.recvOver_ );
	recvBytesRemain_ = std::exchange( other.recvBytesRemain_, 0 );

	return *this;
}

void Session::doRecv( ) {
	DWORD recvFlag{ };
	::ZeroMemory( &recvOver_.over_, sizeof( recvOver_.over_ ) );
	recvOver_.wsaBuf_.len = bufferSize - recvBytesRemain_;
	recvOver_.wsaBuf_.buf = recvOver_.buffer_.data( ) + recvBytesRemain_;

	auto ret = ::WSARecv( clientSocket_, &recvOver_.wsaBuf_, 1, 
		nullptr, &recvFlag, &recvOver_.over_, nullptr );
	if ( ret == SOCKET_ERROR ) {
		if ( WSAGetLastError( ) != WSA_IO_PENDING ) {
			errorDisplay( "WSARecv", WSAGetLastError( ) );
		}
	}
}

void Session::interpretData( DWORD bytesTransferred ) {
	// readOffset_ -> 읽어야 할 데이터 시작 위치
	// recvBytesRemain_ + bytesTransferred -> 지금까지 받은 데이터 크기
	
	std::uint16_t readOffset{ };
	recvBytesRemain_ += static_cast<std::uint16_t>( bytesTransferred );

	while ( recvBytesRemain_ >= sizeof( std::uint16_t ) ) {
		auto packetSize = *reinterpret_cast<std::uint16_t*>( recvOver_.buffer_.data( ) + readOffset );

		if ( recvBytesRemain_ < packetSize ) {
			break;
		}

		processPacket( recvOver_.buffer_.data( ) + readOffset );
		readOffset += packetSize;
		recvBytesRemain_ -= packetSize;
	}
	
	if ( readOffset > 0 && recvBytesRemain_ > 0 ) {
		std::memcpy( recvOver_.buffer_.data( ), recvOver_.buffer_.data( ) + readOffset, recvBytesRemain_ );
	}
}

void Session::processPacket( const char* ) {
	// 패킷에 들어있는 NetId로 매핑된 NetEx를 찾는다.
	// 그 NetEx에다 대고 processPacket을 호출한다.
	// 그러면 그 NetEx가 갖고있는 NetExProcessor가 자신의 정의에 맞게 알아서 잘 처리해줄 것이다.
}
// ==================================================================================
