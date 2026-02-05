#include "pch.hpp"

#include <iostream>
#include <system_error>


#include "../common/protocol.hpp"

int main()
{
	SocketUtils::init();

	auto clientSock = SocketUtils::createSocket();
	auto serverAddr = NetAddress(serverIp, serverPort);

	if (SOCKET_ERROR == ::connect(clientSock,
		reinterpret_cast<const SOCKADDR*>(&serverAddr.sockAddr()), sizeof(SOCKADDR_IN))
	) {
		int32 errCode = ::WSAGetLastError();
		if(errCode == WSAEWOULDBLOCK) {
			std::cout << "Connect is in progress...\n";
		}
		else {
			std::cerr << "Connect failed with error: " << errCode << '\n'
				<< "Error message: " << std::system_category().message(errCode) << '\n';
			::exit(errCode);
		}
	}

	while (true) {
		char msg[]{"Hello, Lobby Server!"};
		::send(clientSock, msg, sizeof(msg), 0);

		::recv(clientSock, msg, sizeof(msg), 0);
		std::cout << "Received from server: " << msg << '\n';

		std::this_thread::sleep_for(1s);
	}

	SocketUtils::release();
}