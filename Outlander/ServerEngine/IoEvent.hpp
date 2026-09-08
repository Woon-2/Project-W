#ifndef io_event_hpp
#define io_event_hpp

#include "simpleWindows.hpp"
#include <vector>
#include <memory>
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
	// owner_는 진행 중인 I/O가 완료될 때까지 대상 객체(주로 Session)를 살려두는 소유 참조다.
	// register 시 setOwner(shared_from_this())로 잡고, 완료 처리 후 setOwner(nullptr)로 놓는다.
	void setOwner(const std::shared_ptr<IocpDispatchable>& owner) { owner_ = owner; }
	std::shared_ptr<IocpDispatchable> owner() const { return owner_; }

private:
	WSAOVERLAPPED over_;
	IoType type_;
	std::shared_ptr<IocpDispatchable> owner_;
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

	void setSession(const std::shared_ptr<Session>& session) { session_ = session; }
	const std::shared_ptr<Session>& session() { return session_; }

private:
	std::shared_ptr<Session> session_;
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

	std::vector<std::shared_ptr<SendBuffer>> sendBuffers_;
};

#endif // io_event_hpp