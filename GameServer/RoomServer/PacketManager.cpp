#include "rspch.hpp"
#include "PacketManager.hpp"
#include "SendBuffer.hpp"
#include "BufferWriter.hpp"
#include "GameSession.hpp"
#include "ObjectPool.hpp"
#include "Room.hpp"

void PacketManager::handlePacket(GameSession* session, byte* buffer, int32 len) {
	auto header = reinterpret_cast<PacketHeader*>(buffer);
	
	switch (header->type) {
	case PacketType::C_Move:
		handleCMovePacket(session, buffer, len);
		break;

	default:
		std::cout << "Unknown packet type received. Type: " << static_cast<uint16>(header->type) << '\n';
		break;
	}
}

void PacketManager::handleCMovePacket(GameSession* session, byte* buffer, int32 len) {
	auto clientMovePacket = reinterpret_cast<CMovePacket*>(buffer);
	auto cMvPktClone = ObjectPool<CMovePacket>::pop();

	cMvPktClone->pos = clientMovePacket->pos;
	cMvPktClone->orient = clientMovePacket->orient;
	cMvPktClone->velocity = clientMovePacket->velocity;
	
	session->room()->doAsync([session, cMvPktClone]() {
		session->room()->move(session->id(), cMvPktClone);
	});
}

SendBuffer* PacketManager::makeSEnterPacket(const PlayerInfo& playerInfo, const std::vector<ObjectInfo>& objInfos) {
	int32 objCnt = static_cast<int32>(objInfos.size());
	auto sendBuffer = SendBufferManager::open(sizeof(SEnterPacket) + sizeof(ObjectInfo) * objCnt);
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto enterPacket = bw.reserve<SEnterPacket>();
	enterPacket->myInfo = playerInfo;
	enterPacket->objCnt = objCnt;

	auto infos = bw.reserve<ObjectInfo>(objCnt);
	for (int32 i = 0; i < objCnt; ++i) {
		infos[i].type = objInfos[i].type;
		infos[i].objectId = objInfos[i].objectId;
		infos[i].materialSetIdx = objInfos[i].materialSetIdx;
		infos[i].pos = objInfos[i].pos;
		infos[i].orient = objInfos[i].orient;
		infos[i].scale = objInfos[i].scale;
	}

	enterPacket->objsOffset = static_cast<uint16>(reinterpret_cast<uint64>(infos) - reinterpret_cast<uint64>(enterPacket));
	enterPacket->size = bw.writeSize();
	enterPacket->type = PacketType::S_Enter;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

SendBuffer* PacketManager::makeSEnterOtherPacket(const PlayerInfo& playerInfo) {
	auto sendBuffer = SendBufferManager::open(sizeof(SEnterOtherPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto enterOtherPacket = bw.reserve<SEnterOtherPacket>();
	enterOtherPacket->otherInfo = playerInfo;

	enterOtherPacket->size = bw.writeSize();
	enterOtherPacket->type = PacketType::S_Enter_Other;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

SendBuffer* PacketManager::makeSLeavePacket(uint16 playerId) {
	auto sendBuffer = SendBufferManager::open(sizeof(SLeavePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto leavePacket = bw.reserve<SLeavePacket>();
	leavePacket->playerId = playerId;

	leavePacket->size = bw.writeSize();
	leavePacket->type = PacketType::S_Leave;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

SendBuffer* PacketManager::makeSMovePacket(uint16 playerId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 orient, DirectX::XMFLOAT3 velocity) {
	auto sendBuffer = SendBufferManager::open(sizeof(SMovePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto sMvPkt = bw.reserve<SMovePacket>();
	sMvPkt->playerId = playerId;
	sMvPkt->pos = pos;
	sMvPkt->orient = orient;
	sMvPkt->velocity = velocity;

	sMvPkt->size = bw.writeSize();
	sMvPkt->type = PacketType::S_Move;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}
