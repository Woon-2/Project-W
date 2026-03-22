#include "rspch.hpp"
#include "PacketManager.hpp"
#include "SendBuffer.hpp"
#include "BufferWriter.hpp"

void PacketManager::handlePacket(byte* buffer, int32 len) {
	auto header = reinterpret_cast<PacketHeader*>(buffer);
	
	switch (header->type) {


	default:
		std::cout << "Unknown packet type received. Type: " << static_cast<uint16>(header->type) << '\n';
		break;
	}
}

SendBuffer* PacketManager::makeSEnterPacket(int32 playerId, int32 playerCnt) {
	auto sendBuffer = SendBufferManager::open(sizeof(SEnterPacket) + sizeof(PlayerInfo) * playerCnt);
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto enterPacket = bw.reserve<SEnterPacket>();
	enterPacket->playerId = playerId;
	enterPacket->playerCnt = playerCnt;

	auto playerInfos = bw.reserve<PlayerInfo>(playerCnt);
	playerInfos[0].playerId = playerId;
	playerInfos[0].materialSetIdx = 0;
}
