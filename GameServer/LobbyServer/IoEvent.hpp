#ifndef io_event_hpp
#define io_event_hpp

/*-------------------
       IoEvent
-------------------*/

enum class IoType : uint8 {
    none,
    disconnect,
    accept,
    recv,
    send
};

class IocpObject;
class IoEvent {
public:
    IoEvent() : over_(), type_(IoType::none), owner_() {}
    IoEvent(IoType type) : over_(), type_(type), owner_() {}

    void clear() {
        ::ZeroMemory(&over_, sizeof(WSAOVERLAPPED));
    }

    void setIoType(IoType type) { type_ = type; }
    IoType type() const { return type_; }
    void setOwner(IocpObject* iocpObject) { owner_ = iocpObject; }
    IocpObject* owner() const { return owner_; }

private:
    WSAOVERLAPPED over_;
	IoType type_;
	IocpObject* owner_;
};

/*-------------------------
      DisconnectEvent
-------------------------*/

class DisconnectEvent : public IoEvent {
public:
    DisconnectEvent() : IoEvent(IoType::disconnect) {}
};

/*---------------------
	  AcceptEvent
---------------------*/

class Session;
class AcceptEvent : public IoEvent {
public:
	AcceptEvent() : IoEvent(IoType::accept), session_(nullptr) {}

    void setSession(Session* session) { session_ = session; }
	Session* session() const { return session_; }
    
private:
    Session* session_;
};

/*-------------------
	  RecvEvent
-------------------*/

class RecvEvent : public IoEvent {
public:
    RecvEvent() : IoEvent(IoType::recv) {}
};

/*-------------------
	  SendEvent
-------------------*/

class SendBuffer;
class SendEvent : public IoEvent {
public:
    SendEvent() : IoEvent(IoType::send), sendBuffers_() {}

    std::vector<SendBuffer*> sendBuffers_;
};

#endif	// io_event_hpp