#ifndef IO_EVENT_HPP
#define IO_EVENT_HPP

#include "pch.hpp"

enum class IoType : byte {
	None,
	Accept,
	Recv,
	Send
};

class IoEvent {
public:
	IoEvent( ) : over_( ), type_( IoType::None ) {}
	IoEvent( IoType type ) : over_( ), type_( type ) {}

	void clear( ) {
		::ZeroMemory( &over_, sizeof( WSAOVERLAPPED ) );
		type_ = IoType::None;
	}

	IoType getType( ) const {
		return type_;
	}

	void setType( IoType type ) {
		type_ = type;
	}

private:
	WSAOVERLAPPED over_;
	IoType type_;
};


#endif // IO_EVENT_HPP