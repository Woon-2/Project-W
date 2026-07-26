#include "rspch.hpp"
#include "PacketManager.hpp"
#include "SendBuffer.hpp"
#include "BufferWriter.hpp"
#include "GameSession.hpp"
#include "ObjectPool.hpp"
#include "Room.hpp"
#include "EntryTicket.hpp"
#include "AccountRegistry.hpp"

namespace {
uint64 networkNowMs() {
	return static_cast<uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

void PacketManager::handlePacket(GameSession* session, byte* buffer, int32 len) {
	auto header = reinterpret_cast<PacketHeader*>(buffer);

	// 입장 게이트: C_Enter로 티켓을 검증하기 전에는 다른 패킷을 처리하지 않는다.
	// 보안 목적이자 널 방어다 — 아래 핸들러 대부분이 session->room()을 널 체크 없이 역참조하므로,
	// 게이트가 없으면 C_Enter 없이 C_Move 하나만 보내도 서버가 죽는다.
	if (header->type != PacketType::C_Enter && !session->isEntryAuthorized()) {
		std::cout << "Unauthorized packet dropped. type: " << static_cast<uint16>(header->type)
			<< " session: " << session->id() << '\n';
		return;
	}

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

	case PacketType::C_TimeSync:
		handleCTimeSyncPacket(session, buffer, len);
		break;

	case PacketType::C_InventoryAction:
		handleCInventoryActionPacket(session, buffer, len);
		break;

	default:
		std::cout << "Unknown packet type received. Type: " << static_cast<uint16>(header->type) << '\n';
		break;
	}
}

void PacketManager::handleCEnterPacket(GameSession* session, byte* buffer, int32 len) {
	if (len != sizeof(CEnterPacket)) {
		std::cout << "[EntryTicket] 잘못된 C_Enter 크기. len: " << len
			<< " session: " << session->id() << '\n';
		session->disconnect("malformed C_Enter");
		return;
	}

	// 중복 C_Enter는 무시한다. 두 번째를 처리하면 계정 바인딩이 꼬이고 방에 두 번 들어간다.
	if (session->isEntryAuthorized() || session->room() != nullptr) {
		return;
	}

	auto pkt = reinterpret_cast<CEnterPacket*>(buffer);

	const EntryTicketResult verdict = EntryTicketAuthority::verify(pkt->ticket);
	if (verdict != EntryTicketResult::Ok) {
		std::cout << "[EntryTicket] 거부(" << EntryTicketAuthority::toString(verdict)
			<< ") session: " << session->id() << '\n';
		session->disconnect("invalid entry ticket");
		return;
	}

	const EntryTicketPayload& tp = pkt->ticket.payload;

	// 계정당 룸서버 세션 하나. 로비는 핸드오프로 소켓이 닫히는 순간 계정 바인딩을 풀기 때문에
	// (LobbyServer/GameSession.cpp onDisconnected) 중복 입장 차단은 룸서버가 자체적으로 해야 한다.
	if (!AccountRegistry::bind(tp.accountId, static_cast<uint16>(session->id()))) {
		std::cout << "[EntryTicket] 계정 중복 입장 거부. accountId: " << tp.accountId
			<< " session: " << session->id() << '\n';
		session->disconnect("account already in a room");
		return;
	}

	session->setAccount(tp.accountId, tp.nickname);

	std::wcout << L"[EntryTicket] ok accountId=" << tp.accountId
		<< L" nickname=" << session->nickname()
		<< L" session=" << session->id() << L'\n';

	const auto ordinal = static_cast<uint8>(pkt->weaponType);
	session->player()->setWeaponType(
		ordinal <= static_cast<uint8>(PlayerWeaponType::HeavyArrow)
			? pkt->weaponType
			: PlayerWeaponType::Katana);

	// 방 배정의 권위 출처는 티켓 안의 코드다(바깥 사본은 존재하지 않는다).
	std::string code(tp.lobbyCode, strnlen_s(tp.lobbyCode, sizeof(tp.lobbyCode)));
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
	cMouseMvPktClone->pitchRadian = clientMouseMovePacket->pitchRadian;

	session->room()->doAsync([session, cMouseMvPktClone]() {
		session->room()->rotate(session->id(), cMouseMvPktClone);
	});
}

void PacketManager::handleCAttackPacket( GameSession* session, byte* buffer, int32 len ) {
	uint64 actionServerMs = reinterpret_cast<CAttackPacket*>(buffer)->actionServerMs;
	session->room()->doAsync( [ session, actionServerMs ]() {
		session->room()->attack( session->id(), actionServerMs );
	} );
}

void PacketManager::handleCSkillStartPacket( GameSession* session, byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<CSkillStartPacket*>(buffer);
	uint32 skillAssetId = pkt->skillAssetId;
	uint64 actionServerMs = pkt->actionServerMs;
	uint32 skillSeed    = pkt->skillSeed;
	float  aimPitchRad  = pkt->aimPitchRadian;
	session->room()->doAsync( [session, skillAssetId, actionServerMs, skillSeed, aimPitchRad]() {
		session->room()->skillStart( session->id(), skillAssetId, actionServerMs, skillSeed, aimPitchRad );
	} );
}

void PacketManager::handleCTimeSyncPacket(GameSession* session, byte* buffer, int32 len) {
	auto pkt = reinterpret_cast<CTimeSyncPacket*>(buffer);
	const uint64 serverReceiveMs = networkNowMs();
	const uint64 serverSendMs = networkNowMs();
	session->send(makeSTimeSyncPacket(pkt->clientSendMs, serverReceiveMs, serverSendMs));
}

void PacketManager::handleCSelectSkillPacket( GameSession* session, byte* buffer, int32 len ) {
	uint8 slot = reinterpret_cast<CSelectSkillPacket*>(buffer)->slot;
	session->room()->doAsync( [session, slot]() {
		session->room()->selectSkill( session->id(), slot );
	} );
}

void PacketManager::handleCInventoryActionPacket(GameSession* session, byte* buffer, int32 len) {
	if (!session || !session->room() || len != sizeof(CInventoryActionPacket))
		return;

	const auto* pkt = reinterpret_cast<const CInventoryActionPacket*>(buffer);
	if (static_cast<uint8>(pkt->action) > static_cast<uint8>(InventoryAction::DiscardOne))
		return;

	const uint32 revision = pkt->revision;
	const uint8 slotIndex = pkt->slotIndex;
	const InventoryAction action = pkt->action;
	session->room()->doAsync([session, revision, slotIndex, action]() {
		if (session->room())
			session->room()->inventoryAction(session->id(), revision, slotIndex, action);
	});
}

std::shared_ptr<SendBuffer> PacketManager::makeSEnterPacket(const PlayerInfo& playerInfo, const std::vector<ObjectInfo>& objInfos, const std::vector<PlayerNameInfo>& nameInfos) {
	int32 objCnt = static_cast<int32>(objInfos.size());
	int32 nameCnt = static_cast<int32>(nameInfos.size());
	auto sendBuffer = SendBufferManager::open(
		sizeof(SEnterPacket) + sizeof(ObjectInfo) * objCnt + sizeof(PlayerNameInfo) * nameCnt);
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto enterPacket = bw.reserve<SEnterPacket>();
	enterPacket->myInfo = playerInfo;
	enterPacket->objCnt = objCnt;
	enterPacket->nameCnt = static_cast<uint8>(nameCnt);

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

	auto names = bw.reserve<PlayerNameInfo>(nameCnt);
	for (int32 i = 0; i < nameCnt; ++i) {
		names[i] = nameInfos[i];
	}

	enterPacket->objsOffset = static_cast<uint16>(reinterpret_cast<uint64>(infos) - reinterpret_cast<uint64>(enterPacket));
	enterPacket->namesOffset = static_cast<uint16>(reinterpret_cast<uint64>(names) - reinterpret_cast<uint64>(enterPacket));
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

std::shared_ptr<SendBuffer> PacketManager::makeSMouseMovePacket(uint16 playerId, float yawRad, float pitchRad) {
	auto sendBuffer = SendBufferManager::open(sizeof(SMouseMovePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto sMouseMvPkt = bw.reserve<SMouseMovePacket>();
	sMouseMvPkt->playerId = playerId;
	sMouseMvPkt->yawRadian = yawRad;
	sMouseMvPkt->pitchRadian = pitchRad;

	sMouseMvPkt->size = bw.writeSize();
	sMouseMvPkt->type = PacketType::S_MouseMove;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSNpcMovePacket(uint16 npcId, uint8 statusFlags, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 orient, DirectX::XMFLOAT3 velocity) {
	auto sendBuffer = SendBufferManager::open(sizeof(SNpcMovePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto sNpcMvPkt = bw.reserve<SNpcMovePacket>();
	sNpcMvPkt->npcId = npcId;
	sNpcMvPkt->statusFlags = statusFlags;
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

std::shared_ptr<SendBuffer> PacketManager::makeSNpcBarrierPacket(
	bool active,
	const std::vector<uint32>& npcIds,
	uint16 impulseOnlyNpcId) {
	const uint16 count = static_cast<uint16>(npcIds.size());
	auto sendBuffer = SendBufferManager::open(sizeof(SNpcBarrierPacket) + sizeof(SNpcBarrierInfo) * count);
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<SNpcBarrierPacket>();

	auto entries = bw.reserve<SNpcBarrierInfo>(count);
	for (uint16 i = 0; i < count; ++i) {
		entries[i].npcId = static_cast<uint16>(npcIds[i]);
	}

	pkt->active           = active ? 1 : 0;
	pkt->impulseOnlyNpcId = impulseOnlyNpcId;
	pkt->dataOffset       = static_cast<uint16>(reinterpret_cast<uint64>(entries) - reinterpret_cast<uint64>(pkt));
	pkt->npcCount         = count;

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


std::shared_ptr<SendBuffer> PacketManager::makeSSkillStartPacket(uint32 skillAssetId, uint16 ownerId, uint16 elapsedMs, uint32 skillSeed, float aimPitchRad,
	uint8 castAnchorValid, float castAnchorX, float castAnchorZ) {
	auto sendBuffer = SendBufferManager::open(sizeof(SSkillStartPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt          = bw.reserve<SSkillStartPacket>();
	pkt->skillAssetId = skillAssetId;
	pkt->ownerId      = ownerId;
	pkt->elapsedMs    = elapsedMs;
	pkt->skillSeed    = skillSeed;
	pkt->aimPitchRadian = aimPitchRad;
	pkt->castAnchorValid = castAnchorValid;
	pkt->castAnchorX     = castAnchorX;
	pkt->castAnchorZ     = castAnchorZ;
	pkt->size         = bw.writeSize();
	pkt->type         = PacketType::S_SkillStart;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSTimeSyncPacket(uint64 clientSendMs,
	                                                            uint64 serverReceiveMs,
	                                                            uint64 serverSendMs) {
	auto sendBuffer = SendBufferManager::open(sizeof(STimeSyncPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<STimeSyncPacket>();
	pkt->clientSendMs = clientSendMs;
	pkt->serverReceiveMs = serverReceiveMs;
	pkt->serverSendMs = serverSendMs;
	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_TimeSync;

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
                                                                DirectX::XMFLOAT3 targetVelocity,
                                                                uint8 hitAnimIndex) {
	auto sendBuffer = SendBufferManager::open(sizeof(SSkillHitPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt              = bw.reserve<SSkillHitPacket>();
	pkt->attackerId       = attackerId;
	pkt->targetId         = targetId;
	pkt->newHp            = newHp;
	pkt->skillAssetId     = skillAssetId;
	pkt->targetVelocity   = targetVelocity;
	pkt->hitAnimIndex     = hitAnimIndex;
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

std::shared_ptr<SendBuffer> PacketManager::makeSTacticalDialoguePacket(
	uint16 zoneId, TacticalDialogueId dialogueId) {
	auto sendBuffer = SendBufferManager::open(sizeof(STacticalDialoguePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt        = bw.reserve<STacticalDialoguePacket>();
	pkt->zoneId     = zoneId;
	pkt->dialogueId = dialogueId;
	pkt->size       = bw.writeSize();
	pkt->type       = PacketType::S_TacticalDialogue;

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

std::shared_ptr<SendBuffer> PacketManager::makeSInventorySnapshotPacket(
	const Inventory& inventory) {
	const uint8 count = static_cast<uint8>(inventory.slotCount());
	auto sendBuffer = SendBufferManager::open(
		sizeof(SInventorySnapshotPacket) + sizeof(InventorySlotInfo) * count);
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto* pkt = bw.reserve<SInventorySnapshotPacket>();
	auto* entries = bw.reserve<InventorySlotInfo>(count);
	for (uint8 i = 0; i < count; ++i) {
		const ItemStack* stack = inventory.slot(i);
		entries[i] = stack
			? InventorySlotInfo{ stack->itemId, stack->quantity }
			: InventorySlotInfo{};
	}

	pkt->revision = inventory.revision();
	pkt->slotsOffset = static_cast<uint16>(
		reinterpret_cast<uint64>(entries) - reinterpret_cast<uint64>(pkt));
	pkt->slotCount = count;
	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_InventorySnapshot;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSInventoryActionResultPacket(
	uint32 revision, uint8 slotIndex, InventoryAction action,
	InventoryActionResult result, InventorySlotInfo slot) {
	auto sendBuffer = SendBufferManager::open(sizeof(SInventoryActionResultPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto* pkt = bw.reserve<SInventoryActionResultPacket>();
	pkt->revision = revision;
	pkt->slotIndex = slotIndex;
	pkt->action = action;
	pkt->result = result;
	pkt->slot = slot;
	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_InventoryActionResult;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}
