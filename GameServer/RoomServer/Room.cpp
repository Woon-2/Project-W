#include "rspch.hpp"
#include "Room.hpp"
#include "Player.hpp"

void Room::enter(Player* player) {
	players_.push_back(player);
	playerIdMap_[player->id()] = player;
}

void Room::leave(Player* player) {
	std::erase(players_, [player](Player* p) { return p == player; });
	playerIdMap_.erase(player->id());
}

void Room::broadcast(SendBuffer* sendBuffer) {
	for(auto player : players_) {
		player->ownerSession()->send(sendBuffer);
	}
}
