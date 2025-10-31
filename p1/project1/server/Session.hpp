#ifndef SESSION_HPP
#define SESSION_HPP

#include "IoEvent.hpp"
#include "IocpObject.hpp"

class Service;
class Session : public IocpObject {
public:
	Session( ) : service_( ), sock_( SocketUtils::createSocket( ) ),
		netAddr_( ), connected_( false ), connectEvent_( ),
		disconnectEvent_( ), recvEvent_( ), recvBuf_( ) {}

	virtual ~Session( ) {
		SocketUtils::closeSocket( sock_ );
	}

	bool connect( ) {
		return registerConnect( );
	}
	void disconnect( const std::string& cause );

	// Setters and Getters
	void setNetAddress( const NetAddress& netAddr ) {
		netAddr_ = netAddr;
	}

	const NetAddress& getNetAddress( ) const {
		return netAddr_;
	}

	void setService( const std::shared_ptr<Service>& service ) {
		service_ = service;
	}

	std::shared_ptr<Service> getService( ) const {
		return service_.lock( );
	}

	SPSession getSPSession( ) {
		return std::static_pointer_cast<Session>( shared_from_this( ) );
	}

	// Connection State
	bool isConnected( ) const {
		return connected_.load( );
	}

private:
	// Interface overrides
	virtual void dispatch( IoEvent* event, int32 numBytes ) override;

	virtual HANDLE getHandle( ) const override {
		return reinterpret_cast<HANDLE>( sock_ );
	}

	virtual SOCKET getSock( ) const override {
		return sock_;
	}

	// For transmission events
	bool registerConnect( );
	void registerDisconnect( );
	void registerRecv( );
	void registerSend( );

	void processConnect( );
	void processDisconnect( );
	void processRecv( int32 numBytes );
	void processSend( int32 numBytes );

	void handleError( int32 errCode );

protected:
	// Will be overridden by content side
	virtual void onConnected( ) {}
	virtual void onDisconnected( ) {}
	virtual int32 onRecv( char* buffer, int32 len ) {
		return len;
	}
	virtual void onSend( int32 len ) {}

private:
	std::weak_ptr<Service> service_;
	SOCKET sock_;
	NetAddress netAddr_;
	std::atomic_bool connected_;

	ConnectEvent connectEvent_;
	DisconnectEvent disconnectEvent_;
	RecvEvent recvEvent_;

public:
	std::array<char, bufSize> recvBuf_;

	friend class Listener;
	friend class IocpCore;
	friend class Service;
};

#endif // SESSION_HPP