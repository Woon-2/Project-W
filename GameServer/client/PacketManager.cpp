#include "pch.hpp"
#include "PacketManager.hpp"
#include "online/onlineGame.hpp"
#include "SendBuffer.hpp"
#include "BufferWriter.hpp"
#include "ClientApp.hpp"

void PacketManager::handlePacket(byte* buffer, int32 len) {
	auto header = reinterpret_cast<PacketHeader*>(buffer);
	
	switch (header->type) {
	case PacketType::S_Enter:
		handleSEnterPacket(buffer, len);
		break;

	case PacketType::S_Enter_Other:
		handleSEnterOtherPacket(buffer, len);
		break;

	case PacketType::S_Leave:
		handleSLeavePacket(buffer, len);
		break;

	case PacketType::S_Move:
		handleSMovePacket(buffer, len);
		break;

	case PacketType::S_MouseMove:
		handleSMouseMovePacket(buffer, len);
		break;

	case PacketType::S_NpcMove:
		handleSNpcMovePacket(buffer, len);
		break;

	case PacketType::S_NpcMoveBatch:
		handleSNpcMoveBatchPacket(buffer, len);
		break;

	case PacketType::S_NpcSpawnBatch:
		handleSNpcSpawnBatchPacket(buffer, len);
		break;

	case PacketType::S_NpcBarrier:
		handleSNpcBarrierPacket(buffer, len);
		break;

	case PacketType::S_NpcHide:
		handleSNpcHidePacket(buffer, len);
		break;

	case PacketType::S_NpcAttack:
		handleSNpcAttackPacket( buffer, len );
		break;

	case PacketType::S_PlayerAttack:
		handleSPlayerAttackPacket( buffer, len );
		break;

	case PacketType::S_Hit:
		handleSHitPacket(buffer, len);
		break;

	case PacketType::S_NpcRespawn:
		handleSNpcRespawnPacket( buffer, len );
		break;

	case PacketType::S_SkillStart:
		handleSSkillStartPacket( buffer, len );
		break;

	case PacketType::S_SkillHit:
		handleSSkillHitPacket( buffer, len );
		break;

	case PacketType::S_SkillCharge:
		handleSSkillChargePacket( buffer, len );
		break;

	case PacketType::S_SkillSelect:
		handleSSkillSelectPacket( buffer, len );
		break;

	case PacketType::S_SkillUseReject:
		handleSSkillUseRejectPacket( buffer, len );
		break;

	case PacketType::S_ComboState:
		handleSComboStatePacket( buffer, len );
		break;

	case PacketType::S_PlayerHp:
		handleSPlayerHpPacket( buffer, len );
		break;

	case PacketType::S_DebugHitbox:
		handleSDebugHitboxPacket( buffer, len );
		break;

	case PacketType::S_StrongholdState:
		handleSStrongholdStatePacket( buffer, len );
		break;

	case PacketType::S_ZoneState:
		handleSZoneStatePacket( buffer, len );
		break;

	case PacketType::S_PlayerKnockback:
		handleSPlayerKnockbackPacket( buffer, len );
		break;

	case PacketType::S_TimeSync:
		handleSTimeSyncPacket( buffer, len );
		break;

	case PacketType::S_TacticalDialogue:
		handleSTacticalDialoguePacket( buffer, len );
		break;

	case PacketType::S_InventorySnapshot:
		handleSInventorySnapshotPacket(buffer, len);
		break;

	case PacketType::S_InventoryActionResult:
		handleSInventoryActionResultPacket(buffer, len);
		break;

	case PacketType::S_CreateRoom:
		handleSCreateRoomPacket( buffer, len );
		break;

	case PacketType::S_JoinRoom:
		handleSJoinRoomPacket( buffer, len );
		break;

	case PacketType::S_LobbyRoomPlayerJoined:
		handleSLobbyRoomPlayerJoinedPacket( buffer, len );
		break;

	case PacketType::S_LobbyRoomPlayerLeft:
		handleSLobbyRoomPlayerLeftPacket( buffer, len );
		break;

	case PacketType::S_LobbyWeaponSelected:
		handleSLobbyWeaponSelectedPacket( buffer, len );
		break;

	case PacketType::S_GameStart:
		handleSGameStartPacket( buffer, len );
		break;

	default:
		std::cout << "Unknown packet type received. Type: " << static_cast<uint16>(header->type) << '\n';
		break;
	}
}

