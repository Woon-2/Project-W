#ifndef protocol_hpp
#define protocol_hpp

#include "macro.hpp"
#include "types.hpp"
#include "../common/mathUtil.hpp"

enum class PacketType : uint16 {
	C_Enter,
	S_Enter,
	S_Enter_Other,

	S_Leave,

	C_Move,
	S_Move,

	C_DebugTeleport,   // 디버그 전용: 안티치트 클램프를 우회해 플레이어를 임의 좌표로 이동(아레나 텔레포트 테스트)

	C_MouseMove,
	S_MouseMove,

	S_NpcMove,
	S_NpcMoveBatch,
	S_NpcSpawnBatch,
	S_NpcBarrier,

	C_Attack,
	S_NpcAttack,
	S_PlayerAttack,
	S_Hit,

	S_NpcRespawn,
	S_NpcHide,

	C_SkillStart,
	S_SkillStart,
	S_SkillHit,

	C_SelectSkill,
	S_SkillSelect,
	S_SkillCharge,
	S_SkillUseReject,
	S_ComboState,

	S_PlayerHp,   // server-authoritative player HP push (regen); no hit animation

	S_DebugHitbox,

	S_StrongholdState,

	S_ZoneState,

	S_PlayerKnockback,

	// Lobby
	C_CreateRoom,
	S_CreateRoom,
	C_JoinRoom,
	S_JoinRoom,
	C_LeaveRoom,
	S_LobbyRoomPlayerJoined,
	S_LobbyRoomPlayerLeft,
	C_SelectWeapon,
	S_LobbyWeaponSelected,
	C_GameStart,
	S_GameStart,

	// Append-only: room-server monotonic clock synchronization.
	C_TimeSync,
	S_TimeSync,

	// Append-only: account system (register/login).
	C_Register,
	S_Register,
	C_Login,
	S_Login,
};

enum class ObjectType : uint16 {
	Player,
	Goblin,
	Ground,
	Stronghold,
	Snake,
	Mushroom,
	Hobgoblin,
	// Append-only (ordinals are wire values shared by client + server).
	Bomber,
	Birdy,
	Slime,
	Treant,
	Grandbaum,   // named variant of Treant (shares Treant anims, different model)
	Isys,        // named variant of Birdy  (shares Birdy anims, different model)
	Boss,        // final boss: 1:1 combat (NOT tactical), own 14-clip rig
};

enum class PlayerWeaponType : uint8 {
	Katana,
	SpearHook,
	CrystalWand,
	HeavyArrow,
};

// Append-only (서수가 클라·서버 공유 wire 값이므로 중간 삽입 금지).
enum class AccountResult : uint8 {
	Ok,
	InvalidInput,        // 빈 값이거나 허용 길이 초과 등 형식 오류
	DuplicateId,         // 가입: 이미 존재하는 ID
	NoSuchAccount,       // 로그인: 없는 ID
	WrongPassword,       // 로그인: 비밀번호 불일치
	AlreadyLoggedIn,     // 로그인: 이미 접속 중인 계정
	DbError,             // 서버 내부 오류 (클라이언트는 "잠시 후 다시 시도" 안내)
	DuplicateNickname,   // 가입: 이미 사용 중인 닉네임. ID·닉네임 둘 다 중복이면 DuplicateId를 먼저 응답한다
};

// 계정 문자열 최대 크기(널 종료 포함). 클라 입력 UI와 DB 스키마(NVARCHAR)가 이 값에 맞춘다.
constexpr int32 kLoginIdMax = 24;     // ASCII
constexpr int32 kPasswordMax = 32;    // ASCII
constexpr int32 kNicknameMax = 16;    // wchar_t — 한글 지원

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
constexpr int32 kPlayerMaxHp = 5000;

#pragma pack(push, 1)

struct CEnterPacket : public PacketHeader {
	char lobbyCode[7];   // 로비에서 받은 방 코드(6자리 영숫자 + null). RoomServer가 방 그룹화에 사용.
	PlayerWeaponType weaponType;
};

