#include "rspch.hpp"
#include "Listener.hpp"
#include "IoEvent.hpp"
#include "GameSession.hpp"

void Listener::startAccept() {
	SocketUtils::setReuseAddr(listenSock_, true);
	SocketUtils::setTcpNoDelay(listenSock_, true);

	SocketUtils::bind(listenSock_, NetAddress(roomServerPort));
	SocketUtils::listen(listenSock_);

	const int32 count = static_cast<int32>(numberOfPhysicalCores());
	acceptEvs_.reserve(count);

	for (int32 i = 0; i < count; ++i) {
		auto acceptEv = ObjectPool<AcceptEvent>::pop();
		acceptEv->setOwner(this);
		registerAccept(acceptEv);
		acceptEvs_.emplace_back(acceptEv);
	}
}

void Listener::dispatch(IoEvent* event, int32 numBytes) {
	ASSERT_CRASH(event->type() == IoType::accept);
	auto acceptEv = static_cast<AcceptEvent*>(event);
	processAccept(acceptEv);
}

void Listener::registerAccept(AcceptEvent* event) {
	auto session = ObjectPool<GameSession>::pop();
	CreateIoCompletionPort(session->getHandle(), iocpHandle_, 0, 0);

	event->clear();
	event->setSession(session);

	if (false == AcceptEx(listenSock_, session->getSocket(), session->recvBuf_.writePos(), 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, nullptr, reinterpret_cast<WSAOVERLAPPED*>(event))
	) {
		const int32 errCode = WSAGetLastError();
		if (errCode != ERROR_IO_PENDING) {
			std::cout << "AcceptEx failed. Error Code - " << errCode << '\n'
				<< "Error Message - " << std::system_category().message(errCode) << '\n';

			//ObjectPool<GameSession>::push(session);
			//registerAccept(event);
			exit(-1);
		}
	};
}

void Listener::processAccept(AcceptEvent* event) {
	auto session = event->session();
	SocketUtils::setUpdateAcceptContext(session->getSocket(), listenSock_);

	SOCKADDR_IN clientAddr{};
	int32 len = sizeof(clientAddr);
	if (SOCKET_ERROR == getpeername(session->getSocket(), reinterpret_cast<SOCKADDR*>(&clientAddr), &len)) {
		const int32 errCode = WSAGetLastError();
		std::cout << "getpeername failed. Error Code - " << errCode << '\n'
			<< "Error Message - " << std::system_category().message(errCode) << '\n';

		exit(errCode);
	}

	session->setNetAddress(NetAddress(clientAddr));
	session->setId(IdPool::pop());
	std::cout << "New client connected. Assigned GameSession ID: " << session->id() << '\n';
	session->processConnect();

	registerAccept(event);
}