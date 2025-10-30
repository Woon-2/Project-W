#ifndef LISTENER_HPP
#define LISTENER_HPP

#include "IoEvent.hpp"

class Listener : public IocpObject {
public:
	Listener( ) : listenSock_( SocketUtils::createSocket( ) ),
		acceptEvents_( ), service_( ) {}

	virtual ~Listener( ) {
		SocketUtils::closeSocket( listenSock_ );

		for( auto event : acceptEvents_ ) {
			delete event;
		}
	}

	void startAccept( const SPServerService& service );
	virtual void dispatch( IoEvent* event, int32 numBytes ) override;
	void registerAccept( AcceptEvent* event );
	void processAccept( AcceptEvent* event );

	virtual HANDLE getHandle( ) const override {
		return reinterpret_cast<HANDLE>( listenSock_ );
	}

	virtual SOCKET getSock( ) const override {
		return listenSock_;
	}
	
private:
	SOCKET listenSock_;
	std::vector<AcceptEvent*> acceptEvents_;
	SPServerService service_;
};

#endif // LISTENER_HPP