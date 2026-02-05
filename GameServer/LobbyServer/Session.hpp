#ifndef session_hpp
#define session_hpp

#include "IocpObject.hpp"
#include "IoEvent.hpp"
#include "RecvBuffer.hpp"
#include "SendBuffer.hpp"

/*---------------
     Session
---------------*/

class Session : public IocpObject {
public:
	Session()
		: sock_(SocketUtils::createSocket()), netAddr_(), connected_(false),
		disconnEv_(), recvEv_(), sendEv_(), recvBuf_(0x10000)/*64KB*/,
		sendQueue_(), sending_(false), id_(-1) {}

	virtual ~Session() {
		SocketUtils::closeSocket(sock_);
	}

	void disconnect(std::string_view cause);
	void send(SendBuffer* sendBuffer);

	// SOCKET 타입을 HANDLE 타입으로 변환하여 반환하는 멤버 함수
	virtual HANDLE getHandle() const { return reinterpret_cast<HANDLE>(sock_); }
	virtual SOCKET getSocket() const { return sock_; }

	void setNetAddress(const NetAddress& netAddr) { netAddr_ = netAddr; }
	const NetAddress& netAddress() const { return netAddr_; }
	void setId(int32 id) { id_ = id; }
	int32 id() const { return id_; }

	bool isConnected() const { return connected_.load(); }

private:
	virtual void dispatch(IoEvent* event, int32 numBytes) override;

	void registerDisconnect();
	void registerRecv();
	void registerSend();

	void processConnect();
	void processDisconnect();
	void processRecv(int32 numBytes);
	void processSend(int32 numBytes);

	void handleError(std::string_view where, int32 errCode);

protected:
	// 컨텐츠 쪽에서 오버라이드해서 사용
	virtual void onConnected() {}
	virtual void onDisconnected() {}
	virtual int32 onRecv(uint8* buffer, int32 len) { return len; }
	virtual void onSend(int32 len) {}

private:
    SOCKET sock_;
    NetAddress netAddr_;
	std::atomic_bool connected_;

	DisconnectEvent disconnEv_;
	RecvEvent recvEv_;
	SendEvent sendEv_;

	RecvBuffer recvBuf_;
	ccqueue<SendBuffer*> sendQueue_;
	std::atomic_bool sending_;

	int32 id_;

	friend class Listener;
};

/*---------------------
	 PacketSession
---------------------*/

//class PacketSession : public Session {
//public:
//
//}

#endif	// session_hpp