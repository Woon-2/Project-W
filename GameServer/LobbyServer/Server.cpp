#include "pch.hpp"
#include "Server.hpp"
#include "Memory.hpp"

void Server::start() {
	listener_ = xnew<Listener>();
	ASSERT_CRASH(iocpCore_.registerHandle(listener_));
	listener_->startAccept();
}

void Server::stop() {
	xdelete(listener_);
	listener_ = nullptr;
}

IocpCore Server::iocpCore_;
Listener* Server::listener_ = nullptr;