void PacketManager::handleSEnterPacket(byte* buffer, int32 len) {
	auto enterPacket = reinterpret_cast<SEnterPacket*>(buffer);
	auto playerInfo = enterPacket->myInfo;
	auto objList = enterPacket->getObjectList();

	auto game = INet::ClientApp::onlineGame();
	std::vector<uint16> existingPlayerIds;
	existingPlayerIds.reserve(static_cast<std::size_t>(objList.count()));
	for (int32 i = 0; i < objList.count(); ++i) {
		if (objList[i].type == ObjectType::Player) {
			existingPlayerIds.push_back(objList[i].objectId);
		}
	}
	game->prepareInGamePartyRoster(playerInfo.playerId, existingPlayerIds);
	game->setupPlayer(playerInfo);
	game->beginServerTimeSync();
	
	for (int32 i = 0; i < objList.count(); ++i) {
		const auto& objInfo = objList[i];

		switch (objInfo.type) {
		case ObjectType::Player:
			game->createOtherPlayer(objInfo);
			break;

		case ObjectType::Goblin:
			game->createGoblin(objInfo);
			break;

		case ObjectType::Hobgoblin:
			game->createHobgoblin(objInfo);
			break;

		case ObjectType::Snake:
			game->createSnake(objInfo);
			break;

		case ObjectType::Mushroom:
			game->createMushroom(objInfo);
			break;

		case ObjectType::Bomber:
			game->createBomber(objInfo);
			break;

		case ObjectType::Birdy:
			game->createBirdy(objInfo);
			break;

		case ObjectType::Slime:
			game->createSlime(objInfo);
			break;

		case ObjectType::Treant:
			game->createTreant(objInfo);
			break;

		case ObjectType::Grandbaum:
			game->createGrandbaum(objInfo);
			break;

		case ObjectType::Isys:
			game->createIsys(objInfo);
			break;

		case ObjectType::Boss:
			game->createBoss(objInfo);
			break;

		case ObjectType::Ground:
			game->setupGround(objInfo);
			break;

		case ObjectType::Stronghold:
			game->createStronghold(objInfo);
			break;

		default:
			std::cout << "Unknown object type received. Type: " << static_cast<uint16>(objInfo.type) << '\n';
			break;
		}
	}
}

void PacketManager::handleSEnterOtherPacket(byte* buffer, int32 len) {
	auto enterOtherPacket = reinterpret_cast<SEnterOtherPacket*>(buffer);
	auto otherPlayerInfo = enterOtherPacket->otherInfo;

	auto game = INet::ClientApp::onlineGame();
	game->createOtherPlayer(otherPlayerInfo);
}

void PacketManager::handleSLeavePacket(byte* buffer, int32 len) {
	auto leavePacket = reinterpret_cast<SLeavePacket*>(buffer);

	auto game = INet::ClientApp::onlineGame();
	game->removePlayer(leavePacket->playerId);
}

void PacketManager::handleSMovePacket(byte* buffer, int32 len) {
	auto sMvPkt = reinterpret_cast<SMovePacket*>(buffer);

	auto game = INet::ClientApp::onlineGame();
	game->movePlayer(sMvPkt->playerId, sMvPkt->pos, sMvPkt->velocity);
}

void PacketManager::handleSMouseMovePacket(byte* buffer, int32 len) {
	auto sMouseMvPkt = reinterpret_cast<SMouseMovePacket*>(buffer);

	auto game = INet::ClientApp::onlineGame();
	game->rotatePlayer(sMouseMvPkt->playerId, sMouseMvPkt->yawRadian, sMouseMvPkt->pitchRadian);
}

