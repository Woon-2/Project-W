#include "lspch.hpp"
#include "LobbyServer.hpp"

void LobbyServer::start() {
	reactor_.registerHandle( listener_.getHandle() );
	listener_.startAccept();
	std::cout << "Lobby Server started on port " << lobbyServerPort << '\n';
}
