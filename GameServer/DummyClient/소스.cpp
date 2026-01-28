#include "pch.hpp"

#include <iostream>
#include <system_error>


#include "../LobbyServer/protocol.hpp"

int main()
{
	SocketUtils::init();

	auto clientSock = SocketUtils::createSocket();
	auto serverAddr = NetAddress(serverIp, serverPort);

	if (SOCKET_ERROR == ::connect(clientSock,
		reinterpret_cast<const SOCKADDR*>(&serverAddr.sockAddr()), sizeof(SOCKADDR_IN))
	) {
		int32 errCode = ::WSAGetLastError();
		std::cerr << "Connect failed with error: " << errCode << '\n'
			<< "Error message: " << std::system_category().message(errCode) << '\n';
		::exit(errCode);
	}

	while (true);

	SocketUtils::release();
}