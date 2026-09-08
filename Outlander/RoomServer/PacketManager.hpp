#ifndef packet_manager_hpp
#define packet_manager_hpp

class SendBuffer;
class GameSession;
class Inventory;

/**
* @brief SingletonBase
*/
class PacketManager {
public:
	static void handlePacket(GameSession* session, byte* buffer, int32 len);
	static void handleCEnterPacket(GameSession* session, byte* buffer, int32 len);
	static void handleCMovePacket(GameSession* session, byte* buffer, int32 len);
	static void handleCDebugTeleportPacket(GameSession* session, byte* buffer, int32 len);
	static void handleCMouseMovePacket(GameSession* session, byte* buffer, int32 len);
	static void handleCAttackPacket(GameSession* session, byte* buffer, int32 len);
	static void handleCSkillStartPacket(GameSession* session, byte* buffer, int32 len);
	static void handleCSelectSkillPacket(GameSession* session, byte* buffer, int32 len);
	static void handleCTimeSyncPacket(GameSession* session, byte* buffer, int32 len);
	static void handleCInventoryActionPacket(GameSession* session, byte* buffer, int32 len);
	static void handleCItemPickupPacket(GameSession* session, byte* buffer, int32 len);

	static std::shared_ptr<SendBuffer> makeSEnterPacket(const PlayerInfo& playerInfo, const std::vector<ObjectInfo>& objInfos, const std::vector<PlayerNameInfo>& nameInfos);
	static std::shared_ptr<SendBuffer> makeSEnterOtherPacket(const PlayerInfo& playerInfo);
	static std::shared_ptr<SendBuffer> makeSLeavePacket(uint16 playerId);
	static std::shared_ptr<SendBuffer> makeSMovePacket(uint16 playerId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 velocity);
	static std::shared_ptr<SendBuffer> makeSMouseMovePacket(uint16 playerId, float yawRad, float pitchRad);
	static std::shared_ptr<SendBuffer> makeSNpcMovePacket(uint16 npcId, uint8 statusFlags, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 orient, DirectX::XMFLOAT3 velocity);
	static std::shared_ptr<SendBuffer> makeSNpcMoveBatchPacket(const std::vector<SNpcMoveInfo>& infos);
	static std::shared_ptr<SendBuffer> makeSNpcSpawnBatchPacket(const std::vector<ObjectInfo>& objInfos);
	static std::shared_ptr<SendBuffer> makeSNpcBarrierPacket(
		bool active,
		const std::vector<uint32>& npcIds,
		uint16 impulseOnlyNpcId = SNpcBarrierPacket::INVALID_NPC_ID);
	static std::shared_ptr<SendBuffer> makeSNpcHidePacket(const std::vector<uint32>& npcIds);
	static std::shared_ptr<SendBuffer> makeSNpcAttackPacket(uint16 npcId);
	static std::shared_ptr<SendBuffer> makeSPlayerAttackPacket(uint16 attackerId);
	static std::shared_ptr<SendBuffer> makeSHitPacket(uint16 attackerId, uint16 targetId, int32 newHp);
	static std::shared_ptr<SendBuffer> makeSNpcRespawnPacket(uint16 npcId, int32 newHp, DirectX::XMFLOAT3 spawnPos);
	static std::shared_ptr<SendBuffer> makeSStrongholdStatePacket(uint16 strongholdId, int32 hp, uint8 state);
	static std::shared_ptr<SendBuffer> makeSZoneStatePacket(uint16 zoneId, uint8 state);
	// castAnchorValid=0(기본)이면 앵커 필드는 무시되고 클라가 시전자 위치에서 castAnchor를 캡처한다
	// (기존 모든 스킬의 동작). 1이면 castAnchorX/Z가 앵커 위치를 대신한다 — 대상 지정형 지면 스킬용.
	static std::shared_ptr<SendBuffer> makeSSkillStartPacket(uint32 skillAssetId, uint16 ownerId, uint16 elapsedMs, uint32 skillSeed, float aimPitchRad,
		uint8 castAnchorValid = 0, float castAnchorX = 0.f, float castAnchorZ = 0.f);
	static std::shared_ptr<SendBuffer> makeSPlayerKnockbackPacket(uint16 playerId, float dirX, float dirZ, float speed, uint16 knockMs, uint16 postLockMs);
	static std::shared_ptr<SendBuffer> makeSSkillHitPacket(uint16 attackerId, uint16 targetId, int32 newHp, uint32 skillAssetId, DirectX::XMFLOAT3 targetVelocity, uint8 hitAnimIndex = 0);
	static std::shared_ptr<SendBuffer> makeSSkillSelectPacket(uint16 playerId, uint8 slot);
	static std::shared_ptr<SendBuffer> makeSSkillChargePacket(uint16 playerId, uint8 slot, float charge);
	static std::shared_ptr<SendBuffer> makeSSkillUseRejectPacket(uint8 slot);
	static std::shared_ptr<SendBuffer> makeSComboStatePacket(uint16 playerId, uint16 comboCount, float windowMs);
	static std::shared_ptr<SendBuffer> makeSPlayerHpPacket(uint16 playerId, int32 newHp);
	static std::shared_ptr<SendBuffer> makeSDebugHitboxPacket(const OBBInfo* obbs, uint16 count);
	static std::shared_ptr<SendBuffer> makeSTimeSyncPacket(uint64 clientSendMs, uint64 serverReceiveMs, uint64 serverSendMs);
	static std::shared_ptr<SendBuffer> makeSTacticalDialoguePacket(uint16 zoneId, TacticalDialogueId dialogueId);
	static std::shared_ptr<SendBuffer> makeSInventorySnapshotPacket(const Inventory& inventory);
	static std::shared_ptr<SendBuffer> makeSInventoryActionResultPacket(
		uint32 revision, uint8 slotIndex, InventoryAction action,
		InventoryActionResult result, InventorySlotInfo slot);
	static std::shared_ptr<SendBuffer> makeSItemDropBatchPacket(const std::vector<ItemDropInfo>& drops);
	static std::shared_ptr<SendBuffer> makeSItemDropRemovePacket(
		uint16 dropId, uint16 pickerObjId, ItemPickupResult result);
};

#endif // packet_manager_hpp
