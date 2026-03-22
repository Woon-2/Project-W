#include "rspch.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "MemoryManager.hpp"
#include "PacketManager.hpp"

void Room::enter(GameSession* session) {
	// player들에 대한 snapshot 만들기
	auto players = std::vector<PlayerInfo>();
	players.reserve(sessions_.size() + 1);	// 새로 들어오는 플레이어 + 기존 플레이어들

	auto newPlayerInfo = PlayerInfo{
		.playerId = static_cast<uint16>(session->id()),
		.materialSetIdx = 0,
	};
	players.emplace_back(std::move(newPlayerInfo));

	for (auto session : sessions_) {
		auto playerInfo = PlayerInfo{
			.playerId = static_cast<uint16>(session->id()),
			.materialSetIdx = 0,
		};
		players.emplace_back(std::move(playerInfo));
	}

	// 패킷 생성 후 send
	auto sendBuffer = PacketManager::makeSEnterPacket(session->id(), players);
	session->send(sendBuffer);

	// 새로 들어온 플레이어의 정보를 기존 플레이어들에게 브로드캐스트

	// room 상태 변경
	sessions_.push_back(session);
	sessionIdMap_[session->id()] = session;
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
