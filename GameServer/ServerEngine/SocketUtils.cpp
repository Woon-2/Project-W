#include "pch.hpp"
#include "SocketUtils.hpp"

void SocketUtils::init() {
	WSADATA wsaData{};
	ASSERT_CRASH(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);

	auto dummySock = createSocket();
	bindWindowsFuncEx(dummySock, WSAID_CONNECTEX, reinterpret_cast<LPVOID*>(&ConnectEx));
	bindWindowsFuncEx(dummySock, WSAID_DISCONNECTEX, reinterpret_cast<LPVOID*>(&DisconnectEx));
	closeSocket(dummySock);
}

void SocketUtils::release() {
	WSACleanup();
}

SOCKET SocketUtils::createSocket() {
	auto sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (sock == INVALID_SOCKET) {
		CRASH("WSASocket failed.");
	}

	return sock;
}

void SocketUtils::closeSocket(SOCKET& sock) {
	if (sock == INVALID_SOCKET) {
		return;
	}

	closesocket(sock);
	sock = INVALID_SOCKET;
}

void SocketUtils::bindWindowsFuncEx(SOCKET sock, GUID guid, LPVOID* fn) {
	DWORD bytes{};
	ASSERT_CRASH(WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guid, sizeof(guid), fn, sizeof(*fn), &bytes, nullptr, nullptr) != SOCKET_ERROR);
}

void SocketUtils::setReuseAddr(SOCKET sock, bool reuse) {
	ASSERT_CRASH(setSockOpt(sock, SOL_SOCKET, SO_REUSEADDR, reuse));
}

void SocketUtils::setTcpNoDelay(SOCKET sock, bool noDelay) {
	ASSERT_CRASH(setSockOpt(sock, IPPROTO_TCP, TCP_NODELAY, noDelay));
}

void SocketUtils::setUpdateAcceptContext(SOCKET sock, SOCKET listenSock) {
	// This option should be set if the getpeername, getsockname, 
	// getsockopt, or setsockopt functions are to be used on the accepted socket.
	ASSERT_CRASH(setSockOpt(sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, listenSock));
}

void SocketUtils::bind(SOCKET sock, const NetAddress& netAddr) {
	ASSERT_CRASH(::bind(sock, reinterpret_cast<const SOCKADDR*>(&netAddr.sockAddr()), sizeof(SOCKADDR_IN)) != SOCKET_ERROR);
}

void SocketUtils::listen(SOCKET sock, int32 backlog) {
	ASSERT_CRASH(::listen(sock, backlog) != SOCKET_ERROR);
}

LPFN_CONNECTEX SocketUtils::ConnectEx = nullptr;
LPFN_DISCONNECTEX SocketUtils::DisconnectEx = nullptr;
