#ifndef LISTENER_HPP
#define LISTENER_HPP

#include "IoEvent.hpp"

class Listener : public IocpObject {
public:
	Listener( ) : listenSock_( SocketUtils::createSocket( ) ), acceptEvents_( ) {}

	virtual ~Listener( ) {
		SocketUtils::closeSocket( listenSock_ );

		for( auto event : acceptEvents_ ) {
			delete event;
		}
	}

	void startAccept( const NetAddress& netAddr );
	virtual void dispatch( IoEvent* event, int32 numBytes ) override;
	void registerAccept( IoEvent* event );
	void processAccept( IoEvent* event );

	virtual HANDLE getHandle( ) const override {
		return reinterpret_cast<HANDLE>( listenSock_ );
	}

	virtual SOCKET getSock( ) const override {
		return listenSock_;
	}
	
private:
	SOCKET listenSock_;
	std::vector<IoEvent*> acceptEvents_;
};

void worker( );

#endif // LISTENER_HPP