void PacketManager::handleSNpcMovePacket(byte* buffer, int32 len) {
	auto sNpcMvPkt = reinterpret_cast<SNpcMovePacket*>(buffer);
	INet::ClientApp::onlineGame()->moveGoblin(
		sNpcMvPkt->npcId, sNpcMvPkt->statusFlags,
		sNpcMvPkt->pos, sNpcMvPkt->orient, sNpcMvPkt->velocity);
}

void PacketManager::handleSNpcMoveBatchPacket(byte* buffer, int32 len) {
	auto sNpcMvBatPkt = reinterpret_cast<SNpcMoveBatchPacket*>(buffer);
	auto list = sNpcMvBatPkt->getNpcMoveList();

	auto game = INet::ClientApp::onlineGame();

	for (uint16 i = 0; i < list.count(); ++i) {
		const auto& info = list[i];
		game->moveGoblin(info.npcId, info.statusFlags, info.pos, info.orient, info.velocity);
	}
}

void PacketManager::handleSNpcSpawnBatchPacket(byte* buffer, int32 len) {
	auto pkt = reinterpret_cast<SNpcSpawnBatchPacket*>(buffer);
	auto objList = pkt->getObjectList();

	auto game = INet::ClientApp::onlineGame();

	for (uint16 i = 0; i < objList.count(); ++i) {
		const auto& objInfo = objList[i];

		switch (objInfo.type) {
		case ObjectType::Goblin:
			game->createGoblin(objInfo);
			break;

		case ObjectType::Hobgoblin:
			game->createHobgoblin(objInfo);
			break;

		case ObjectType::Snake:
			game->createSnake(objInfo);
			break;

		case ObjectType::Mushroom:
			game->createMushroom(objInfo);
			break;

		case ObjectType::Bomber:
			game->createBomber(objInfo);
			break;

		case ObjectType::Birdy:
			game->createBirdy(objInfo);
			break;

		case ObjectType::Slime:
			game->createSlime(objInfo);
			break;

		case ObjectType::Treant:
			game->createTreant(objInfo);
			break;

		case ObjectType::Grandbaum:
			game->createGrandbaum(objInfo);
			break;

		case ObjectType::Isys:
			game->createIsys(objInfo);
			break;

		case ObjectType::Boss:
			game->createBoss(objInfo);
			break;

		default:
			std::cout << "S_NpcSpawnBatch: unsupported object type " << static_cast<uint16>(objInfo.type) << '\n';
			break;
		}
	}
}

void PacketManager::handleSNpcBarrierPacket(byte* buffer, int32 len) {
	auto pkt = reinterpret_cast<SNpcBarrierPacket*>(buffer);
	auto list = pkt->getList();

	std::vector<uint16> ids;
	ids.reserve(list.count());
	for (uint16 i = 0; i < list.count(); ++i) {
		ids.push_back(list[i].npcId);
	}

	INet::ClientApp::onlineGame()->setNpcBarrier(
		pkt->active != 0, ids, pkt->impulseOnlyNpcId);
}

void PacketManager::handleSNpcHidePacket(byte* buffer, int32 len) {
	auto pkt = reinterpret_cast<SNpcHidePacket*>(buffer);
	auto list = pkt->getList();

	std::vector<uint16> ids;
	ids.reserve(list.count());
	for (uint16 i = 0; i < list.count(); ++i) {
		ids.push_back(list[i].npcId);
	}

	INet::ClientApp::onlineGame()->hideNpcs(ids);
}

void PacketManager::handleSNpcAttackPacket( byte* buffer, int32 len ) {
	auto sNpcAtkPkt = reinterpret_cast<SNpcAttackPacket*>(buffer);
	INet::ClientApp::onlineGame()->onNpcAttack( sNpcAtkPkt->npcId );
}

void PacketManager::handleSPlayerAttackPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SPlayerAttackPacket*>(buffer);
	INet::ClientApp::onlineGame()->onPlayerAttack( pkt->attackerId );
}

