#include "pch.hpp"
#include "Session.hpp"
#include "Service.hpp"
#include "SendBuffer.hpp"

void Session::send( const SPSendBuffer& sendBuffer ) {
	if ( !isConnected( ) ) {
		return;
	}

	bool canSend{ false };
	{
		std::lock_guard<std::mutex> lock( sendMtx_ );
		sendQueue_.push( sendBuffer );
		if ( sending_.exchange( true ) == false ) {
			canSend = true;
		}
	}

	if ( canSend ) {
		registerSend( );
	}
}

void Session::disconnect( const std::string& cause ) {
	if ( connected_.exchange( false ) == false ) {
		return;
	}
	
	std::cout << "Disconnected: " << cause << '\n';	// temporary
	registerDisconnect( );
}

void Session::dispatch( IoEvent* event, int32 numBytes ) {
	switch ( event->getType( ) ) {
	case IoType::Connect:
		processConnect( );
		break;

	case IoType::Disconnect:
		processDisconnect( );
		break;

	case IoType::Recv:
		processRecv( numBytes );
		break;

	case IoType::Send:
		processSend( numBytes );
		break;

	default:
		break;
	}
}

bool Session::registerConnect( ) {
	if ( isConnected( ) ) {
		return false;
	}
	if ( getService( )->getType( ) != ServiceType::Client ) {
		return false;
	}

	SocketUtils::setReuseAddr( sock_, true );
	SocketUtils::bindAnyAddr( sock_, 0 );	// 0번 포트를 사용하면, 운영체제가 빈 포트를 자동으로 할당

	connectEvent_.clear( );
	connectEvent_.setOwner( shared_from_this( ) );	// add reference

	DWORD numBytes{ };
	auto sockAddr = getService( )->getNetAddress( ).getSockAddr( );
	if ( false == SocketUtils::ConnectEx( sock_, reinterpret_cast<SOCKADDR*>( &sockAddr ),
		sizeof( sockAddr ), nullptr, 0, &numBytes, reinterpret_cast<WSAOVERLAPPED*>( &connectEvent_ ) )
	) {
		auto errCode = ::WSAGetLastError( );
		if ( errCode != WSA_IO_PENDING ) {
			connectEvent_.setOwner( nullptr );	// release reference
			return false;
		}
	}

	return true;
}

void Session::registerDisconnect( ) {
	disconnectEvent_.clear( );
	disconnectEvent_.setOwner( shared_from_this( ) );	// add reference

	if ( false == SocketUtils::DisconnectEx( sock_,
		reinterpret_cast<WSAOVERLAPPED*>( &disconnectEvent_ ), TF_REUSE_SOCKET, 0 )
	) {
		auto errCode = ::WSAGetLastError( );
		if ( errCode != ERROR_IO_PENDING ) {
			handleError( errCode );
			disconnectEvent_.setOwner( nullptr );	// release reference
		}
	}
}

void Session::registerRecv( ) {
	if( !isConnected( ) ) {
		return;
	}

	recvEvent_.clear( );
	recvEvent_.setOwner( shared_from_this( ) );	// add reference

	auto wsaBuf = WSABUF{
		.len = static_cast<ULONG>( recvBuf_.freeSize( ) ),
		.buf = reinterpret_cast<char*>( recvBuf_.writePos( ) )
	};

	DWORD numBytes{ };
	DWORD flags{ };
	if ( ::WSARecv( sock_, &wsaBuf, 1, &numBytes, &flags,
		reinterpret_cast<WSAOVERLAPPED*>( &recvEvent_ ), nullptr ) == SOCKET_ERROR
	) {
		auto errCode = ::WSAGetLastError( );
		if ( errCode != ERROR_IO_PENDING ) {
			handleError( errCode );
			recvEvent_.setOwner( nullptr );	// release reference
		}
	}
}

