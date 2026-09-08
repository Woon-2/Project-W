#ifndef session_hpp
#define session_hpp

#include "IocpDispatchable.hpp"
#include "IoEvent.hpp"
#include "RecvBuffer.hpp"

/*---------------
     Session
---------------*/

class Session : public IocpDispatchable {
public:
	Session()
		: sock_(SocketUtils::createSocket()), netAddr_(), connected_(false), disconnEv_(),
		recvEv_(), sendEv_(), recvBuf_(0x10000)/*64KB*/, sendQueue_(), sending_(false), id_(-1) {}

	virtual ~Session() {
		SocketUtils::closeSocket(sock_);
	}

	virtual void dispatch(IoEvent* event, int32 numBytes) override;
	void disconnect(std::string_view cause);
	void send(const std::shared_ptr<SendBuffer>& sendBuffer);

	void setNetAddress(const NetAddress& addr) { netAddr_ = addr; }
	const NetAddress& netAddress() const { return netAddr_; }
	void setId(int32 id) { id_ = id; }
	int32 id() const { return id_; }
	virtual HANDLE getHandle() const { return reinterpret_cast<HANDLE>(sock_); }
	virtual SOCKET getSocket() const { return sock_; }

	bool isConnected() { return connected_.load(); }

private:
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
	// 컨텐츠 쪽에서 오버라이드해서 사용
	virtual void onDisconnected() {}
	// 컨텐츠 쪽에서 오버라이드해서 사용
	virtual int32 onRecv(byte* buffer, int32 len) { return len; }
	// 컨텐츠 쪽에서 오버라이드해서 사용
	virtual void onSend(int32 len) {}

private:
	SOCKET sock_;
	NetAddress netAddr_;
	std::atomic_bool connected_;

	DisconnectEvent disconnEv_;
	RecvEvent recvEv_;
	SendEvent sendEv_;

	RecvBuffer recvBuf_;
	ccqueue<std::shared_ptr<SendBuffer>> sendQueue_;
	std::atomic_bool sending_;

	int32 id_;

	friend class Listener;
};

/*---------------------
	 PacketSession
---------------------*/

class PacketSession : public Session {
public:
	PacketSession() = default;
	virtual ~PacketSession() = default;

	virtual int32 onRecv(byte* buffer, int32 len) override sealed;
	virtual void processPacket(byte* buffer, int32 len) = 0;
};

#endif // session_hpp