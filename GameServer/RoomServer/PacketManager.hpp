#ifndef packet_manager_hpp
#define packet_manager_hpp

class SendBuffer;
class GameSession;

/**
* @brief SingletonBase
*/
class PacketManager {
public:
	static void handlePacket(GameSession* session, byte* buffer, int32 len);
	static void handleCMovePacket(GameSession* session, byte* buffer, int32 len);
	static void handleCMouseMovePacket(GameSession* session, byte* buffer, int32 len);
	static void handleCAttackPacket(GameSession* session, byte* buffer, int32 len);
	static void handleCSkillStartPacket(GameSession* session, byte* buffer, int32 len);

	static std::shared_ptr<SendBuffer> makeSEnterPacket(const PlayerInfo& playerInfo, const std::vector<ObjectInfo>& objInfos);
	static std::shared_ptr<SendBuffer> makeSEnterOtherPacket(const PlayerInfo& playerInfo);
	static std::shared_ptr<SendBuffer> makeSLeavePacket(uint16 playerId);
	static std::shared_ptr<SendBuffer> makeSMovePacket(uint16 playerId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 velocity);
	static std::shared_ptr<SendBuffer> makeSMouseMovePacket(uint16 playerId, float yawRad);
	static std::shared_ptr<SendBuffer> makeSNpcMovePacket(uint16 npcId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 orient, DirectX::XMFLOAT3 velocity);
	static std::shared_ptr<SendBuffer> makeSNpcMoveBatchPacket(const std::vector<SNpcMoveInfo>& infos);
	static std::shared_ptr<SendBuffer> makeSNpcAttackPacket(uint16 npcId);
	static std::shared_ptr<SendBuffer> makeSPlayerAttackPacket(uint16 attackerId);
	static std::shared_ptr<SendBuffer> makeSHitPacket(uint16 targetId, int32 newHp);
	static std::shared_ptr<SendBuffer> makeSNpcRespawnPacket(uint16 npcId, int32 newHp, DirectX::XMFLOAT3 spawnPos);
	static std::shared_ptr<SendBuffer> makeSSkillStartPacket(uint32 skillAssetId, uint16 ownerId, uint16 elapsedMs);
	static std::shared_ptr<SendBuffer> makeSSkillHitPacket(uint16 attackerId, uint16 targetId, int32 newHp, uint32 skillAssetId, DirectX::XMFLOAT3 targetVelocity);
	static std::shared_ptr<SendBuffer> makeSDebugHitboxPacket(const OBBInfo* obbs, uint16 count);
};

#endif // packet_manager_hpp