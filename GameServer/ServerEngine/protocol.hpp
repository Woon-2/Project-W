#ifndef protocol_hpp
#define protocol_hpp

#include "macro.hpp"
#include "types.hpp"
#include "../common/mathUtil.hpp"

constexpr const char* serverIp = "127.0.0.1";
constexpr uint16 lobbyServerPort = 8888;
constexpr uint16 roomServerPort  = 9000;

enum class PacketType : uint16 {
	C_Enter,
	S_Enter,
	S_Enter_Other,

	S_Leave,

	C_Move,
	S_Move,

	C_MouseMove,
	S_MouseMove,

	S_NpcMove,
	S_NpcMoveBatch,

	C_Attack,
	S_NpcAttack,
	S_PlayerAttack,
	S_Hit,

	S_NpcRespawn,

	C_SkillStart,
	S_SkillStart,
	S_SkillHit,

	S_DebugHitbox,

	S_StrongholdState,

	S_ZoneState,

	// Lobby
	C_CreateRoom,
	S_CreateRoom,
	C_JoinRoom,
	S_JoinRoom,
	C_LeaveRoom,
	S_LobbyRoomPlayerJoined,
	S_LobbyRoomPlayerLeft,
	C_GameStart,
	S_GameStart,
};

enum class ObjectType : uint16 {
	Player,
	Goblin,
	Ground,
	Stronghold,
};

struct PacketHeader {
	uint16 size;
	PacketType type;
};

template<class T>
class DataList {
public:
	DataList() : data_(nullptr), cnt_(0u) {}
	DataList(T* data, uint16 cnt) : data_(data), cnt_(cnt) {}

	T& operator[](uint16 idx) {
		ASSERT_CRASH(idx < cnt_);
		return data_[idx];
	}

	uint16 count() const { return cnt_; }

private:
	T* data_;
	uint16 cnt_;
};

// Authoritative default player HP, shared by client and server so enter-time
// HP sync agrees on the same value.
constexpr int32 kPlayerMaxHp = 1'000'000;

#pragma pack(push, 1)

struct CEnterPacket : public PacketHeader {
	char lobbyCode[7];   // 로비에서 받은 방 코드(6자리 영숫자 + null). RoomServer가 방 그룹화에 사용.
};

struct PlayerInfo {
	uint16 playerId;
	uint16 materialSetIdx;
	int32 hp;
	int32 maxHp;
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 orient;
	DirectX::XMFLOAT3 scale;
	// 추후에 player 고유 정보 추가 필요
};

struct ObjectInfo {
	ObjectType type;
	uint16 objectId;
	uint16 materialSetIdx;
	int32 hp;
	int32 maxHp;
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 orient;
	DirectX::XMFLOAT3 scale;
};

struct SEnterPacket : public PacketHeader {
	PlayerInfo myInfo;

	uint16 objsOffset;	// objectInfo 배열의 시작 위치
	uint16 objCnt;

	using ObjectList = DataList<ObjectInfo>;

	ObjectList getObjectList() {
		byte* dataStart = reinterpret_cast<byte*>(this) + objsOffset;
		return ObjectList(reinterpret_cast<ObjectInfo*>(dataStart), objCnt);
	}
};

struct SEnterOtherPacket : public PacketHeader {
	PlayerInfo otherInfo;
};

struct SLeavePacket : public PacketHeader {
	uint16 playerId;
};

struct CMovePacket : public PacketHeader {
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 velocity;
};

struct SMovePacket : public PacketHeader {
	uint16 playerId;
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 velocity;
};

struct CMouseMovePacket : public PacketHeader {
	float yawRadian;
};

struct SMouseMovePacket : public PacketHeader {
	uint16 playerId;
	float yawRadian;
};

struct SNpcMovePacket : public PacketHeader {
	uint16 npcId;
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 orient;
	DirectX::XMFLOAT3 velocity;
};

struct SNpcMoveInfo {
	uint16              npcId;
	DirectX::XMFLOAT3   pos;
	DirectX::XMFLOAT4   orient;
	DirectX::XMFLOAT3   velocity;
};

struct SNpcMoveBatchPacket : public PacketHeader {
	uint16 dataOffset;
	uint16 npcCount;

