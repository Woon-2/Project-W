#include "pch.hpp"
#include "PacketManager.hpp"
#include "online/onlineGame.hpp"

void PacketManager::handlePacket(Online::Game* game, byte* buffer, int32 len) {
	auto header = reinterpret_cast<PacketHeader*>(buffer);

	switch (header->type) {
	case PacketType::S_Enter:
		handleSEnterPacket(game, buffer, len);
		break;

	case PacketType::S_Enter_Other:
		handleSEnterOtherPacket(game, buffer, len);
		break;

	case PacketType::S_Leave:
		handleSLeavePacket(game, buffer, len);
		break;

	default:
		std::cout << "Unknown packet type received. Type: " << static_cast<uint16>(header->type) << '\n';
		break;
	}
}

void PacketManager::handleSEnterPacket(Online::Game* game, byte* buffer, int32 len) {
	auto enterPacket = reinterpret_cast<SEnterPacket*>(buffer);
	auto playerInfo = enterPacket->myInfo;

	game->setupPlayer(playerInfo);

	auto objList = enterPacket->getObjectList();

	for (int32 i = 0; i < objList.count(); ++i) {
		const auto& objInfo = objList[i];

		switch (objInfo.type) {
		case ObjectType::Player:
			game->createOtherPlayer(objInfo);
			break;

		case ObjectType::Ground:
			game->setupGround(objInfo);
			break;

		default:
			std::cout << "Unknown object type received. Type: " << static_cast<uint16>(objInfo.type) << '\n';
			break;
		}
	}
}

void PacketManager::handleSEnterOtherPacket(Online::Game* game, byte* buffer, int32 len) {
	auto enterOtherPacket = reinterpret_cast<SEnterOtherPacket*>(buffer);
	auto otherPlayerInfo = enterOtherPacket->otherInfo;

	game->createOtherPlayer(otherPlayerInfo);
}

void PacketManager::handleSLeavePacket(Online::Game* game, byte* buffer, int32 len) {
	auto leavePacket = reinterpret_cast<SLeavePacket*>(buffer);

	game->removePlayer(leavePacket->playerId);
}
