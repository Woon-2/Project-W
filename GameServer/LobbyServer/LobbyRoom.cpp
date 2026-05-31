#include "lspch.hpp"
#include "LobbyRoom.hpp"
#include "LobbyManager.hpp"
#include "GameSession.hpp"
#include "SendBuffer.hpp"
#include "PacketManager.hpp"

LobbyRoom::LobbyRoom(const std::string& code) : code_(code) {}

bool LobbyRoom::isFull() const {
	std::lock_guard lock(mutex_);
	return static_cast<int>(players_.size()) >= maxPlayers;
}

int LobbyRoom::playerCnt() const {
	std::lock_guard lock(mutex_);
	return static_cast<int>(players_.size());
}

uint16 LobbyRoom::hostId() const {
	std::lock_guard lock(mutex_);
	return hostId_;
}

std::vector<uint16> LobbyRoom::playerIds() const {
	std::lock_guard lock(mutex_);
	std::vector<uint16> ids;
	ids.reserve(players_.size());
	for (auto* p : players_)
		ids.push_back(static_cast<uint16>(p->id()));
	return ids;
}

std::vector<LobbyPlayerInfo> LobbyRoom::playerInfos() const {
	std::lock_guard lock(mutex_);
	std::vector<LobbyPlayerInfo> infos;
	infos.reserve(players_.size());
	for (auto* p : players_)
		infos.push_back(LobbyPlayerInfo{ static_cast<uint16>(p->id()) });
	return infos;
}

bool LobbyRoom::enter(GameSession* session) {
	std::lock_guard lock(mutex_);

	if (static_cast<int>(players_.size()) >= maxPlayers)
		return false;

	LobbyPlayerInfo info{ static_cast<uint16>(session->id()) };
	auto joinedBuf = PacketManager::makeSLobbyRoomPlayerJoinedPacket(info);
	for (auto* p : players_)
		p->send(joinedBuf);

	players_.push_back(session);

	if (players_.size() == 1)
		hostId_ = static_cast<uint16>(session->id());

	return true;
}

void LobbyRoom::leave(GameSession* session) {
	bool isEmpty = false;
	std::string codeSnapshot;

	{
		std::lock_guard lock(mutex_);

		auto it = std::find(players_.begin(), players_.end(), session);
		if (it == players_.end())
			return;

		players_.erase(it);

		if (players_.empty()) {
			isEmpty      = true;
			codeSnapshot = code_;
		} else {
			if (hostId_ == static_cast<uint16>(session->id()))
				hostId_ = static_cast<uint16>(players_.front()->id());

			broadcast(PacketManager::makeSLobbyRoomPlayerLeftPacket(static_cast<uint16>(session->id())));
		}
	}

	if (isEmpty)
		LobbyManager::removeRoom(codeSnapshot);
}

void LobbyRoom::startGame() {
	std::lock_guard lock(mutex_);
	broadcast(PacketManager::makeSGameStartPacket(serverIp, roomServerPort, code_));
}

void LobbyRoom::broadcast(const std::shared_ptr<SendBuffer>& buf) {
	for (auto* p : players_)
		p->send(buf);
}
