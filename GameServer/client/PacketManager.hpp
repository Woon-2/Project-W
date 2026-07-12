#ifndef client_packet_manager_hpp
#define client_packet_manager_hpp

#include "protocol.hpp"

namespace Online { class Game; }
class SendBuffer;

class PacketManager {
public:
	static void handlePacket(byte* buffer, int32 len);
	static void handleSEnterPacket(byte* buffer, int32 len);
	static void handleSEnterOtherPacket(byte* buffer, int32 len);
	static void handleSLeavePacket(byte* buffer, int32 len);
	static void handleSMovePacket(byte* buffer, int32 len);
	static void handleSMouseMovePacket(byte* buffer, int32 len);
	static void handleSNpcMovePacket(byte* buffer, int32 len);
	static void handleSNpcMoveBatchPacket(byte* buffer, int32 len);
	static void handleSNpcSpawnBatchPacket(byte* buffer, int32 len);
	static void handleSNpcBarrierPacket(byte* buffer, int32 len);
	static void handleSNpcHidePacket(byte* buffer, int32 len);
	static void handleSNpcAttackPacket(byte* buffer, int32 len);
	static void handleSPlayerAttackPacket(byte* buffer, int32 len);
	static void handleSHitPacket(byte* buffer, int32 len);
	static void handleSNpcRespawnPacket( byte* buffer, int32 len );
	static void handleSSkillStartPacket( byte* buffer, int32 len );
	static void handleSSkillHitPacket( byte* buffer, int32 len );
	static void handleSSkillChargePacket( byte* buffer, int32 len );
	static void handleSSkillSelectPacket( byte* buffer, int32 len );
	static void handleSSkillUseRejectPacket( byte* buffer, int32 len );
	static void handleSComboStatePacket( byte* buffer, int32 len );
	static void handleSPlayerHpPacket( byte* buffer, int32 len );
	static void handleSDebugHitboxPacket( byte* buffer, int32 len );
	static void handleSStrongholdStatePacket( byte* buffer, int32 len );
	static void handleSZoneStatePacket( byte* buffer, int32 len );
	static void handleSPlayerKnockbackPacket( byte* buffer, int32 len );
	static void handleSTimeSyncPacket( byte* buffer, int32 len );

	// 로비
	static void handleSCreateRoomPacket( byte* buffer, int32 len );
	static void handleSJoinRoomPacket( byte* buffer, int32 len );
	static void handleSLobbyRoomPlayerJoinedPacket( byte* buffer, int32 len );
	static void handleSLobbyRoomPlayerLeftPacket( byte* buffer, int32 len );
	static void handleSLobbyWeaponSelectedPacket( byte* buffer, int32 len );
	static void handleSGameStartPacket( byte* buffer, int32 len );

	static std::shared_ptr<SendBuffer> makeCMovePacket(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 velocity);
	static std::shared_ptr<SendBuffer> makeCDebugTeleportPacket(DirectX::XMFLOAT3 pos);
	static std::shared_ptr<SendBuffer> makeCMouseMovePacket(float yawRad);
	static std::shared_ptr<SendBuffer> makeCAttackPacket(uint64 actionServerMs);
	static std::shared_ptr<SendBuffer> makeCSkillStartPacket(uint32 skillAssetId, uint64 actionServerMs, uint32 skillSeed);
	static std::shared_ptr<SendBuffer> makeCSelectSkillPacket(uint8 slot);
	static std::shared_ptr<SendBuffer> makeCTimeSyncPacket(uint64 clientSendMs);

	static std::shared_ptr<SendBuffer> makeCEnterPacket(const std::string& lobbyCode, PlayerWeaponType weaponType);

	// 로비
	static std::shared_ptr<SendBuffer> makeCCreateRoomPacket();
	static std::shared_ptr<SendBuffer> makeCJoinRoomPacket(const std::string& code);
	static std::shared_ptr<SendBuffer> makeCLeaveRoomPacket();
	static std::shared_ptr<SendBuffer> makeCSelectWeaponPacket(PlayerWeaponType weaponType);
	static std::shared_ptr<SendBuffer> makeCGameStartPacket();
};

#endif // client_packet_manager_hpp
