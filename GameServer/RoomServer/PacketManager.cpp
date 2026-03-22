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

SendBuffer* PacketManager::makeSEnterPacket(int32 playerId, const std::vector<PlayerInfo>& Infos) {
	int32 playerCnt = Infos.size();
	auto sendBuffer = SendBufferManager::open(sizeof(SEnterPacket) + sizeof(PlayerInfo) * playerCnt);
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto enterPacket = bw.reserve<SEnterPacket>();
	enterPacket->playerId = playerId;
	enterPacket->playerCnt = playerCnt;

	auto playerInfos = bw.reserve<PlayerInfo>(playerCnt);
	for (int32 i = 0; i < playerCnt; ++i) {
		playerInfos[i].playerId = Infos[i].playerId;
		playerInfos[i].materialSetIdx = Infos[i].materialSetIdx;
		playerInfos[i].pos = Infos[i].pos;
		playerInfos[i].orient = Infos[i].orient;
		playerInfos[i].scale = Infos[i].scale;
	}

	enterPacket->playersOffset = reinterpret_cast<uint64>(playerInfos) - reinterpret_cast<uint64>(enterPacket);
	enterPacket->size = bw.writeSize();
	enterPacket->type = PacketType::S_Enter;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}