void PacketManager::handleSHitPacket(byte* buffer, int32 len) {
	auto sHitPkt = reinterpret_cast<SHitPacket*>(buffer);
	INet::ClientApp::onlineGame()->applyHit( sHitPkt->targetId, sHitPkt->newHp, sHitPkt->attackerId);
}

void PacketManager::handleSNpcRespawnPacket( byte* buffer, int32 len ) {
	auto sNpcRespawnPkt = reinterpret_cast<SNpcRespawnPacket*>(buffer);
	INet::ClientApp::onlineGame()->onNpcRespawn( sNpcRespawnPkt->npcId, sNpcRespawnPkt->newHp, sNpcRespawnPkt->spawnPos );
}

void PacketManager::handleSSkillStartPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SSkillStartPacket*>(buffer);
	INet::ClientApp::onlineGame()->onSkillStart( pkt->ownerId, pkt->skillAssetId, pkt->elapsedMs, pkt->skillSeed, pkt->aimPitchRadian );
}

void PacketManager::handleSTimeSyncPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<STimeSyncPacket*>(buffer);
	INet::ClientApp::onlineGame()->onServerTimeSync(
		pkt->clientSendMs, pkt->serverReceiveMs, pkt->serverSendMs);
}

void PacketManager::handleSSkillHitPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SSkillHitPacket*>(buffer);
	INet::ClientApp::onlineGame()->onSkillHit( pkt->attackerId, pkt->targetId, pkt->newHp, pkt->skillAssetId, pkt->targetVelocity, pkt->hitAnimIndex );
}

void PacketManager::handleSSkillChargePacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SSkillChargePacket*>(buffer);
	INet::ClientApp::onlineGame()->onSkillCharge( pkt->playerId, pkt->slot, pkt->charge );
}

void PacketManager::handleSSkillSelectPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SSkillSelectPacket*>(buffer);
	INet::ClientApp::onlineGame()->onSkillSelect( pkt->playerId, pkt->slot );
}

void PacketManager::handleSSkillUseRejectPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SSkillUseRejectPacket*>(buffer);
	INet::ClientApp::onlineGame()->onSkillUseReject( pkt->slot );
}

void PacketManager::handleSComboStatePacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SComboStatePacket*>(buffer);
	INet::ClientApp::onlineGame()->onComboState( pkt->playerId, pkt->comboCount, pkt->windowMs );
}

void PacketManager::handleSPlayerHpPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SPlayerHpPacket*>(buffer);
	INet::ClientApp::onlineGame()->onPlayerHp( pkt->playerId, pkt->newHp );
}

void PacketManager::handleSDebugHitboxPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SDebugHitboxPacket*>(buffer);
	INet::ClientApp::onlineGame()->onDebugHitboxes( pkt );
}

void PacketManager::handleSStrongholdStatePacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SStrongholdStatePacket*>(buffer);
	INet::ClientApp::onlineGame()->onStrongholdState( pkt->strongholdId, pkt->hp, pkt->state );
}

void PacketManager::handleSZoneStatePacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SZoneStatePacket*>(buffer);
	INet::ClientApp::onlineGame()->onZoneState( pkt->zoneId, pkt->state );
}

void PacketManager::handleSTacticalDialoguePacket( byte* buffer, int32 len ) {
	if (len < static_cast<int32>(sizeof(STacticalDialoguePacket))) return;

	auto pkt = reinterpret_cast<STacticalDialoguePacket*>(buffer);
	if (pkt->size != sizeof(STacticalDialoguePacket) ||
		static_cast<uint8>(pkt->dialogueId) >= static_cast<uint8>(TacticalDialogueId::Count)) {
		return;
	}

	if (auto* game = INet::ClientApp::onlineGame()) {
		game->onTacticalDialogue(pkt->zoneId, pkt->dialogueId);
	}
}

