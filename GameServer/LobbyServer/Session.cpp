#include "pch.hpp"
#include "Session.hpp"
#include "Memory.hpp"

void Session::disconnect(std::string_view cause) {
	if (connected_.exchange(false) == false) {
		return;
	}

	std::cout << "Disconnected: " << cause << '\n';	// log로 남길 것
	registerDisconnect();
}

void Session::send(SendBuffer* sendBuffer) {
	if (!isConnected()) {
		return;
	}

	sendQueue_.enqueue(sendBuffer);
	if (sending_.exchange(true) == false) {
		registerSend();
	}
}

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
		break;
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
	if(!isConnected()) {
		return;
	}

	sendEv_.clear();
	sendEv_.setOwner(this);	// add reference

	const int32 bulkSize = 100;
	auto sbVecs = std::vector<SendBuffer*>(bulkSize);
	auto size = sendQueue_.try_dequeue_bulk(sbVecs.begin(), bulkSize);

	sendEv_.sendBuffers_.reserve(size);
	for (int32 i = 0; i < size; ++i) {
		sendEv_.sendBuffers_.emplace_back(sbVecs[i]);
	}
	
	auto wsaBufs = std::vector<WSABUF>();
	wsaBufs.reserve(size);
	for (const auto& sendBuffer : sendEv_.sendBuffers_) {
		wsaBufs.emplace_back(WSABUF{
			.len = static_cast<ULONG>(sendBuffer->size()),
			.buf = reinterpret_cast<char*>(sendBuffer->data())
		});
	}

	DWORD numBytes{};
	if (SOCKET_ERROR == ::WSASend(sock_, wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()),
		&numBytes, 0, reinterpret_cast<WSAOVERLAPPED*>(&sendEv_), nullptr)
	) {
		int32 errCode = ::WSAGetLastError();
		if (errCode != ERROR_IO_PENDING) {
			handleError( "register send"sv, errCode);
			sendEv_.setOwner(nullptr);	// release reference

			for (auto sb : sendEv_.sendBuffers_) {
				xdelete(sb);
			}
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
		disconnect("RecvBuffer moveWritePos() failed"sv);
		return;
	}

	int32 dataSize = recvBuf_.dataSize();
	int32 readBytes = onRecv(recvBuf_.readPos(), dataSize);

	if (readBytes < 0 || readBytes > dataSize || !recvBuf_.moveReadPos(readBytes)) {
		disconnect("Failed to process received data"sv);
		return;
	}

	recvBuf_.clean();
	registerRecv();
}

void Session::processSend(int32 numBytes) {
	sendEv_.setOwner(nullptr);	// release reference

	for (auto sb : sendEv_.sendBuffers_) {
		xdelete(sb);
	}
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
		std::cout << "Session " << where << " Error\n";
		disconnect("Connection aborted or reset"sv);
		break;

	default:
		std::cout << "Session " << where << " Error Code - " << errCode << '\n'
			<< "Error Message : " << std::system_category().message(errCode) << '\n';
		break;
	}
}