	using NpcMoveList = DataList<SNpcMoveInfo>;
	NpcMoveList getNpcMoveList() {
		byte* dataStart = reinterpret_cast<byte*>(this) + dataOffset;
		return NpcMoveList(reinterpret_cast<SNpcMoveInfo*>(dataStart), npcCount);
	}
};

struct CAttackPacket : public PacketHeader {
	uint64 clientMs;
};

struct SNpcAttackPacket : public PacketHeader {
	uint16 npcId;
};

struct SPlayerAttackPacket : public PacketHeader {
	uint16 attackerId;
};

struct SHitPacket : public PacketHeader {
	uint16 targetId;
	int32  newHp;
};

struct SNpcRespawnPacket : public PacketHeader {
	uint16 npcId;
	int32 newHp;
	DirectX::XMFLOAT3 spawnPos;
};

struct CSkillStartPacket : public PacketHeader {
	uint32 skillAssetId;
	uint64 clientMs;
};

struct SSkillStartPacket : public PacketHeader {
	uint32 skillAssetId;
	uint16 ownerId;
	uint16 elapsedMs;
};

struct SSkillHitPacket : public PacketHeader {
	uint16              attackerId;
	uint16              targetId;
	int32               newHp;
	uint32              skillAssetId;
	DirectX::XMFLOAT3  targetVelocity;  // target's linear velocity at hit time; used for ragdoll impulse on kill
};

struct OBBInfo {
	DirectX::XMFLOAT3 center;
	DirectX::XMFLOAT3 halfExtents;
	DirectX::XMFLOAT4 orient;
};

// Stronghold structure state sync. HP-tick during damage rides on the existing
// S_Hit / S_SkillHit (target id based); this packet conveys destroy/rebuild
// transitions (and the post-rebuild full HP).
struct SStrongholdStatePacket : public PacketHeader {
	uint16 strongholdId;
	int32  hp;
	uint8  state;   // 0 = Alive, 1 = Destroyed
};

// Zone trigger state sync. Server-authoritative gameplay zones broadcast this
// when a zone-driven action needs the client to react cosmetically (e.g. arena
// barrier lock, cutscene start). The meaning of `state` is defined per zone tag
// by the bound handler. Boss spawns reuse the existing S_NpcRespawn packet.
struct SZoneStatePacket : public PacketHeader {
	uint16 zoneId;
	uint8  state;
};

struct SDebugHitboxPacket : public PacketHeader {
	uint16 dataOffset;
	uint16 obbCount;

	using OBBList = DataList<OBBInfo>;
	OBBList getOBBList() {
		byte* dataStart = reinterpret_cast<byte*>(this) + dataOffset;
		return OBBList(reinterpret_cast<OBBInfo*>(dataStart), obbCount);
	}
};

// --- Lobby ---

struct LobbyPlayerInfo {
	uint16 sessionId;
};

struct SCreateRoomPacket : public PacketHeader {
	uint16 myId;    // 수신자 본인의 sessionId
	char code[7];   // 6자리 영숫자 + null
};

struct CJoinRoomPacket : public PacketHeader {
	char code[7];
};

struct SJoinRoomPacket : public PacketHeader {
	bool   success;
	uint16 myId;            // 수신자 본인의 sessionId
	uint8  playerCnt;
	uint16 hostId;
	char   code[7];
	uint16 playersOffset;   // LobbyPlayerInfo 배열 시작 위치 (this 기준)

	using PlayerList = DataList<LobbyPlayerInfo>;
	PlayerList getPlayerList() {
		byte* start = reinterpret_cast<byte*>(this) + playersOffset;
		return PlayerList(reinterpret_cast<LobbyPlayerInfo*>(start), playerCnt);
	}
};

struct SLobbyRoomPlayerJoinedPacket : public PacketHeader {
	LobbyPlayerInfo info;
};

struct SLobbyRoomPlayerLeftPacket : public PacketHeader {
	uint16 sessionId;
};

struct SGameStartPacket : public PacketHeader {
	char   roomServerIp[16];
	uint16 roomServerPort;
	char   lobbyCode[7];
};

#pragma pack(pop)

#endif // protocol_hpp