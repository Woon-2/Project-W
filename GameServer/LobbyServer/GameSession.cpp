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

int32 GameSession::onRecv(uint8* buffer, int32 len) {
	//std::cout << "recv len: " << len << '\n';

	auto sendBuffer = SendBufferManager::open(1024);
	std::memcpy(sendBuffer->data(), buffer, len);
	sendBuffer->close(static_cast<uint32>(len));

	send(sendBuffer);
	return len;
}
