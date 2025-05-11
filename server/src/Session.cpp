#include "Session.hpp"
#include "game/level.hpp"

extern ccQueue <u16t> gIdPool;

void errorDisplay( std::string_view where, int error ) {
	std::cerr << where << " failed : "
		<< std::system_category( ).message( error ) << '\n';
}

// Session ==========================================================================
Session::Session()
	: clientSocket_{ INVALID_SOCKET }, id_( -1 ), entityId_( -1u ), recvOver_( IO_OP::IO_RECV ),
	recvBytesRemain_{ 0u }, sendQueue_( ), recvBuffer_( ), completedAccept{ false } {}

Session::~Session() {
	close();
}

Session::Session( Session&& rhs ) noexcept
	: clientSocket_{ rhs.clientSocket_.load( ) },
	id_{ std::exchange( rhs.id_, -1 ) }, entityId_( std::exchange( rhs.entityId_, -1u ) ),
	recvOver_( std::move( rhs.recvOver_ ) ), 
	recvBuffer_( std::move( rhs.recvBuffer_ ) ), recvBytesRemain_{ std::exchange( rhs.recvBytesRemain_, 0 ) }, 
	sendQueue_( std::move( rhs.sendQueue_ ) ) {

	rhs.clientSocket_.store( INVALID_SOCKET );

	rhs.recvOver_.pSendQueue_.reset();
	rhs.recvOver_.wsaBufs_.clear( );
	rhs.recvOver_.wsaBufs_.shrink_to_fit( );

	rhs.recvBuffer_.clear( );
	rhs.recvBuffer_.shrink_to_fit( );

	rhs.sendQueue_.clear( );
	rhs.sendQueue_.shrink_to_fit( );
}

Session& Session::operator=( Session&& other ) noexcept {
	if ( this == &other ) {
		return *this;
	}

	clientSocket_ = other.clientSocket_.load();
	other.clientSocket_.store( INVALID_SOCKET );
	id_ = std::exchange( other.id_, -1 );

	recvOver_ = std::move( other.recvOver_ );
	other.recvOver_.pSendQueue_.reset();
	other.recvOver_.wsaBufs_.clear( );
	other.recvOver_.wsaBufs_.shrink_to_fit();

	recvBuffer_ = std::move( other.recvBuffer_ );
	other.recvBuffer_.clear( );
	other.recvBuffer_.shrink_to_fit();

	recvBytesRemain_ = std::exchange( other.recvBytesRemain_, 0 );

	sendQueue_ = std::move( other.sendQueue_ );
	other.sendQueue_.clear( );
	other.sendQueue_.shrink_to_fit();

	return *this;
}

Session& Session::init(SOCKET socket, i16t sessionId) {
	clientSocket_ = socket;
	id_ = sessionId;
	recvOver_ = OverlappedEx{ IO_OP::IO_RECV };
	recvOver_.wsaBufs_.resize(1u);
	recvBuffer_.resize(recvBufSize);
	recvBytesRemain_ = 0u;
	sendQueue_.clear();
	sendQueue_.shrink_to_fit();

	return *this;
}

bool Session::close() {
	auto oldSocket = clientSocket_.load();
	if ( oldSocket == INVALID_SOCKET ) {
		return false;
	}

	if ( clientSocket_.compare_exchange_strong( oldSocket, INVALID_SOCKET ) ) {
		completedAccept.store(false);

		::closesocket( oldSocket );

		recvOver_.pSendQueue_.reset();
		recvOver_.wsaBufs_.clear( );
		recvOver_.wsaBufs_.shrink_to_fit( );

		recvBuffer_.clear( );
		recvBuffer_.shrink_to_fit( );

		sendQueue_.clear();
		sendQueue_.shrink_to_fit();

		return true;
	}

	return false;
}

