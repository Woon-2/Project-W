#include "pch.hpp"
#include "IocpCore.hpp"
#include "Listener.hpp"
#include "Session.hpp"

void worker( ) {
	while ( true ) {
		gIocpCore.dispatch( );
	}
}

void Listener::startAccept( const NetAddress& netAddr ) {
	SocketUtils::setReuseAddr( listenSock_, true );
	SocketUtils::bindAnyAddr( listenSock_, serverPort );
	SocketUtils::listen( listenSock_ );

	gIocpCore.registerHandle( this );

	const int32 acceptCount = 1;
	for( auto i = 0; i < acceptCount; ++i ) {
		IoEvent* event = new IoEvent( IoType::Accept );
		event->setOwner( shared_from_this( ) );
		acceptEvents_.push_back( event );
		registerAccept( event );
	}
}

void Listener::dispatch( IoEvent* event, int32 numBytes ) {
	auto ioType = event->getType( );

	switch ( ioType ) {
	case IoType::Accept:
		processAccept( event );
		break;

	default:
		break;
	}
}

void Listener::registerAccept( IoEvent* event ) {
	auto session = std::make_shared<Session>( );

	event->clear( );
	event->setSession( session );

	gIocpCore.registerHandle( session.get( ) );

	if ( !::AcceptEx( listenSock_, session->getSock( ), session->recvBuf_.data( ),
		0, sizeof( SOCKADDR_IN ) + 16, sizeof( SOCKADDR_IN ) + 16,
		nullptr, reinterpret_cast<OVERLAPPED*>( event ) )
	) {
		auto errCode = ::WSAGetLastError( );
		if ( errCode != ERROR_IO_PENDING ) {
			registerAccept( event );
		}
	}
}

void Listener::processAccept( IoEvent* event ) {
	auto session = event->getSession( );
	SocketUtils::setUpdateAcceptContext( session->getSock( ), listenSock_ );

	SOCKADDR_IN sockAddr{ };
	int32 len = sizeof( SOCKADDR_IN );
	if ( ::getpeername( session->getSock( ),
		reinterpret_cast<SOCKADDR*>( &sockAddr ), &len ) == SOCKET_ERROR
	) {
		registerAccept( event );
		return;
	}

	session->setNetAddress( NetAddress( sockAddr ) );

	std::cout << "Client connected!\n";

	// TODO

	registerAccept( event );
}