void Session::registerSend( ) {
	if ( !isConnected( ) ) {
		return;
	}

	sendEvent_.clear( );
	sendEvent_.setOwner( shared_from_this( ) );	// add reference

	{
		//std::lock_guard<std::mutex> lock( sendMtx_ );

		int32 writeSize{ };
		while ( !sendQueue_.empty( ) ) {
			auto sendBuffer = sendQueue_.front( );

			writeSize += sendBuffer->writeSize( );	// 나중에 send size 제한에 사용할 수도 있음

			sendQueue_.pop( );
			sendEvent_.sendBuffers_.push_back( sendBuffer );
		}
	}

	std::vector<WSABUF> wsaBufs;
	wsaBufs.reserve( sendEvent_.sendBuffers_.size( ) );
	for ( const auto& sendBuffer : sendEvent_.sendBuffers_ ) {
		wsaBufs.push_back( WSABUF{
			.len = static_cast<ULONG>( sendBuffer->writeSize( ) ),
			.buf = reinterpret_cast<char*>( sendBuffer->data( ) )
		} );
	}

	DWORD numBytes{ };
	if ( ::WSASend( sock_, wsaBufs.data( ), static_cast<DWORD>( wsaBufs.size( ) ),
		&numBytes, 0, reinterpret_cast<WSAOVERLAPPED*>( &sendEvent_ ), nullptr ) == SOCKET_ERROR
	) {
		auto errCode = ::WSAGetLastError( );
		if ( errCode != ERROR_IO_PENDING ) {
			handleError( errCode );
			sendEvent_.setOwner( nullptr );	// release reference
			sendEvent_.sendBuffers_.clear( );
			sending_.store( false );
		}
	}
}

void Session::processConnect( ) {
	connectEvent_.setOwner( nullptr );	// release reference
	connected_.store( true );

	getService( )->addSession( getSPSession( ) );
	onConnected( );	// used by content side
	registerRecv( );
}

void Session::processDisconnect( ) {
	disconnectEvent_.setOwner( nullptr );	// release reference
	onDisconnected( );	// 컨텐츠 쪽에서 사용
	getService( )->releaseSession( getSPSession( ) );
}

void Session::processRecv( int32 numBytes ) {
	recvEvent_.setOwner( nullptr );	// release reference

	if ( numBytes == 0 ) {
		disconnect( "Recv 0" );
		return;
	}

	if ( recvBuf_.moveWritePos( numBytes ) == false ) {
		disconnect( "RecvBuffer Move WritePos Failed" );
		return;
	}

	auto dataSize = recvBuf_.dataSize( );
	auto readBytes = onRecv( recvBuf_.readPos( ), dataSize );	// 컨텐츠 쪽에서 사용

	if( readBytes < 0 || readBytes > dataSize || recvBuf_.moveReadPos( readBytes ) == false ) {
		disconnect( "RecvBuffer Move ReadPos Failed" );
		return;
	}

	// readPos, writePos 정리
	recvBuf_.clean( );

	registerRecv( );
}

void Session::processSend( int32 numBytes ) {
	sendEvent_.setOwner( nullptr );	// release reference
	sendEvent_.sendBuffers_.clear( );

	if ( numBytes == 0 ) {
		disconnect( "Send 0" );
		return;
	}

	// 컨텐츠 쪽에서 사용
	onSend( numBytes );

	std::lock_guard<std::mutex> lock( sendMtx_ );
	if ( sendQueue_.empty( ) ) {
		sending_.store( false );
	}
	else {
		registerSend( );
	}
}

void Session::handleError( int32 errCode ) {
	switch ( errCode ) {
	case WSAECONNRESET:
	case WSAECONNABORTED:
		disconnect( "Handle Error" );
		break;

	default:
		// TODO: log
		std::cout << std::system_category( ).message( errCode ) << '\n';	// temporary
		break;
	}
}

/*---------------------
	 PacketSession
---------------------*/

int32 PacketSession::onRecv( uint8* buffer, int32 len ) {
	int32 recvLen{ };

	while ( true ) {
		int32 dataSize = len - recvLen;

		// 헤더 크기보다 데이터 크기가 작으면 중단
		if ( dataSize < static_cast<int32>( sizeof( PacketHeader ) ) ) {
			break;
		}

		auto header = reinterpret_cast<PacketHeader*>( &buffer[ recvLen ] );
		// 패킷 전체 크기보다 데이터 크기가 작으면 중단
		if ( dataSize < header->size ) {
			break;
		}

		// 패킷 처리
		onRecvPacket( &buffer[ recvLen ], header->size );
		recvLen += sizeof( Packet );
	}
	return recvLen;
}
