#include "sepch.hpp"
#include "Session.hpp"
#include "SendBuffer.hpp"
#include "BufferReader.hpp"

/*---------------
	 Session
---------------*/

void Session::dispatch(IoEvent* event, int32 numBytes) {
	switch (event->type()) {
	case IoType::disconnect:
		processDisconnect();
		break;

	case IoType::recv:
		processRecv(numBytes);
		break;

	case IoType::send:
		processSend(numBytes);
		break;

	default:
		std::cerr << "Unknown IO event type" << '\n';
		break;
	}
}

void Session::disconnect(std::string_view cause) {
	if (connected_.exchange(false) == false) {
		return;
	}

	std::cout << "Disconnect - " << cause << '\n';
	registerDisconnect();
}

void Session::send(const std::shared_ptr<SendBuffer>& sendBuffer) {
	if (!isConnected()) {
		return;
	}

	sendQueue_.enqueue(sendBuffer);
	if (sending_.exchange(true) == false) {
		registerSend();
	}
}

void Session::registerDisconnect() {
	disconnEv_.clear();
	disconnEv_.setOwner(this);	// add reference

	if (false == SocketUtils::DisconnectEx(sock_,
		reinterpret_cast<WSAOVERLAPPED*>(&disconnEv_), TF_REUSE_SOCKET, 0)
	) {
		int32 errCode = ::WSAGetLastError();
		if (errCode != ERROR_IO_PENDING) {
			handleError("register disconnect"sv, errCode);
			disconnEv_.setOwner(nullptr);	// release reference
		}
	}
}

void Session::registerRecv() {
	if (!isConnected()) {
		return;
	}

	recvEv_.clear();
	recvEv_.setOwner(this);	// add reference

	auto wsaBuf = WSABUF{
		.len = static_cast<ULONG>(recvBuf_.freeSize()),
		.buf = reinterpret_cast<char*>(recvBuf_.writePos())
	};

	DWORD numBytes{};
	DWORD flags{};
	if ( SOCKET_ERROR == ::WSARecv(sock_, &wsaBuf, 1,
		&numBytes, &flags, reinterpret_cast<WSAOVERLAPPED*>(&recvEv_), nullptr)
	) {
		int32 errCode = ::WSAGetLastError();
		if (errCode != ERROR_IO_PENDING) {
			handleError("register recv"sv, errCode);
			recvEv_.setOwner(nullptr);	// release reference
		}
	}
}

void Session::registerSend() {
	if (!isConnected()) {
		return;
	}

	sendEv_.clear();
	sendEv_.setOwner(this);	// add reference

	const int32 bulkSize = 100;
	sendEv_.sendBuffers_.resize(bulkSize);	// 개선 필요 : bulkSize를 고정하면 resize를 send할 때마다 해줄 필요가 없어지지 않을까
	auto size = sendQueue_.try_dequeue_bulk(sendEv_.sendBuffers_.begin(), bulkSize);

	auto wsaBufs = std::vector<WSABUF>();
	wsaBufs.reserve(size);
	for (int32 i = 0; i < size; ++i) {
		wsaBufs.emplace_back(WSABUF{
			.len = static_cast<ULONG>(sendEv_.sendBuffers_[i]->writeSize()),
			.buf = reinterpret_cast<char*>(sendEv_.sendBuffers_[i]->data())
		});
	}

	DWORD numBytes{};
	if (SOCKET_ERROR == ::WSASend(sock_, wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()),
		&numBytes, 0, reinterpret_cast<WSAOVERLAPPED*>(&sendEv_), nullptr)
	) {
		int32 errCode = ::WSAGetLastError();
		if (errCode != ERROR_IO_PENDING) {
			handleError("register send"sv, errCode);
			sendEv_.setOwner(nullptr);	// release reference
			sendEv_.sendBuffers_.clear();
			sending_.store(false);
		}
	}
}

void Session::processConnect() {
	connected_.store(true);
	onConnected();
	registerRecv();
}

void Session::processDisconnect() {
	disconnEv_.setOwner(nullptr);	// release reference
	onDisconnected();
}

void Session::processRecv(int32 numBytes) {
	recvEv_.setOwner(nullptr);	// release reference

	if (numBytes == 0) {
		disconnect("Recv 0"sv);
		return;
	}
	if (!recvBuf_.moveWritePos(numBytes)) {
		disconnect("Failed to write to recv buffer"sv);
		return;
	}

	int32 dataSize = recvBuf_.dataSize();
	int32 readSize = onRecv(recvBuf_.readPos(), dataSize);
	
	if (readSize < 0 || readSize > dataSize || !recvBuf_.moveReadPos(readSize)) {
		disconnect("Failed to process received data"sv);
		return;
	}

	recvBuf_.clean();
	registerRecv();
}

void Session::processSend(int32 numBytes) {
	sendEv_.setOwner(nullptr);	// release reference
	sendEv_.sendBuffers_.clear();

	if (numBytes == 0) {
		disconnect("Send 0"sv);
		return;
	}

	onSend(numBytes);
	sending_.store(false);
}

void Session::handleError(std::string_view where, int32 errCode) {
	switch (errCode) {
	case WSAECONNRESET:
	case WSAECONNABORTED:
		std::cerr << "Error in [" << where << "] (Error Code - " << errCode << ")\n"
			<< "Error Message - Connection reset or aborted\n";
		break;

	default:
		std::cerr << "Error in [" << where << "] (Error Code - " << errCode << ")\n"
			<< "Error Message - " << std::system_category().message(errCode) << '\n';
		break;
	}
}

/*---------------------
	 PacketSession
---------------------*/

int32 PacketSession::onRecv(byte* buffer, int32 len) {
	int32 recvLen{};

	while (true) {
		int32 dataSize = len - recvLen;

		// 최소한 패킷 헤더 크기만큼의 데이터가 수신되어야 한다.
		if (dataSize < sizeof(PacketHeader)) {
			break;
		}

		auto header = reinterpret_cast<PacketHeader*>(buffer + recvLen);
		// 패킷 헤더에 명시된 크기만큼의 데이터가 수신되어야 한다.
		// size는 패킷 헤더 크기 + 패킷 크기
		if (dataSize < header->size) {
			break;
		}

		processPacket(buffer + recvLen, header->size);
		recvLen += header->size;
	}

	return recvLen;
}
