#ifndef SESSION_HPP
#define SESSION_HPP

#include "IoEvent.hpp"
#include "IocpObject.hpp"
#include "RecvBuffer.hpp"

/*---------------
     Session
---------------*/

class Service;
class Session : public IocpObject {
	enum {
		bufferSize = 0x10000	// 64KB
	};

public:
	Session( ) : service_( ), sock_( SocketUtils::createSocket( ) ), netAddr_( ),
		connected_( false ), connectEvent_( ), disconnectEvent_( ),	sendEvent_( ),
		recvEvent_( ), recvBuf_( bufferSize ), sendQueue_( ), sendMtx_( ), sending_( false ) {}

	virtual ~Session( ) {
		SocketUtils::closeSocket( sock_ );
	}

	// 외부에서 사용
	void send( const SPSendBuffer& sendBuffer );

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

	// 연결 상태 확인
	bool isConnected( ) const {
		return connected_.load( );
	}

private:
	// 인터페이스 구현
	virtual void dispatch( IoEvent* event, int32 numBytes ) override;

	virtual HANDLE getHandle( ) const override {
		return reinterpret_cast<HANDLE>( sock_ );
	}

	virtual SOCKET getSock( ) const override {
		return sock_;
	}

	// 통신 관련 함수
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
	// 컨텐츠 쪽에서 오버라이드해서 사용
	virtual void onConnected( ) {}
	virtual void onDisconnected( ) {}
	virtual int32 onRecv( uint8* buffer, int32 len ) {
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
	SendEvent sendEvent_;

	RecvBuffer recvBuf_;
	std::queue<SPSendBuffer> sendQueue_;
	std::mutex sendMtx_;	// temporary
	std::atomic_bool sending_;

	friend class Listener;
	friend class IocpCore;
	friend class Service;
};

/*---------------------
	 PacketSession
---------------------*/

class PacketSession : public Session {
public:
	PacketSession( ) {}
	virtual ~PacketSession( ) {}

	SPPacketSession getSPPacketSession( ) {
		return std::static_pointer_cast<PacketSession>( shared_from_this( ) );
	}

protected:
	virtual int32 onRecv( uint8* buffer, int32 len ) sealed;
	virtual int32 onRecvPacket( uint8* buffer, int32 len ) = 0;
};

#endif // SESSION_HPP