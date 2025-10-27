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

private:
	WSAOVERLAPPED over_;
	IoType type_;
};


#endif // IO_EVENT_HPP