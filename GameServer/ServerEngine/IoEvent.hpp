#ifndef io_event_hpp
#define io_event_hpp

#include "simpleWindows.hpp"
#include <vector>
#include "types.hpp"

enum class IoType : uint8 {
	none,
	disconnect,
	accept,
	recv,
	send
};

/*---------------
     IoEvent
---------------*/

class IocpDispatchable;

class IoEvent {
public:
	IoEvent() : over_(), type_(IoType::none), owner_(nullptr) {}
	IoEvent(IoType type) : over_(), type_(type), owner_(nullptr) {}

	void clear() {
		ZeroMemory(&over_, sizeof(WSAOVERLAPPED));
	}

	void setType(IoType type) { type_ = type; }
	IoType type() const { return type_; }
	void setOwner(IocpDispatchable* owner) { owner_ = owner; }
	IocpDispatchable* owner() const { return owner_; }

private:
	WSAOVERLAPPED over_;
	IoType type_;
	IocpDispatchable* owner_;
};

/*-----------------------
	 DisconnectEvent
-----------------------*/

class DisconnectEvent : public IoEvent {
public:
	DisconnectEvent() : IoEvent(IoType::disconnect) {}
};

/*-------------------
	 AcceptEvent
-------------------*/

class Session;

class AcceptEvent : public IoEvent {
public:
	AcceptEvent() : IoEvent(IoType::accept), session_(nullptr) {}

	void setSession(Session* session) { session_ = session; }
	Session* session() { return session_; }

private:
	Session* session_;
};

/*-----------------
	 RecvEvent
-----------------*/

class RecvEvent : public IoEvent {
public:
	RecvEvent() : IoEvent(IoType::recv) {}
};

/*-----------------
	 SendEvent
-----------------*/

class SendBuffer;

class SendEvent : public IoEvent {
public:
	SendEvent() : IoEvent(IoType::send), sendBuffers_() {}

	std::vector<SendBuffer*> sendBuffers_;
};

#endif // io_event_hpp