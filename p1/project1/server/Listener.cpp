#include "pch.hpp"
#include "IocpCore.hpp"
#include "Listener.hpp"
#include "Session.hpp"
#include "Service.hpp"

void Listener::startAccept( const SPServerService& service ) {
	service_ = service;
	ASSERT_CRASH( service_ != nullptr );

	ASSERT_CRASH( service_->getIocpCore( )->registerHandle( shared_from_this( ) ) );

	SocketUtils::setReuseAddr( listenSock_, true );
	SocketUtils::bind( listenSock_, service_->getNetAddress( ) );
	SocketUtils::listen( listenSock_ );

	const auto acceptCount = service_->getMaxSessionCount( );
	for( auto i = 0; i < acceptCount; ++i ) {
		auto event = new AcceptEvent( );
		event->setOwner( shared_from_this( ) );
		acceptEvents_.push_back( event );
		registerAccept( event );
	}
}

void Listener::dispatch( IoEvent* event, int32 numBytes ) {
	ASSERT_CRASH( event->getType( ) == IoType::Accept );
	auto acceptEvent = static_cast<AcceptEvent*>( event );
	processAccept( acceptEvent );
}

void Listener::registerAccept( AcceptEvent* event ) {
	auto session = service_->createSession( );

	event->clear( );
	event->setSession( session );

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

void Listener::processAccept( AcceptEvent* event ) {
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
