#include "pch.hpp"
#include "Session.hpp"
#include "Service.hpp"

void Session::disconnect( const std::string& cause ) {
	if ( connected_.exchange( false ) == false ) {
		return;
	}

	std::cout << "Disconnected: " << cause << '\n';	// temporary

	onDisconnected( );	// used by content side
	SocketUtils::closeSocket( sock_ );
	getService( )->releaseSession( getSPSession( ) );
}

void Session::dispatch( IoEvent* event, int32 numBytes ) {
	switch ( event->getType( ) ) {
	case IoType::Connect:
		processConnect( );
		break;

	case IoType::Recv:
		processRecv( numBytes );
		break;

	default:
		break;
	}
}

void Session::registerConnect( )
{
}

void Session::registerRecv( ) {
	if( !isConnected( ) ) {
		return;
	}

	recvEvent_.clear( );
	recvEvent_.setOwner( shared_from_this( ) );	// add reference

	auto wsaBuf = WSABUF{
		.len = static_cast<ULONG>( recvBuf_.size( ) ),
		.buf = recvBuf_.data( )
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

void Session::processRecv( int32 numBytes ) {
	recvEvent_.setOwner( nullptr );	// release reference

	if ( numBytes == 0 ) {
		disconnect( "Recv 0" );
		return;
	}

	onRecv( recvBuf_.data( ), numBytes );	// used by content side
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