void PacketManager::handleSInventorySnapshotPacket(byte* buffer, int32 len) {
	if (len < sizeof(SInventorySnapshotPacket))
		return;
	auto* pkt = reinterpret_cast<SInventorySnapshotPacket*>(buffer);
	if (pkt->size != len)
		return;
	const std::size_t dataEnd = static_cast<std::size_t>(pkt->slotsOffset)
		+ sizeof(InventorySlotInfo) * pkt->slotCount;
	if (pkt->slotsOffset < sizeof(SInventorySnapshotPacket)
		|| dataEnd > static_cast<std::size_t>(len)) {
		return;
	}

	auto list = pkt->getSlotList();
	std::vector<InventorySlotInfo> slots;
	slots.reserve(list.count());
	for (uint16 i = 0; i < list.count(); ++i)
		slots.push_back(list[i]);
	INet::ClientApp::onlineGame()->onInventorySnapshot(pkt->revision, slots);
}

void PacketManager::handleSInventoryActionResultPacket(byte* buffer, int32 len) {
	if (len != sizeof(SInventoryActionResultPacket))
		return;
	const auto* pkt = reinterpret_cast<const SInventoryActionResultPacket*>(buffer);
	if (pkt->size != sizeof(SInventoryActionResultPacket)
		|| static_cast<uint8>(pkt->action) > static_cast<uint8>(InventoryAction::DiscardOne)
		|| static_cast<uint8>(pkt->result) > static_cast<uint8>(InventoryActionResult::StaleRevision)) {
		return;
	}
	INet::ClientApp::onlineGame()->onInventoryActionResult(
		pkt->revision, pkt->slotIndex, pkt->action, pkt->result, pkt->slot);
}

void PacketManager::handleSPlayerKnockbackPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SPlayerKnockbackPacket*>(buffer);
	INet::ClientApp::onlineGame()->onPlayerKnockback( pkt->playerId, pkt->dirX, pkt->dirZ, pkt->speed, pkt->knockMs, pkt->postLockMs );
}

void PacketManager::handleSCreateRoomPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SCreateRoomPacket*>(buffer);
	std::string code( pkt->code, strnlen( pkt->code, sizeof( pkt->code ) ) );
	INet::ClientApp::onlineGame()->onLobbyCreated( code, pkt->myId );
}

void PacketManager::handleSJoinRoomPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SJoinRoomPacket*>(buffer);
	std::string code( pkt->code, strnlen( pkt->code, sizeof( pkt->code ) ) );

	std::vector<LobbyPlayerInfo> playerInfos;
	auto list = pkt->getPlayerList();
	playerInfos.reserve( list.count() );
	for ( uint16 i = 0; i < list.count(); ++i ) {
		playerInfos.push_back( list[i] );
	}

	INet::ClientApp::onlineGame()->onLobbyJoined( pkt->success, pkt->hostId, pkt->myId, code, playerInfos );
}

void PacketManager::handleSLobbyRoomPlayerJoinedPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SLobbyRoomPlayerJoinedPacket*>(buffer);
	INet::ClientApp::onlineGame()->onLobbyPlayerJoined( pkt->info );
}

void PacketManager::handleSLobbyRoomPlayerLeftPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SLobbyRoomPlayerLeftPacket*>(buffer);
	INet::ClientApp::onlineGame()->onLobbyPlayerLeft( pkt->sessionId );
}

void PacketManager::handleSLobbyWeaponSelectedPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SLobbyWeaponSelectedPacket*>(buffer);
	INet::ClientApp::onlineGame()->onLobbyWeaponSelected( pkt->sessionId, pkt->weaponType );
}

void PacketManager::handleSGameStartPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SGameStartPacket*>(buffer);
	std::string ip( pkt->roomServerIp, strnlen( pkt->roomServerIp, sizeof( pkt->roomServerIp ) ) );
	std::string code( pkt->lobbyCode, strnlen( pkt->lobbyCode, sizeof( pkt->lobbyCode ) ) );
	INet::ClientApp::onlineGame()->onGameStart( ip, pkt->roomServerPort, code );
}

