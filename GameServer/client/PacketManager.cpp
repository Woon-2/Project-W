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

	case PacketType::S_DebugHitbox:
		handleSDebugHitboxPacket( buffer, len );
		break;

	case PacketType::S_StrongholdState:
		handleSStrongholdStatePacket( buffer, len );
		break;

	case PacketType::S_ZoneState:
		handleSZoneStatePacket( buffer, len );
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

	auto game = INet::ClientApp::onlineGame();
	game->setupPlayer(playerInfo);

	auto objList = enterPacket->getObjectList();
	
	for (int32 i = 0; i < objList.count(); ++i) {
		const auto& objInfo = objList[i];

		switch (objInfo.type) {
		case ObjectType::Player:
			game->createOtherPlayer(objInfo);
			break;

		case ObjectType::Goblin:
			game->createGoblin(objInfo);
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
	game->rotatePlayer(sMouseMvPkt->playerId, sMouseMvPkt->yawRadian);
}

void PacketManager::handleSNpcMovePacket(byte* buffer, int32 len) {
	auto sNpcMvPkt = reinterpret_cast<SNpcMovePacket*>(buffer);
	INet::ClientApp::onlineGame()->moveGoblin(sNpcMvPkt->npcId, sNpcMvPkt->pos, sNpcMvPkt->orient, sNpcMvPkt->velocity);
}

void PacketManager::handleSNpcMoveBatchPacket(byte* buffer, int32 len) {
	auto sNpcMvBatPkt = reinterpret_cast<SNpcMoveBatchPacket*>(buffer);
	auto list = sNpcMvBatPkt->getNpcMoveList();

	auto game = INet::ClientApp::onlineGame();

	for (uint16 i = 0; i < list.count(); ++i) {
		const auto& info = list[i];
		game->moveGoblin(info.npcId, info.pos, info.orient, info.velocity);
	}
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
	INet::ClientApp::onlineGame()->applyHit( sHitPkt->targetId, sHitPkt->newHp);
}

void PacketManager::handleSNpcRespawnPacket( byte* buffer, int32 len ) {
	auto sNpcRespawnPkt = reinterpret_cast<SNpcRespawnPacket*>(buffer);
	INet::ClientApp::onlineGame()->onNpcRespawn( sNpcRespawnPkt->npcId, sNpcRespawnPkt->newHp, sNpcRespawnPkt->spawnPos );
}

void PacketManager::handleSSkillStartPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SSkillStartPacket*>(buffer);
	INet::ClientApp::onlineGame()->onSkillStart( pkt->ownerId, pkt->skillAssetId, pkt->elapsedMs );
}

void PacketManager::handleSSkillHitPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SSkillHitPacket*>(buffer);
	INet::ClientApp::onlineGame()->onSkillHit( pkt->attackerId, pkt->targetId, pkt->newHp, pkt->skillAssetId, pkt->targetVelocity );
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

void PacketManager::handleSCreateRoomPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SCreateRoomPacket*>(buffer);
	std::string code( pkt->code, strnlen( pkt->code, sizeof( pkt->code ) ) );
	INet::ClientApp::onlineGame()->onLobbyCreated( code, pkt->myId );
}

void PacketManager::handleSJoinRoomPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SJoinRoomPacket*>(buffer);
	std::string code( pkt->code, strnlen( pkt->code, sizeof( pkt->code ) ) );

	std::vector<uint16> playerIds;
	auto list = pkt->getPlayerList();
	playerIds.reserve( list.count() );
	for ( uint16 i = 0; i < list.count(); ++i ) {
		playerIds.push_back( list[i].sessionId );
	}

	INet::ClientApp::onlineGame()->onLobbyJoined( pkt->success, pkt->hostId, pkt->myId, code, playerIds );
}

void PacketManager::handleSLobbyRoomPlayerJoinedPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SLobbyRoomPlayerJoinedPacket*>(buffer);
	INet::ClientApp::onlineGame()->onLobbyPlayerJoined( pkt->info.sessionId );
}

void PacketManager::handleSLobbyRoomPlayerLeftPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SLobbyRoomPlayerLeftPacket*>(buffer);
	INet::ClientApp::onlineGame()->onLobbyPlayerLeft( pkt->sessionId );
}

void PacketManager::handleSGameStartPacket( byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<SGameStartPacket*>(buffer);
	std::string ip( pkt->roomServerIp, strnlen( pkt->roomServerIp, sizeof( pkt->roomServerIp ) ) );
	std::string code( pkt->lobbyCode, strnlen( pkt->lobbyCode, sizeof( pkt->lobbyCode ) ) );
	INet::ClientApp::onlineGame()->onGameStart( ip, pkt->roomServerPort, code );
}

std::shared_ptr<SendBuffer> PacketManager::makeCSkillStartPacket(uint32 skillAssetId, uint64 clientMs) {
	auto sendBuffer = SendBufferManager::open(sizeof(CSkillStartPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<CSkillStartPacket>();
	pkt->skillAssetId = skillAssetId;
	pkt->clientMs     = clientMs;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_SkillStart;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCAttackPacket(uint64 clientMs) {
	auto sendBuffer = SendBufferManager::open(sizeof(CAttackPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto cAtkPkt = bw.reserve<CAttackPacket>();
	cAtkPkt->clientMs = clientMs;

	cAtkPkt->size = bw.writeSize();
	cAtkPkt->type = PacketType::C_Attack;

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

std::shared_ptr<SendBuffer> PacketManager::makeCMouseMovePacket(float yawRad) {
	auto sendBuffer = SendBufferManager::open(sizeof(CMouseMovePacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto cMouseMvPkt = bw.reserve<CMouseMovePacket>();
	cMouseMvPkt->yawRadian = yawRad;

	cMouseMvPkt->size = bw.writeSize();
	cMouseMvPkt->type = PacketType::C_MouseMove;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeCEnterPacket(const std::string& lobbyCode) {
	auto sendBuffer = SendBufferManager::open(sizeof(CEnterPacket));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<CEnterPacket>();
	strncpy_s(pkt->lobbyCode, lobbyCode.data(), _TRUNCATE);

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

std::shared_ptr<SendBuffer> PacketManager::makeCGameStartPacket() {
	auto sendBuffer = SendBufferManager::open(sizeof(PacketHeader));
	auto bw = BufferWriter(sendBuffer->data(), sendBuffer->allocSize());

	auto pkt = bw.reserve<PacketHeader>();
	pkt->size = bw.writeSize();
	pkt->type = PacketType::C_GameStart;

	sendBuffer->close(bw.writeSize());
	return sendBuffer;
}
