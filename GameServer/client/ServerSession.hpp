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
		std::vector<SendBuffer*> sendBuffers{};	// Send일 때만 유효

		void clear() {
			ZeroMemory(&over, sizeof(WSAOVERLAPPED));
		}
	};

public:
	ServerSession() : sock_(SocketUtils::createSocket()), netAddr_(serverIp, serverPort),
		connected_(false),recvBuf_(0x10000)/*64KB*/, sending_(false), pendingSendBuffers_(), game_(nullptr)
	{
		ZeroMemory(&recvOver_.over, sizeof(WSAOVERLAPPED));
		recvOver_.type = IoType::Recv;
		recvOver_.owner = this;

		ZeroMemory(&sendOver_.over, sizeof(WSAOVERLAPPED));
		sendOver_.type = IoType::Send;
		sendOver_.owner = this;
	}

	~ServerSession();

	bool connect();
	void addSendBuffer(SendBuffer* sendBuffer) { pendingSendBuffers_.push_back(sendBuffer); }
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
	std::vector<SendBuffer*> pendingSendBuffers_;

	Online::Game* game_;
};

#endif // server_session_hpp