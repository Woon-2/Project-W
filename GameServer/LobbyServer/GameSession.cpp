#include "lspch.hpp"
#include "GameSession.hpp"
#include "LobbyRoom.hpp"
#include "PacketManager.hpp"
#include "IdPool.hpp"

void GameSession::onConnected() {
	std::cout << "Lobby client connected. id: " << id() << '\n';
}

void GameSession::onDisconnected() {
	if ( myRoom_ ) {
		myRoom_->leave( this );
		myRoom_ = nullptr;
	}

	IdPool::push( id() );
	std::cout << "Lobby client disconnected. id: " << id() << '\n';
}

void GameSession::processPacket( byte* buffer, int32 len ) {
	PacketManager::handlePacket( this, buffer, len );
}
