#include "pch.hpp"
#include "Session.hpp"
#include "Service.hpp"

void Session::disconnect( const std::string& cause ) {
	if ( connected_.exchange( false ) == false ) {
		return;
	}

	std::cout << "Disconnected: " << cause << '\n';	// temporary

	onDisconnected( );	// 컨텐츠 쪽에서 사용
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

void Session::registerSend( )
{
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

void Session::processSend( int32 numBytes )
{
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
