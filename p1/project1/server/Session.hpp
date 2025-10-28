#ifndef SESSION_HPP
#define SESSION_HPP

#include "IocpObject.hpp"

class Session : public IocpObject {
public:
	Session( ) : sock_( SocketUtils::createSocket( ) ),
		netAddr_( ), connected_( false ), recvBuf_( ) {}

	virtual ~Session( ) {
		SocketUtils::closeSocket( sock_ );
	}

	virtual void dispatch( IoEvent* event, int32 numBytes ) override;

	void setNetAddress( const NetAddress& netAddr ) {
		netAddr_ = netAddr;
	}

	virtual HANDLE getHandle( ) const override {
		return reinterpret_cast<HANDLE>( sock_ );
	}

	virtual SOCKET getSock( ) const override {
		return sock_;
	}

	const NetAddress& getNetAddress( ) const {
		return netAddr_;
	}

private:
	SOCKET sock_;
	NetAddress netAddr_;
	std::atomic_bool connected_;

public:
	std::array<int8, bufSize> recvBuf_;
};

#endif // SESSION_HPP