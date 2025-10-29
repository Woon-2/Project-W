#ifndef IO_EVENT_HPP
#define IO_EVENT_HPP

#include "pch.hpp"
#include "IocpObject.hpp"
#include "Session.hpp"

enum class IoType : int8 {
	None,
	Accept,
	Recv,
	Send
};

class IoEvent {
public:
	IoEvent( ) : over_( ), type_( IoType::None ), owner_( ), session_( ) {}
	IoEvent( IoType type ) : over_( ), type_( type ), owner_( ), session_( ) {}

	void clear( ) {
		::ZeroMemory( &over_, sizeof( WSAOVERLAPPED ) );
	}

	void setType( IoType type ) {
		type_ = type;
	}

	IoType getType( ) const {
		return type_;
	}

	void setOwner( const SPIocpObject& iocpObject ) {
		owner_ = iocpObject;
	}

	SPIocpObject getOwner( ) const {
		return owner_;
	}

	void setSession( const SPSession& session ) {
		session_ = session;
	}

	SPSession getSession( ) const {
		return session_;
	}

private:
	WSAOVERLAPPED over_;
	IoType type_;
	SPIocpObject owner_;
	SPSession session_;
};


#endif // IO_EVENT_HPP