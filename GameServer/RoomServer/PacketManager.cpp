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

	case PacketType::C_MouseMove:
		handleCMouseMovePacket(session, buffer, len);
		break;

	case PacketType::C_Attack:
		handleCAttackPacket(session, buffer, len);
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
	cMvPktClone->velocity = clientMovePacket->velocity;
	
	session->room()->doAsync([session, cMvPktClone]() {
		session->room()->move(session->id(), cMvPktClone);
	});
}

void PacketManager::handleCMouseMovePacket(GameSession* session, byte* buffer, int32 len) {
	auto clientMouseMovePacket = reinterpret_cast<CMouseMovePacket*>(buffer);
	auto cMouseMvPktClone = ObjectPool<CMouseMovePacket>::pop();

	cMouseMvPktClone->yawRadian = clientMouseMovePacket->yawRadian;

	session->room()->doAsync([session, cMouseMvPktClone]() {
		session->room()->rotate(session->id(), cMouseMvPktClone);
	});
}

void PacketManager::handleCAttackPacket( GameSession* session, byte* buffer, int32 len ) {
	uint64 clientMs = reinterpret_cast<CAttackPacket*>(buffer)->clientMs;
	session->room()->doAsync( [ session, clientMs ]() {
		session->room()->attack( session->id(), clientMs );
	} );
}

std::shared_ptr<SendBuffer> PacketManager::makeSEnterPacket(const PlayerInfo& playerInfo, const std::vector<ObjectInfo>& objInfos) {
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

std::shared_ptr<SendBuffer> PacketManager::makeSEnterOtherPacket(const PlayerInfo& playerInfo) {
	auto sendBuffer = SendBufferManager::open(sizeof(SEnterOtherPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto enterOtherPacket = bw.reserve<SEnterOtherPacket>();
	enterOtherPacket->otherInfo = playerInfo;

	enterOtherPacket->size = bw.writeSize();
	enterOtherPacket->type = PacketType::S_Enter_Other;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSLeavePacket(uint16 playerId) {
	auto sendBuffer = SendBufferManager::open(sizeof(SLeavePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto leavePacket = bw.reserve<SLeavePacket>();
	leavePacket->playerId = playerId;

	leavePacket->size = bw.writeSize();
	leavePacket->type = PacketType::S_Leave;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSMovePacket(uint16 playerId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 velocity) {
	auto sendBuffer = SendBufferManager::open(sizeof(SMovePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto sMvPkt = bw.reserve<SMovePacket>();
	sMvPkt->playerId = playerId;
	sMvPkt->pos = pos;
	sMvPkt->velocity = velocity;

	sMvPkt->size = bw.writeSize();
	sMvPkt->type = PacketType::S_Move;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSMouseMovePacket(uint16 playerId, float yawRad) {
	auto sendBuffer = SendBufferManager::open(sizeof(SMouseMovePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto sMouseMvPkt = bw.reserve<SMouseMovePacket>();
	sMouseMvPkt->playerId = playerId;
	sMouseMvPkt->yawRadian = yawRad;

	sMouseMvPkt->size = bw.writeSize();
	sMouseMvPkt->type = PacketType::S_MouseMove;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSNpcMovePacket(uint16 npcId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 orient, DirectX::XMFLOAT3 velocity) {
	auto sendBuffer = SendBufferManager::open(sizeof(SNpcMovePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto sNpcMvPkt = bw.reserve<SNpcMovePacket>();
	sNpcMvPkt->npcId = npcId;
	sNpcMvPkt->pos = pos;
	sNpcMvPkt->orient = orient;
	sNpcMvPkt->velocity = velocity;

	sNpcMvPkt->size = bw.writeSize();
	sNpcMvPkt->type = PacketType::S_NpcMove;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSNpcMoveBatchPacket(const std::vector<SNpcMoveInfo>& infos) {
	const uint16 count = static_cast<uint16>(infos.size());
	auto sendBuffer = SendBufferManager::open( sizeof( SNpcMoveBatchPacket ) + sizeof( SNpcMoveInfo ) * count );
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto sNpcMvBatPkt = bw.reserve<SNpcMoveBatchPacket>();

	auto entries = bw.reserve<SNpcMoveInfo>(count);
	for ( uint16 i = 0; i < count; ++i ) {
		entries[ i ] = infos[ i ];
	}

	sNpcMvBatPkt->dataOffset = static_cast<uint16>( reinterpret_cast<uint64>( entries ) - reinterpret_cast<uint64>( sNpcMvBatPkt ) );
	sNpcMvBatPkt->npcCount = count;

	sNpcMvBatPkt->size = bw.writeSize();
	sNpcMvBatPkt->type = PacketType::S_NpcMoveBatch;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSNpcAttackPacket( uint16 npcId ) {
	auto sendBuffer = SendBufferManager::open( sizeof( SNpcAttackPacket ) );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SNpcAttackPacket>();
	pkt->npcId = npcId;
	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_NpcAttack;

	sendBuffer->close( bw.writeSize() );
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSHitPacket(uint16 targetId, int32 newHp) {
	auto sendBuffer = SendBufferManager::open(sizeof(SHitPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SHitPacket>();
	pkt->targetId = targetId;
	pkt->newHp    = newHp;
	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_Hit;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSTimeSyncPacket(uint64 serverMs) {
	auto sendBuffer = SendBufferManager::open(sizeof(STimeSyncPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<STimeSyncPacket>();
	pkt->serverMs = serverMs;
	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_TimeSync;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}
