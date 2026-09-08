#ifndef listener_hpp
#define listener_hpp

#include "IocpDispatchable.hpp"

class AcceptEvent;

class Listener : public IocpDispatchable {
public:
	Listener( HANDLE iocpHandle, uint16 listenPort )
		: listenSock_( SocketUtils::createSocket() ), acceptEvs_(),
		  iocpHandle_( iocpHandle ), listenPort_( listenPort ) {}

	virtual ~Listener() {
		SocketUtils::closeSocket( listenSock_ );
		for ( auto ev : acceptEvs_ ) {
			ObjectPool<AcceptEvent>::push( ev );
		}
	}

	void startAccept();
	virtual void dispatch( IoEvent* event, int32 numBytes ) override;

	virtual HANDLE getHandle() const override { return reinterpret_cast<HANDLE>(listenSock_); }
	virtual SOCKET getSocket() const override { return listenSock_; }
	uint16 listenPort() const { return listenPort_; }

private:
	void registerAccept( AcceptEvent* acceptEv );
	void processAccept( AcceptEvent* acceptEv );

private:
	SOCKET listenSock_;
	std::vector<AcceptEvent*> acceptEvs_;
	HANDLE iocpHandle_;
	uint16 listenPort_;
};

#endif // listener_hpp
