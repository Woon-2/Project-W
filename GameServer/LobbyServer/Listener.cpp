#include "pch.hpp"
#include "Listener.hpp"
#include "IoEvent.hpp"
#include "GameSession.hpp"
#include "Server.hpp"
#include "IdPool.hpp"

void Listener::startAccept() {
	SocketUtils::setReuseAddr(listenSock_, true);
	SocketUtils::setTcpNoDelay(listenSock_, true);
	
	auto netAddr = NetAddress(serverPort);
	SocketUtils::bind(listenSock_, netAddr);
	SocketUtils::listen(listenSock_);

	// 물리 코어 수 만큼 acceptEvent 등록
	const int32 acceptEvCnt = static_cast<int32>(numberOfPhysicalCores());
	acceptEvs_.reserve(acceptEvCnt);

	for (int32 i = 0; i < acceptEvCnt; ++i) {
		auto ev = xnew<AcceptEvent>();
		ev->setOwner(this);
		acceptEvs_.emplace_back(ev);
		registerAccept(ev);
	}	
}

void Listener::dispatch(IoEvent* event, int32 numBytes) {
	ASSERT_CRASH(event->type() == IoType::accept);
	auto acceptEv = static_cast<AcceptEvent*>(event);
	processAccept(acceptEv);
}

void Listener::registerAccept(AcceptEvent* acceptEv) {
	auto session = xnew<GameSession>();
	Server::iocpCore().registerHandle(session);

	acceptEv->clear();
	acceptEv->setSession(session);

	if (false == ::AcceptEx(listenSock_, session->getSocket(), session->recvBuf_.writePos(), 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, nullptr, reinterpret_cast<WSAOVERLAPPED*>(acceptEv))
	) {
		const int32 errCode = ::WSAGetLastError();
		if (errCode != ERROR_IO_PENDING) {
			std::cout << "AcceptEx failed. Error Code - " << errCode << '\n'
				<< "Error Message - " << std::system_category().message(errCode) << '\n';

			registerAccept(acceptEv);
		}
	}
}

void Listener::processAccept(AcceptEvent* acceptEv) {
	auto session = acceptEv->session();
	SocketUtils::setUpdateAcceptContext(session->getSocket(), listenSock_);

	SOCKADDR_IN clientAddr{};
	int32 len = sizeof(clientAddr);
	if (SOCKET_ERROR == ::getpeername(session->getSocket(), reinterpret_cast<SOCKADDR*>(&clientAddr), &len)) {
		const int32 errCode = ::WSAGetLastError();
		std::cout << "getpeername failed. Error Code -" << errCode << '\n'
			<< "Error Message - " << std::system_category().message(errCode) << '\n';

		::exit(errCode);
	}

	session->setNetAddress(NetAddress(clientAddr));
	session->setId(IdPool::pop());
	session->processConnect();

	registerAccept(acceptEv);
}
