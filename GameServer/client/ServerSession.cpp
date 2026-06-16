#include "pch.hpp"
#include "ServerSession.hpp"
#include "SendBuffer.hpp"
#include "MemoryManager.hpp"
#include "PacketManager.hpp"

extern bool gClose;

bool ServerSession::connect() {
	if (::connect(sock_, reinterpret_cast<const SOCKADDR*>(&netAddr_.sockAddr()), sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
		return false;
	}
	else {
		SocketUtils::setTcpNoDelay(sock_, true);

		connected_ = true;
		registerRecv();
		return true;
	}
}

void ServerSession::send() {
	if (pendingSendBuffers_.empty()) {
		return;
	}
	if (sending_) {
		return;
	}

	registerSend();
}

void ServerSession::registerRecv() {
	if (!connected_) {
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
			connected_ = false;
			return;
		}
	}
	// 성공(즉시 완료) 또는 WSA_IO_PENDING — 어느 경우든 completion routine APC가 큐잉되므로 카운트.
	++pendingIo_;
}

void ServerSession::registerSend() {
	if (!connected_) {
		return;
	}
	
	sending_ = true;
	sendOver_.clear();

	// Send 완료 후 해제할 SendBuffer 설정
	sendOver_.sendBuffers = std::move(pendingSendBuffers_);

	auto wsaBufs = std::vector<WSABUF>();
	wsaBufs.reserve(sendOver_.sendBuffers.size());

	for(auto sendBuf : sendOver_.sendBuffers) {
		 wsaBufs.emplace_back(WSABUF{
			.len = static_cast<ULONG>(sendBuf->writeSize()),
			.buf = reinterpret_cast<char*>(sendBuf->data())
		});
	}

	DWORD numBytes{};
	if (WSASend(sock_, wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()) , &numBytes, 0, &sendOver_.over, &completionCallback) == SOCKET_ERROR) {
		auto error = WSAGetLastError();
		if (error != WSA_IO_PENDING) {
			std::cout << "WSASend failed with error: " << error << '\n';

			sendOver_.sendBuffers.clear();
			sending_ = false;
			connected_ = false;
			return;
		}
	}
	// 성공/PENDING — completion routine APC가 큐잉되므로 미완료 I/O로 카운트.
	++pendingIo_;
}

void ServerSession::processRecv(int32 numBytes) {
	if (numBytes == 0) {
		std::cout << "Recv 0\n";
		connected_ = false;
		return;
	}
	if(!recvBuf_.moveWritePos(numBytes)) {
		std::cout << "Failed to move write position in receive buffer.\n";
		connected_ = false;
		return;
	}

	int32 dataSize = recvBuf_.dataSize();
	int32 recvLen{};
	byte* buffer = recvBuf_.readPos();

	while (true) {
		int32 currDataSize = dataSize - recvLen;
		//std::cout << "curr data size: " << currDataSize << '\n';

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

	if(recvLen < 0 || recvLen > dataSize || !recvBuf_.moveReadPos(recvLen)) {
		std::cout << "Failed to move read position in receive buffer.\n";
		connected_ = false;
		return;
	}

	recvBuf_.clean();
	registerRecv();
}

void ServerSession::processPacket(byte* buffer, int32 len) {
	//std::cout << "Received packet - Size: " << len << '\n';
	PacketManager::handlePacket(buffer, len);
}

void ServerSession::processSend(int32 numBytes) {
	sendOver_.sendBuffers.clear();
	sending_ = false;

	if(numBytes == 0) {
		std::cout << "Connection closed by the server during send.\n";
		connected_ = false;
	}
}

void CALLBACK ServerSession::completionCallback(DWORD error, DWORD numBytes, LPWSAOVERLAPPED overlapped, DWORD flags) {
	auto overEx = reinterpret_cast<OverlappedEx*>(overlapped);
	auto session = overEx->owner;

	// 이 완료 1건(취소 완료 포함)을 소비하므로 미완료 카운트를 먼저 감소시킨다 — closeAndDrain의
	// 드레인 루프가 끝나려면 gClose 여부와 무관하게 항상 감소해야 한다.
	--session->pendingIo_;

	if (gClose) {
		return;   // 종료 중: 추가 winsock/세션 상태 변경 금지(카운트만 정리)
	}

	if (error != 0) {
		std::cout << "I/O operation failed with error: " << error << '\n';

		if(overEx->type == IoType::Send) {
			overEx->sendBuffers.clear();
			session->sending_ = false;
		}

		session->connected_ = false;
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

void ServerSession::closeAndDrain() {
	close();   // closesocket → pending recv/send 취소 유발

	// 취소된 overlapped의 완료 APC를 alertable wait로 전부 배달·소진한다. 이 드레인이 끝나야
	// recvOver_/sendOver_가 안전히 파괴되고, 이후 WSACleanup이 미완료 I/O와 경합하지 않는다.
	// 무한 대기 방지를 위해 상한 가드(약 200ms)를 둔다.
	for (int32 guard = 0; pendingIo_ > 0 && guard < 200; ++guard) {
		SleepEx(1, TRUE);   // alertable: completionCallback APC 실행 → pendingIo_--
	}
}
