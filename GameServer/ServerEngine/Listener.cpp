#include "pch.hpp"
#include "Listener.hpp"
#include "IoEvent.hpp"
#include "Session.hpp"


void Listener::startAccept() {
	SocketUtils::setReuseAddr(listenSock_, true);
	SocketUtils::setTcpNoDelay(listenSock_, true);

	SocketUtils::bind(listenSock_, NetAddress(serverPort));
	SocketUtils::listen(listenSock_);

	const int32 count = static_cast<int32>(numberOfPhysicalCores());
	acceptEvs_.reserve(count);

	for (int32 i = 0; i < count; ++i) {
		auto acceptEv = onew<AcceptEvent>();
		acceptEv->setOwner(this);

		acceptEvs_.emplace_back(acceptEv);
		registerAccept(acceptEv);
	}
}

void Listener::dispatch(IoEvent* event, int32 numBytes) {
	ASSERT_CRASH(event->type() == IoType::accept);
	auto acceptEv = static_cast<AcceptEvent*>(event);
	processAccept(acceptEv);
}

void Listener::registerAccept(AcceptEvent* event) {
	auto session = onew<Session>();
}

void Listener::processAccept(AcceptEvent* event) {

}