struct PlayerInfo {
	uint16 playerId;
	uint16 materialSetIdx;
	PlayerWeaponType weaponType;
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
	PlayerWeaponType weaponType;
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

// 디버그 전용 텔레포트. 서버가 C_Move의 7m/패킷 클램프를 우회해 플레이어를 pos로 즉시 이동시킨다
// (아레나 zone 트리거 테스트용). 이후 S_Move로 다른 클라에 전파된다.
struct CDebugTeleportPacket : public PacketHeader {
	DirectX::XMFLOAT3 pos;
};

struct SMovePacket : public PacketHeader {
	uint16 playerId;
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 velocity;
};

// 서버→클라 강제 넉백 명령(Grandbaum ShieldWall). 플레이어 이동은 클라 권한이므로 서버가 위치를
// 강제할 수 없다 → 클라가 이 명령을 받아 로컬에서 knockMs 동안 dir·speed로 밀려나고, 이어
// postLockMs 동안 입력을 잠근다(그 위치를 계속 C_Move로 서버에 반영).
struct SPlayerKnockbackPacket : public PacketHeader {
	uint16 playerId;
	float  dirX;
	float  dirZ;
	float  speed;
	uint16 knockMs;      // 넉백(강제 이동) 지속(ms)
	uint16 postLockMs;   // 넉백 후 입력잠금(ms)
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

// 런타임에 동적으로 스폰된 NPC들을 클라이언트에 통보. 진입 스냅샷(S_Enter)과
// 동일한 ObjectInfo 리스트 직렬화를 재사용하며, 클라이언트는 각 항목을
// S_Enter와 같은 방식(ObjectType 분기)으로 생성한다.
struct SNpcSpawnBatchPacket : public PacketHeader {
	uint16 dataOffset;   // ObjectInfo 배열 시작 위치 (this 기준)
	uint16 objCnt;

	using ObjectList = DataList<ObjectInfo>;
	ObjectList getObjectList() {
		byte* dataStart = reinterpret_cast<byte*>(this) + dataOffset;
		return ObjectList(reinterpret_cast<ObjectInfo*>(dataStart), objCnt);
	}
};

// 전술 차단벽(barrier) 토글: 지정한 NPC들을 클라에서 '플레이어를 막는 벽'으로
// 설정/해제한다. 클라는 active NPC에 대해 position 기반 분리로 로컬 플레이어를 밀어낸다.
struct SNpcBarrierInfo {
	uint16 npcId;
};

struct SNpcBarrierPacket : public PacketHeader {
	static constexpr uint16 INVALID_NPC_ID = 0xFFFFu;

	uint8  active;       // 1 = 벽 켜기, 0 = 끄기
	uint16 impulseOnlyNpcId; // 실제 barrier에는 포함하지 않고 피격 impulse만 차단할 NPC
	uint16 dataOffset;
	uint16 npcCount;

	using NpcBarrierList = DataList<SNpcBarrierInfo>;
	NpcBarrierList getList() {
		byte* dataStart = reinterpret_cast<byte*>(this) + dataOffset;
		return NpcBarrierList(reinterpret_cast<SNpcBarrierInfo*>(dataStart), npcCount);
	}
};

struct CAttackPacket : public PacketHeader {
	uint64 actionServerMs;   // client-estimated server monotonic time; server validates before use
};

struct SNpcAttackPacket : public PacketHeader {
	uint16 npcId;
};

struct SPlayerAttackPacket : public PacketHeader {
	uint16 attackerId;
};

struct SHitPacket : public PacketHeader {
	uint16 attackerId;
	uint16 targetId;
	int32  newHp;
};

struct SNpcRespawnPacket : public PacketHeader {
	uint16 npcId;
	int32 newHp;
	DirectX::XMFLOAT3 spawnPos;
};

// 지정한 NPC들을 클라에서 즉시 '숨김'(비표시) 처리한다. 사망(S_Hit→시체/래그돌)과 달리
// 죽는 연출 없이 화면에서 제거만 하며, 복귀는 기존 S_NpcRespawn이 hidden을 해제해 재등장시킨다.
// (그랜드밤 방패벽: 후퇴한 원본 뱀 부대를 웨이브 동안 전장에서 퇴장시키는 용도.)
struct SNpcHideInfo {
	uint16 npcId;
};

struct SNpcHidePacket : public PacketHeader {
	uint16 dataOffset;
	uint16 npcCount;

