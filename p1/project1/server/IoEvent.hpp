#ifndef IO_EVENT_HPP
#define IO_EVENT_HPP

#include "pch.hpp"

/*---------------
     IoEvent
---------------*/

enum class IoType : uint8 {
	None,
	Connect,
	Disconnect,
	Accept,
	Recv,
	Send
};

class IoEvent {
public:
	IoEvent( ) : over_( ), type_( IoType::None ), owner_( ) {}
	IoEvent( IoType type ) : over_( ), type_( type ), owner_( ) {}

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

private:
	WSAOVERLAPPED over_;
	IoType type_;
	SPIocpObject owner_;
};

/*--------------------
	 ConnectEvent
--------------------*/

class ConnectEvent : public IoEvent {
public:
	ConnectEvent( ) : IoEvent( IoType::Connect ) {}
};

/*-----------------------
	 DisconnectEvent
-----------------------*/

class DisconnectEvent : public IoEvent {
public:
	DisconnectEvent( ) : IoEvent( IoType::Disconnect ) {}
};

/*-------------------
	 AcceptEvent
-------------------*/

class AcceptEvent : public IoEvent {
public:
	AcceptEvent( ) : IoEvent( IoType::Accept ), session_( ) {}

	void setSession( const SPSession& session ) {
		session_ = session;
	}

	SPSession getSession( ) const {
		return session_;
	}

private:
	SPSession session_;
};

/*-----------------
	 RecvEvent
-----------------*/

class RecvEvent : public IoEvent {
public:
	RecvEvent( ) : IoEvent( IoType::Recv ) {}
};

/*-----------------
	 SendEvent
-----------------*/

class SendEvent : public IoEvent {
public:
	SendEvent( ) : IoEvent( IoType::Send ) {}
};

#endif // IO_EVENT_HPP