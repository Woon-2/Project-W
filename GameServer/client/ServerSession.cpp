#include "pch.hpp"
#include "ServerSession.hpp"
#include "SendBuffer.hpp"
#include "MemoryManager.hpp"
#include "PacketManager.hpp"

bool ServerSession::connect() {
	if (::connect(sock_, reinterpret_cast<const SOCKADDR*>(&netAddr_.sockAddr()), sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
		return false;
	}
	else {
		SocketUtils::setTcpNoDelay(sock_, true);

		connected_.store(true);
		registerRecv();
		return true;
	}
}

void ServerSession::send(SendBuffer* sendBuffer) {
	registerSend(sendBuffer);
}

void ServerSession::registerRecv() {
	if (!isConnected()) {
		return;
	}

	recvOver_.clear();

	auto wsaBuf = WSABUF{
		.len = static_cast<ULONG>(recvBuf_.freeSize()),
		.buf = reinterpret_cast<char*>(recvBuf_.writePos())
	};

	DWORD numBytes{};
	DWORD flags{};
	if (WSARecv(sock_, &wsaBuf, 1, &numBytes, &flags, &recvOver_.over, &completionCallback) == SOCKET_ERROR) {
		auto error = WSAGetLastError();
		if (error != WSA_IO_PENDING) {
			std::cout << "WSARecv failed with error: " << error << '\n';
			connected_.store(false);
		}
	}
}

void ServerSession::registerSend(SendBuffer* sendBuffer) {
	if (!isConnected()) {
		return;
	}

	sendOver_.clear();
	sendOver_.sendBuffer = sendBuffer;	// Send 완료 후 해제할 SendBuffer 설정

	auto wsaBuf = WSABUF{
		.len = static_cast<ULONG>(sendBuffer->writeSize()),
		.buf = reinterpret_cast<char*>(sendBuffer->data())
	};

	DWORD numBytes{};
	if (WSASend(sock_, &wsaBuf, 1, &numBytes, 0, &sendOver_.over, &completionCallback) == SOCKET_ERROR) {
		auto error = WSAGetLastError();
		if (error != WSA_IO_PENDING) {
			std::cout << "WSASend failed with error: " << error << '\n';
			odelete(sendBuffer);
			connected_.store(false);
		}
	}
}

void ServerSession::processRecv(int32 numBytes) {
	if (numBytes == 0) {
		std::cout << "Recv 0\n";
		connected_.store(false);
		return;
	}
	if(!recvBuf_.moveWritePos(numBytes)) {
		std::cout << "Failed to move write position in receive buffer.\n";
		connected_.store(false);
		return;
	}

	int32 dataSize = recvBuf_.dataSize();
	int32 recvLen{};
	byte* buffer = recvBuf_.readPos();

	while (true) {
		int32 currDataSize = dataSize - recvLen;

		if (currDataSize < sizeof(PacketHeader)) {
			break;
		}

		auto header = reinterpret_cast<PacketHeader*>(buffer + recvLen);
		if (currDataSize < header->size) {
			break;
		}

		processPacket(buffer + recvLen, header->size);
		recvLen += header->size;
	}
}

void ServerSession::processPacket(byte* buffer, int32 len) {
	PacketManager::handlePacket(buffer, len);
}

void ServerSession::processSend(int32 numBytes) {
	if (numBytes > 0) {
		odelete(sendOver_.sendBuffer);
	}
	else {
		std::cout << "Connection closed by the server during send.\n";
		connected_.store(false);
	}
}

void CALLBACK ServerSession::completionCallback(DWORD error, DWORD numBytes, LPWSAOVERLAPPED overlapped, DWORD flags) {
	auto overEx = reinterpret_cast<OverlappedEx*>(overlapped);
	auto session = overEx->owner;

	if (error != 0) {
		std::cout << "I/O operation failed with error: " << error << '\n';
		session->connected_.store(false);
		return;
	}

	switch (overEx->type) {
	case IoType::Recv:
		session->processRecv(numBytes);
		break;

	case IoType::Send:
		session->processSend(numBytes);
		break;

	default:
		std::cout << "Unknown I/O operation completed.\n";
		break;
	}
}