	using NpcHideList = DataList<SNpcHideInfo>;
	NpcHideList getList() {
		byte* dataStart = reinterpret_cast<byte*>(this) + dataOffset;
		return NpcHideList(reinterpret_cast<SNpcHideInfo*>(dataStart), npcCount);
	}
};

struct CSkillStartPacket : public PacketHeader {
	uint32 skillAssetId;
	uint64 actionServerMs;   // client-estimated server monotonic time; server validates before use
	uint32 skillSeed;   // caster-generated per-cast seed (deterministic particle hitboxes)
};

struct SSkillStartPacket : public PacketHeader {
	uint32 skillAssetId;
	uint16 ownerId;
	uint16 elapsedMs;
	uint32 skillSeed;   // relayed caster seed; remote clients reproduce identical VFX params
};

struct SSkillHitPacket : public PacketHeader {
	uint16              attackerId;
	uint16              targetId;
	int32               newHp;
	uint32              skillAssetId;
	DirectX::XMFLOAT3  targetVelocity;  // target's linear velocity at hit time; used for ragdoll impulse on kill
	uint8               hitAnimIndex;   // which hit-reaction clip the target should play (server-chosen; multi-hit rigs like Boss). 0 for single-hit monsters.
};

// --- Stack-charge skill system ---

// Client -> server: the player rotated the dial to a new selected slot (0..2).
// Server stores it; monster-kill charge is credited to the player's selected slot.
struct CSelectSkillPacket : public PacketHeader {
	uint8 slot;
};

// Server -> other clients: relay a player's selected slot (teammate HUD mirror).
struct SSkillSelectPacket : public PacketHeader {
	uint16 playerId;
	uint8  slot;
};

// Server -> all clients: authoritative per-slot charge for a player. Clients
// derive stacks (= floor(charge / chargeCost)) and the fill fraction from the
// skill asset. Sent on kill-credit and on cast consumption.
struct SSkillChargePacket : public PacketHeader {
	uint16 playerId;
	uint8  slot;
	float  charge;
};

// Server -> caster only: a wheel-click cast was rejected (insufficient stack or
// still on cooldown) after the client already played it locally; caster rolls back.
struct SSkillUseRejectPacket : public PacketHeader {
	uint8 slot;
};

// Server -> caster: consecutive-kill combo state driving the combo UI. windowMs
// is the full combo window so the client renders the local countdown.
struct SComboStatePacket : public PacketHeader {
	uint16 playerId;
	uint16 comboCount;
	float  windowMs;
};

// Server -> all: authoritative player HP value (driven by server-side regen).
// Applied directly (setHp) on the client without any hit event/animation, so the
// HP bar/text reflect continuous regen via the existing per-frame read.
struct SPlayerHpPacket : public PacketHeader {
	uint16 playerId;
	int32  newHp;
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
	PlayerWeaponType weaponType;
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

struct CSelectWeaponPacket : public PacketHeader {
	PlayerWeaponType weaponType;
};

struct SLobbyWeaponSelectedPacket : public PacketHeader {
	uint16 sessionId;
	PlayerWeaponType weaponType;
};

struct SGameStartPacket : public PacketHeader {
	char   roomServerIp[16];
	uint16 roomServerPort;
	char   lobbyCode[7];
};

// Four-timestamp clock synchronization. The client owns t0/t3 and the room
// server returns t1/t2, allowing an NTP-style offset estimate without assuming
// that steady_clock has the same epoch on different PCs.
struct CTimeSyncPacket : public PacketHeader {
	uint64 clientSendMs;      // t0
};

struct STimeSyncPacket : public PacketHeader {
	uint64 clientSendMs;      // t0 (echo)
	uint64 serverReceiveMs;   // t1
	uint64 serverSendMs;      // t2
};

// --- 계정 시스템 (가입/로그인, 로비서버 전용) ---
// 흐름: 접속 직후 C_Register 또는 C_Login을 보낸다. 로그인 성공(S_Login: Ok) 전에는
// 방 생성 등 다른 C_* 패킷이 서버에서 무시된다(인증 게이트).
// 문자열은 전부 널 종료 고정 배열이다. 비밀번호는 평문으로 전송되고 서버가 해시해 저장한다.

struct CRegisterPacket : public PacketHeader {
	char loginId[kLoginIdMax];
	char password[kPasswordMax];
	wchar_t nickname[kNicknameMax];
};

struct SRegisterPacket : public PacketHeader {
	AccountResult result;
};

struct CLoginPacket : public PacketHeader {
	char loginId[kLoginIdMax];
	char password[kPasswordMax];
};

struct SLoginPacket : public PacketHeader {
	AccountResult result;
	int64 accountId;                  // 실패 시 0
	wchar_t nickname[kNicknameMax];   // 실패 시 빈 문자열
};

#pragma pack(pop)

#endif // protocol_hpp