std::shared_ptr<SendBuffer> PacketManager::makeCSkillStartPacket(uint32 skillAssetId, uint64 actionServerMs, uint32 skillSeed, float aimPitchRad) {
	auto sendBuffer = SendBufferManager::open(sizeof(CSkillStartPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<CSkillStartPacket>();
	pkt->skillAssetId = skillAssetId;
	pkt->actionServerMs = actionServerMs;
	pkt->skillSeed    = skillSeed;
	pkt->aimPitchRadian = aimPitchRad;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_SkillStart;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCSelectSkillPacket(uint8 slot) {
	auto sendBuffer = SendBufferManager::open(sizeof(CSelectSkillPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<CSelectSkillPacket>();
	pkt->slot = slot;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_SelectSkill;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCAttackPacket(uint64 actionServerMs) {
	auto sendBuffer = SendBufferManager::open(sizeof(CAttackPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto cAtkPkt = bw.reserve<CAttackPacket>();
	cAtkPkt->actionServerMs = actionServerMs;

	cAtkPkt->size = bw.writeSize();
	cAtkPkt->type = PacketType::C_Attack;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCTimeSyncPacket(uint64 clientSendMs) {
	auto sendBuffer = SendBufferManager::open(sizeof(CTimeSyncPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<CTimeSyncPacket>();
	pkt->clientSendMs = clientSendMs;
	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_TimeSync;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCInventoryActionPacket(
	uint32 revision, uint8 slotIndex, InventoryAction action) {
	auto sendBuffer = SendBufferManager::open(sizeof(CInventoryActionPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto* pkt = bw.reserve<CInventoryActionPacket>();
	pkt->revision = revision;
	pkt->slotIndex = slotIndex;
	pkt->action = action;
	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_InventoryAction;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCMovePacket(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 velocity) {
	auto sendBuffer = SendBufferManager::open(sizeof(CMovePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto cMvPkt = bw.reserve<CMovePacket>();
	cMvPkt->pos = pos;
	cMvPkt->velocity = velocity;

	cMvPkt->size = bw.writeSize();
	cMvPkt->type = PacketType::C_Move;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCDebugTeleportPacket(DirectX::XMFLOAT3 pos) {
	auto sendBuffer = SendBufferManager::open(sizeof(CDebugTeleportPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<CDebugTeleportPacket>();
	pkt->pos = pos;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_DebugTeleport;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCMouseMovePacket(float yawRad, float pitchRad) {
	auto sendBuffer = SendBufferManager::open(sizeof(CMouseMovePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto cMouseMvPkt = bw.reserve<CMouseMovePacket>();
	cMouseMvPkt->yawRadian = yawRad;
	cMouseMvPkt->pitchRadian = pitchRad;

	cMouseMvPkt->size = bw.writeSize();
	cMouseMvPkt->type = PacketType::C_MouseMove;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCEnterPacket(const std::string& lobbyCode, PlayerWeaponType weaponType) {
	auto sendBuffer = SendBufferManager::open(sizeof(CEnterPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<CEnterPacket>();
	strncpy_s(pkt->lobbyCode, lobbyCode.data(), _TRUNCATE);
	pkt->weaponType = weaponType;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_Enter;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCCreateRoomPacket() {
	auto sendBuffer = SendBufferManager::open(sizeof(PacketHeader));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<PacketHeader>();
	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_CreateRoom;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCJoinRoomPacket(const std::string& code) {
	auto sendBuffer = SendBufferManager::open(sizeof(CJoinRoomPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<CJoinRoomPacket>();
	strncpy_s(pkt->code, code.data(), _TRUNCATE);

	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_JoinRoom;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCLeaveRoomPacket() {
	auto sendBuffer = SendBufferManager::open(sizeof(PacketHeader));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<PacketHeader>();
	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_LeaveRoom;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCSelectWeaponPacket(PlayerWeaponType weaponType) {
	auto sendBuffer = SendBufferManager::open(sizeof(CSelectWeaponPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<CSelectWeaponPacket>();
	pkt->weaponType = weaponType;
	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_SelectWeapon;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCGameStartPacket() {
	auto sendBuffer = SendBufferManager::open(sizeof(PacketHeader));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<PacketHeader>();
	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_GameStart;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}
