#include "Session.hpp"
#include "sNetEx.hpp"

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
	id_{ std::exchange( rhs.id_, -1 ) }, recvOver_( std::move( rhs.recvOver_ ) ), 
	recvBuffer_( std::move( rhs.recvBuffer_ ) ), recvBytesRemain_{ std::exchange( rhs.recvBytesRemain_, 0 ) }, 
	sendQueue_( std::move( rhs.sendQueue_ ) ) {
	rhs.sendQueue_.clear( );
}

Session& Session::operator=( Session&& other ) noexcept {
	if ( this == &other ) {
		return *this;
	}

	clientSocket_ = std::exchange( other.clientSocket_, INVALID_SOCKET );
	id_ = std::exchange( other.id_, -1 );
	recvOver_ = std::move( other.recvOver_ );
	recvBuffer_ = std::move( other.recvBuffer_ );
	recvBytesRemain_ = std::exchange( other.recvBytesRemain_, 0 );
	sendQueue_ = std::move( other.sendQueue_ );
	other.sendQueue_.clear( );

	return *this;
}

void Session::doRecv( ) {
	DWORD recvFlag{ };
	::ZeroMemory( &recvOver_.over_, sizeof( recvOver_.over_ ) );
	auto& wsaBuf = recvOver_.wsaBufs_[ 0 ];

	wsaBuf.len = bufferSize - recvBytesRemain_;
	wsaBuf.buf = recvBuffer_.data( ) + recvBytesRemain_;

	auto ret = ::WSARecv( clientSocket_, &wsaBuf, 1, 
		nullptr, &recvFlag, &recvOver_.over_, nullptr );
	if ( ret == SOCKET_ERROR ) {
		if ( WSAGetLastError( ) != WSA_IO_PENDING ) {
			errorDisplay( "WSARecv", WSAGetLastError( ) );
		}
	}
}

void Session::doSend( ) {
	auto overEx = new OverlappedEx{ IO_OP::IO_SEND, std::move(sendQueue_) };

	auto ret = ::WSASend( clientSocket_, overEx->wsaBufs_.data( ), 
		static_cast<DWORD>( overEx->wsaBufs_.size( ) ), nullptr, 0, &overEx->over_, nullptr );
	if ( ret == SOCKET_ERROR ) {
		if ( WSAGetLastError( ) != WSA_IO_PENDING ) {
			errorDisplay( "WSASend", WSAGetLastError( ) );
		}
	}
}

void Session::interpretData( DWORD bytesTransferred ) {
	// readOffset_ -> 읽어야 할 데이터 시작 위치
	// recvBytesRemain_ + bytesTransferred -> 지금까지 받은 데이터 크기
	
	std::uint16_t readOffset{ };
	recvBytesRemain_ += static_cast<std::uint16_t>( bytesTransferred );

	while ( recvBytesRemain_ >= sizeof( std::uint16_t ) ) {
		auto packetSize = reinterpret_cast<Packet*>( recvBuffer_.data( ) + readOffset )->size;

		if ( recvBytesRemain_ < packetSize ) {
			break;
		}

		auto packet = *reinterpret_cast<Packet*>( recvBuffer_.data( ) + readOffset );
		processPacket( packet );
		readOffset += packetSize;
		recvBytesRemain_ -= packetSize;
	}
	
	if ( readOffset > 0 && recvBytesRemain_ > 0 ) {
		std::memcpy( recvBuffer_.data( ), recvBuffer_.data( ) + readOffset, recvBytesRemain_ );
	}
}

void Session::setNetSystem( SNetExSystem* netSystem ) {
	netSystem_ = netSystem;
}

void Session::processPacket( const Packet& packet ) {
	switch ( packet.type ) {
	case PacketType::CS_Login: {
		
		break;
	}

	case PacketType::CS_World:
		netSystem_->getNetEx( packet.scWorld.netId )->processPacket( packet );
		break;
	}
}
// ==================================================================================
