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
		WSAOVERLAPPED over;
		IoType type;
		ServerSession* owner;
		SendBuffer* sendBuffer;	// Send일 때만 유효

		void clear() {
			ZeroMemory(&over, sizeof(WSAOVERLAPPED));
		}
	};

public:
	ServerSession() : sock_(SocketUtils::createSocket()), netAddr_(serverIp, serverPort),
		connected_(false), recvBuf_(0x10000)/*64KB*/, game_(nullptr)
	{
		ZeroMemory(&recvOver_.over, sizeof(WSAOVERLAPPED));
		recvOver_.type = IoType::Recv;
		recvOver_.owner = this;
		recvOver_.sendBuffer = nullptr;

		ZeroMemory(&sendOver_.over, sizeof(WSAOVERLAPPED));
		sendOver_.type = IoType::Send;
		sendOver_.owner = this;
		sendOver_.sendBuffer = nullptr;
	}

	~ServerSession() {
		SocketUtils::closeSocket(sock_);
	}

	bool connect();
	void send(SendBuffer* sendBuffer);

	void setGame(Online::Game* game) { game_ = game; }

	const std::string& ip() const { return netAddr_.ip(); }
	uint16 port() const { return netAddr_.port(); }
	bool isConnected() const { return connected_.load(); }

private:
	void registerRecv();
	void registerSend(SendBuffer* sendBuffer);

	void processRecv(int32 numBytes);
	void processPacket(byte* buffer, int32 len);
	void processSend(int32 numBytes);

	static void CALLBACK completionCallback(DWORD errr, DWORD numBytes, LPWSAOVERLAPPED overlapped, DWORD flags);

private:
	SOCKET sock_;
	NetAddress netAddr_;
	std::atomic_bool connected_;

	OverlappedEx recvOver_;
	OverlappedEx sendOver_;

	RecvBuffer recvBuf_;

	Online::Game* game_;
};

#endif // server_session_hpp