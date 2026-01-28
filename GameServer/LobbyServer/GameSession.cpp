#include "pch.hpp"
#include "GameSession.hpp"
#include "GameSessionManager.hpp"

void GameSession::onConnected() {
	std::cout << "GameSession connected. id: " << id() << '\n';
	GameSessionManager::addSession(this);
}

void GameSession::onDisconnected() {
	GameSessionManager::removeSession(static_cast<uint32>(id()));
}

void GameSession::processPacket(uint8* buffer, int32 len) {
}