void Session::doRecv( ) {
	DWORD recvFlag{ };
	::ZeroMemory( &recvOver_.over_, sizeof( recvOver_.over_ ) );
	auto& wsaBuf = recvOver_.wsaBufs_[ 0 ];

	wsaBuf.len = recvBufSize - recvBytesRemain_;
	wsaBuf.buf = recvBuffer_.data( ) + recvBytesRemain_;

	auto ret = ::WSARecv( clientSocket_, &wsaBuf, 1, 
		nullptr, &recvFlag, &recvOver_.over_, nullptr );
	if ( ret == SOCKET_ERROR ) {
		auto err = WSAGetLastError( );
		if ( err != WSA_IO_PENDING && err != WSAECONNRESET ) {
			errorDisplay( "WSARecv", WSAGetLastError( ) );

			auto eId = getEntityId( );
			auto packet = Packet{
				.size = calcPacketSize<SCLeave>( ),
				.type = PacketType::SCLeave,
				.scLeave = SCLeave{
					.leaveCnt = 1u,
					.leavedIds = { eId }
				}
			};

			Session::enqueueBroadcastPacket( packet );

			if ( close( ) ) {
				gIdPool.push( eId );
			}
		}
	}
}

void Session::doSend( ) {
	if (sendQueue_.empty()) {
		return;
	}

	auto overEx = new OverlappedEx{ IO_OP::IO_SEND,
		std::make_shared<decltype(sendQueue_)>( std::move(sendQueue_) )
	};

	auto ret = ::WSASend( clientSocket_, overEx->wsaBufs_.data( ), 
		static_cast<DWORD>( overEx->wsaBufs_.size( ) ), nullptr, 0, &overEx->over_, nullptr );
	if ( ret == SOCKET_ERROR ) {
		auto err = WSAGetLastError( );
		if ( err != WSA_IO_PENDING && err != WSAECONNRESET ) {
			errorDisplay( "WSASend", WSAGetLastError( ) );

			auto eId = getEntityId( );
			auto packet = Packet{
				.size = calcPacketSize<SCLeave>( ),
				.type = PacketType::SCLeave,
				.scLeave = SCLeave{
					.leaveCnt = 1u,
					.leavedIds = { eId }
				}
			};

			Session::enqueueBroadcastPacket( packet );

			if ( close( ) ) {
				gIdPool.push( eId );
			}
		}
	}
}

void Session::doBroadcast( pmr::vector<Session*> sessions ) {
	auto pBroadcastQ = std::make_shared<decltype(sendQueue_)>( );
	auto& broadcastQ = *pBroadcastQ;

	Packet packet{ };
	while ( sBroadcastQueue_.try_pop( packet ) ) {
		broadcastQ.push_back( packet );
	}

	if (broadcastQ.empty()) {
		return;
	}

	for ( auto& pSession : sessions ) {
		if (pSession->accessReady()) {
			auto overEx = new OverlappedEx{ IO_OP::IO_SEND, pBroadcastQ };

			auto ret = ::WSASend( pSession->clientSocket_, overEx->wsaBufs_.data( ), 
				static_cast<DWORD>( overEx->wsaBufs_.size( ) ), nullptr, 0, &overEx->over_, nullptr );
			if ( ret == SOCKET_ERROR ){
				auto err = WSAGetLastError( );
				if( err != WSA_IO_PENDING && err != WSAECONNRESET ) {
					errorDisplay( "WSASend", WSAGetLastError( ) );

					auto eId = pSession->getEntityId( );
					auto packet = Packet{
						.size = calcPacketSize<SCLeave>( ),
						.type = PacketType::SCLeave,
						.scLeave = SCLeave{
							.leaveCnt = 1u,
							.leavedIds = { eId }
						}
					};

					Session::enqueueBroadcastPacket( packet );

					if ( pSession->close( ) ) {
						gIdPool.push( eId );
					}
				}
			}
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
		(*packetProcessor_)( packet, *this );
		readOffset += packetSize;
		recvBytesRemain_ -= packetSize;
	}
	
	if ( readOffset > 0 && recvBytesRemain_ > 0 ) {
		std::memcpy( recvBuffer_.data( ), recvBuffer_.data( ) + readOffset, recvBytesRemain_ );
	}
}

ccQueue<Packet> Session::sBroadcastQueue_;
// ==================================================================================
