#include "rspch.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "MemoryManager.hpp"
#include "PacketManager.hpp"

void Room::enter(GameSession* session) {
	// player들에 대한 snapshot 만들기
	auto players = std::vector<PlayerInfo>();
	players.reserve(sessions_.size() + 1); // 현재 방에 있는 플레이어들 + 새로 들어오는 플레이어

	for (auto session : sessions_) {
		auto playerInfo = PlayerInfo{
			.playerId = static_cast<uint16>(session->id()),
			.materialSetIdx = 0,
		};
	}

	sessions_.push_back(session);
	sessionIdMap_[session->id()] = session;

	//auto sendBuffer = PacketManager::makeSEnterPacket(/*정보 넘겨주기*/);
}

void Room::leave(GameSession* session) {
	std::erase_if(sessions_, [session](GameSession* s) { return s == session; });
	odelete(session);
	sessionIdMap_.erase(session->id());
}

void Room::broadcast(SendBuffer* sendBuffer) {
	for(auto session : sessions_) {
		session->send(sendBuffer);
	}
}
