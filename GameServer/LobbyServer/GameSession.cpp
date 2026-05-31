#include "lspch.hpp"
#include "GameSession.hpp"

void GameSession::onConnected() {
	std::cout << "Lobby client connected. id: " << id() << '\n';
}

void GameSession::onDisconnected() {
	std::cout << "Lobby client disconnected. id: " << id() << '\n';
}

void GameSession::processPacket(byte* buffer, int32 len) {
	// TODO: 로비 패킷 처리
}
