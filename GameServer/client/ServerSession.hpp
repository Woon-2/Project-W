#ifndef server_session_hpp
#define server_session_hpp

#include "RecvBuffer.hpp"

class SendBuffer;

namespace Online { class Game; }

class ServerSession {
	enum class IoType {
		Recv,
		Send
	};

	struct OverlappedEx {
		WSAOVERLAPPED over{};
		IoType type{};
		ServerSession* owner{nullptr};
		std::vector<std::shared_ptr<SendBuffer>> sendBuffers{};	// Send일 때만 유효

		void clear() {
			ZeroMemory(&over, sizeof(WSAOVERLAPPED));
		}
	};

public:
	ServerSession(const std::string& ip, uint16 port) : sock_(SocketUtils::createSocket()), netAddr_(ip, port),
		connected_(false),recvBuf_(0x10000)/*64KB*/, sending_(false), pendingSendBuffers_(), game_(nullptr)
	{
		ZeroMemory(&recvOver_.over, sizeof(WSAOVERLAPPED));
		recvOver_.type = IoType::Recv;
		recvOver_.owner = this;

		ZeroMemory(&sendOver_.over, sizeof(WSAOVERLAPPED));
		sendOver_.type = IoType::Send;
		sendOver_.owner = this;
	}

	~ServerSession() {
		pendingSendBuffers_.clear();
		sendOver_.sendBuffers.clear();
		SocketUtils::closeSocket(sock_);
	}

	bool connect();
	// 로비→룸 핸드오프 시 옛 세션을 은퇴시킨다. 소켓을 닫아 잔여 recv/send APC가 곧 완료(error)되게 한다.
	// closeSocket(SOCKET&)이 INVALID 가드 + sock_=INVALID 리셋을 내장하므로 소멸자의 재닫기도 안전(no-op).
	void close() {
		connected_ = false;
		SocketUtils::closeSocket(sock_);
	}
	void addSendBuffer(const std::shared_ptr<SendBuffer>& sendBuffer) { pendingSendBuffers_.push_back(sendBuffer); }
	void send();

	void setGame(Online::Game* game) { game_ = game; }

	const std::string& ip() const { return netAddr_.ip(); }
	uint16 port() const { return netAddr_.port(); }

private:
	void registerRecv();
	void registerSend();

	void processRecv(int32 numBytes);
	void processPacket(byte* buffer, int32 len);
	void processSend(int32 numBytes);

	static void CALLBACK completionCallback(DWORD errr, DWORD numBytes, LPWSAOVERLAPPED overlapped, DWORD flags);

private:
	SOCKET sock_;
	NetAddress netAddr_;
	bool connected_;

	OverlappedEx recvOver_;
	OverlappedEx sendOver_;

	RecvBuffer recvBuf_;

	bool sending_;
	std::vector<std::shared_ptr<SendBuffer>> pendingSendBuffers_;

	Online::Game* game_;
};

#endif // server_session_hpp