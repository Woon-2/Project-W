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
	static void handleSTacticalDialoguePacket( byte* buffer, int32 len );
	static void handleSInventorySnapshotPacket(byte* buffer, int32 len);
	static void handleSInventoryActionResultPacket(byte* buffer, int32 len);
	static void handleSItemDropBatchPacket(byte* buffer, int32 len);
	static void handleSItemDropRemovePacket(byte* buffer, int32 len);

	// 계정
	static void handleSRegisterPacket( byte* buffer, int32 len );
	static void handleSLoginPacket( byte* buffer, int32 len );

	// 로비
	static void handleSCreateRoomPacket( byte* buffer, int32 len );
	static void handleSJoinRoomPacket( byte* buffer, int32 len );
	static void handleSLobbyRoomPlayerJoinedPacket( byte* buffer, int32 len );
	static void handleSLobbyRoomPlayerLeftPacket( byte* buffer, int32 len );
	static void handleSLobbyWeaponSelectedPacket( byte* buffer, int32 len );
	static void handleSGameStartPacket( byte* buffer, int32 len );

	static std::shared_ptr<SendBuffer> makeCMovePacket(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 velocity);
	static std::shared_ptr<SendBuffer> makeCDebugTeleportPacket(DirectX::XMFLOAT3 pos);
	static std::shared_ptr<SendBuffer> makeCMouseMovePacket(float yawRad, float pitchRad);
	static std::shared_ptr<SendBuffer> makeCAttackPacket(uint64 actionServerMs);
	static std::shared_ptr<SendBuffer> makeCSkillStartPacket(uint32 skillAssetId, uint64 actionServerMs, uint32 skillSeed, float aimPitchRad);
	static std::shared_ptr<SendBuffer> makeCSelectSkillPacket(uint8 slot);
	static std::shared_ptr<SendBuffer> makeCTimeSyncPacket(uint64 clientSendMs);
	static std::shared_ptr<SendBuffer> makeCInventoryActionPacket(
		uint32 revision, uint8 slotIndex, InventoryAction action);
	static std::shared_ptr<SendBuffer> makeCItemPickupPacket(uint16 dropId);

	static std::shared_ptr<SendBuffer> makeCEnterPacket(const EntryTicket& ticket, PlayerWeaponType weaponType);

	// 계정. loginId/password는 프로토콜상 ASCII(char[])라 호출부가 변환·검증을 끝낸 뒤 넘긴다.
	static std::shared_ptr<SendBuffer> makeCRegisterPacket(
		const std::string& loginId, const std::string& password, const std::wstring& nickname);
	static std::shared_ptr<SendBuffer> makeCLoginPacket(
		const std::string& loginId, const std::string& password);

	// 로비
	static std::shared_ptr<SendBuffer> makeCCreateRoomPacket();
	static std::shared_ptr<SendBuffer> makeCJoinRoomPacket(const std::string& code);
	static std::shared_ptr<SendBuffer> makeCLeaveRoomPacket();
	static std::shared_ptr<SendBuffer> makeCSelectWeaponPacket(PlayerWeaponType weaponType);
	static std::shared_ptr<SendBuffer> makeCGameStartPacket();
};

#endif // client_packet_manager_hpp
