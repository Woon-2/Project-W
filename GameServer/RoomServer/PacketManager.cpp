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
	case PacketType::C_Enter:
		handleCEnterPacket(session, buffer, len);
		break;

	case PacketType::C_Move:
		handleCMovePacket(session, buffer, len);
		break;

	case PacketType::C_DebugTeleport:
		handleCDebugTeleportPacket(session, buffer, len);
		break;

	case PacketType::C_MouseMove:
		handleCMouseMovePacket(session, buffer, len);
		break;

	case PacketType::C_Attack:
		handleCAttackPacket(session, buffer, len);
		break;

	case PacketType::C_SkillStart:
		handleCSkillStartPacket(session, buffer, len);
		break;

	case PacketType::C_SelectSkill:
		handleCSelectSkillPacket(session, buffer, len);
		break;

	default:
		std::cout << "Unknown packet type received. Type: " << static_cast<uint16>(header->type) << '\n';
		break;
	}
}

void PacketManager::handleCEnterPacket(GameSession* session, byte* buffer, int32 len) {
	auto pkt = reinterpret_cast<CEnterPacket*>(buffer);
	std::string code(pkt->lobbyCode, strnlen_s(pkt->lobbyCode, sizeof(pkt->lobbyCode)));
	const auto ordinal = static_cast<uint8>(pkt->weaponType);
	session->player()->setWeaponType(
		ordinal <= static_cast<uint8>(PlayerWeaponType::HeavyArrow)
			? pkt->weaponType
			: PlayerWeaponType::Katana);
	session->enterRoom(code);
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

void PacketManager::handleCDebugTeleportPacket(GameSession* session, byte* buffer, int32 len) {
	auto pkt = reinterpret_cast<CDebugTeleportPacket*>(buffer);
	const DirectX::XMFLOAT3 pos = pkt->pos;
	session->room()->doAsync([session, pos]() {
		session->room()->debugTeleport(session->id(), pos);
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

void PacketManager::handleCSkillStartPacket( GameSession* session, byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<CSkillStartPacket*>(buffer);
	uint32 skillAssetId = pkt->skillAssetId;
	uint64 clientMs     = pkt->clientMs;
	uint32 skillSeed    = pkt->skillSeed;
	session->room()->doAsync( [session, skillAssetId, clientMs, skillSeed]() {
		session->room()->skillStart( session->id(), skillAssetId, clientMs, skillSeed );
	} );
}

void PacketManager::handleCSelectSkillPacket( GameSession* session, byte* buffer, int32 len ) {
	uint8 slot = reinterpret_cast<CSelectSkillPacket*>(buffer)->slot;
	session->room()->doAsync( [session, slot]() {
		session->room()->selectSkill( session->id(), slot );
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
		infos[i].weaponType = objInfos[i].weaponType;
		infos[i].hp = objInfos[i].hp;
		infos[i].maxHp = objInfos[i].maxHp;
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

std::shared_ptr<SendBuffer> PacketManager::makeSPlayerKnockbackPacket(uint16 playerId, float dirX, float dirZ, float speed, uint16 knockMs, uint16 postLockMs) {
	auto sendBuffer = SendBufferManager::open(sizeof(SPlayerKnockbackPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SPlayerKnockbackPacket>();
	pkt->playerId   = playerId;
	pkt->dirX       = dirX;
	pkt->dirZ       = dirZ;
	pkt->speed      = speed;
	pkt->knockMs    = knockMs;
	pkt->postLockMs = postLockMs;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_PlayerKnockback;

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

std::shared_ptr<SendBuffer> PacketManager::makeSNpcSpawnBatchPacket(const std::vector<ObjectInfo>& objInfos) {
	const uint16 count = static_cast<uint16>(objInfos.size());
	auto sendBuffer = SendBufferManager::open(sizeof(SNpcSpawnBatchPacket) + sizeof(ObjectInfo) * count);
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SNpcSpawnBatchPacket>();

	auto infos = bw.reserve<ObjectInfo>(count);
	for (uint16 i = 0; i < count; ++i) {
		infos[i] = objInfos[i];
	}

	pkt->dataOffset = static_cast<uint16>(reinterpret_cast<uint64>(infos) - reinterpret_cast<uint64>(pkt));
	pkt->objCnt     = count;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_NpcSpawnBatch;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSNpcBarrierPacket(bool active, const std::vector<uint32>& npcIds) {
	const uint16 count = static_cast<uint16>(npcIds.size());
	auto sendBuffer = SendBufferManager::open(sizeof(SNpcBarrierPacket) + sizeof(SNpcBarrierInfo) * count);
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SNpcBarrierPacket>();

	auto entries = bw.reserve<SNpcBarrierInfo>(count);
	for (uint16 i = 0; i < count; ++i) {
		entries[i].npcId = static_cast<uint16>(npcIds[i]);
	}

	pkt->active     = active ? 1 : 0;
	pkt->dataOffset = static_cast<uint16>(reinterpret_cast<uint64>(entries) - reinterpret_cast<uint64>(pkt));
	pkt->npcCount   = count;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_NpcBarrier;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSNpcHidePacket(const std::vector<uint32>& npcIds) {
	const uint16 count = static_cast<uint16>(npcIds.size());
	auto sendBuffer = SendBufferManager::open(sizeof(SNpcHidePacket) + sizeof(SNpcHideInfo) * count);
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SNpcHidePacket>();

	auto entries = bw.reserve<SNpcHideInfo>(count);
	for (uint16 i = 0; i < count; ++i) {
		entries[i].npcId = static_cast<uint16>(npcIds[i]);
	}

	pkt->dataOffset = static_cast<uint16>(reinterpret_cast<uint64>(entries) - reinterpret_cast<uint64>(pkt));
	pkt->npcCount   = count;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_NpcHide;

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

std::shared_ptr<SendBuffer> PacketManager::makeSPlayerAttackPacket( uint16 attackerId ) {
	auto sendBuffer = SendBufferManager::open( sizeof( SPlayerAttackPacket ) );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SPlayerAttackPacket>();
	pkt->attackerId = attackerId;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_PlayerAttack;

	sendBuffer->close( bw.writeSize() );
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSHitPacket(uint16 attackerId, uint16 targetId, int32 newHp) {
	auto sendBuffer = SendBufferManager::open(sizeof(SHitPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SHitPacket>();
	pkt->attackerId = attackerId;
	pkt->targetId = targetId;
	pkt->newHp    = newHp;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_Hit;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSSkillSelectPacket(uint16 playerId, uint8 slot) {
	auto sendBuffer = SendBufferManager::open(sizeof(SSkillSelectPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SSkillSelectPacket>();
	pkt->playerId = playerId;
	pkt->slot     = slot;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_SkillSelect;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSSkillChargePacket(uint16 playerId, uint8 slot, float charge) {
	auto sendBuffer = SendBufferManager::open(sizeof(SSkillChargePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SSkillChargePacket>();
	pkt->playerId = playerId;
	pkt->slot     = slot;
	pkt->charge   = charge;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_SkillCharge;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSSkillUseRejectPacket(uint8 slot) {
	auto sendBuffer = SendBufferManager::open(sizeof(SSkillUseRejectPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SSkillUseRejectPacket>();
	pkt->slot = slot;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_SkillUseReject;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSComboStatePacket(uint16 playerId, uint16 comboCount, float windowMs) {
	auto sendBuffer = SendBufferManager::open(sizeof(SComboStatePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SComboStatePacket>();
	pkt->playerId   = playerId;
	pkt->comboCount = comboCount;
	pkt->windowMs   = windowMs;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_ComboState;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSPlayerHpPacket(uint16 playerId, int32 newHp) {
	auto sendBuffer = SendBufferManager::open(sizeof(SPlayerHpPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SPlayerHpPacket>();
	pkt->playerId = playerId;
	pkt->newHp    = newHp;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_PlayerHp;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}


std::shared_ptr<SendBuffer> PacketManager::makeSSkillStartPacket(uint32 skillAssetId, uint16 ownerId, uint16 elapsedMs, uint32 skillSeed) {
	auto sendBuffer = SendBufferManager::open(sizeof(SSkillStartPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt          = bw.reserve<SSkillStartPacket>();
	pkt->skillAssetId = skillAssetId;
	pkt->ownerId      = ownerId;
	pkt->elapsedMs    = elapsedMs;
	pkt->skillSeed    = skillSeed;
	pkt->size         = bw.writeSize();
	pkt->type         = PacketType::S_SkillStart;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSDebugHitboxPacket(const OBBInfo* obbs, uint16 count) {
	auto sendBuffer = SendBufferManager::open(sizeof(SDebugHitboxPacket) + sizeof(OBBInfo) * count);
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SDebugHitboxPacket>();
	auto entries = bw.reserve<OBBInfo>(count);
	for (uint16 i = 0; i < count; ++i)
		entries[i] = obbs[i];

	pkt->dataOffset = static_cast<uint16>(reinterpret_cast<uint64>(entries) - reinterpret_cast<uint64>(pkt));
	pkt->obbCount   = count;
	pkt->size       = bw.writeSize();
	pkt->type       = PacketType::S_DebugHitbox;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSSkillHitPacket(uint16 attackerId, uint16 targetId,
                                                                int32 newHp, uint32 skillAssetId,
                                                                DirectX::XMFLOAT3 targetVelocity) {
	auto sendBuffer = SendBufferManager::open(sizeof(SSkillHitPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt              = bw.reserve<SSkillHitPacket>();
	pkt->attackerId       = attackerId;
	pkt->targetId         = targetId;
	pkt->newHp            = newHp;
	pkt->skillAssetId     = skillAssetId;
	pkt->targetVelocity   = targetVelocity;
	pkt->size             = bw.writeSize();
	pkt->type             = PacketType::S_SkillHit;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSStrongholdStatePacket(uint16 strongholdId, int32 hp, uint8 state) {
	auto sendBuffer = SendBufferManager::open(sizeof(SStrongholdStatePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt          = bw.reserve<SStrongholdStatePacket>();
	pkt->strongholdId = strongholdId;
	pkt->hp           = hp;
	pkt->state        = state;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_StrongholdState;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSZoneStatePacket(uint16 zoneId, uint8 state) {
	auto sendBuffer = SendBufferManager::open(sizeof(SZoneStatePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt     = bw.reserve<SZoneStatePacket>();
	pkt->zoneId  = zoneId;
	pkt->state   = state;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_ZoneState;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSNpcRespawnPacket(uint16 npcId, int32 newHp, DirectX::XMFLOAT3 spawnPos) {
	auto sendBuffer = SendBufferManager::open(sizeof(SNpcRespawnPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt      = bw.reserve<SNpcRespawnPacket>();
	pkt->npcId    = npcId;
	pkt->newHp    = newHp;
	pkt->spawnPos = spawnPos;

	pkt->size     = bw.writeSize();
	pkt->type     = PacketType::S_NpcRespawn;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}
