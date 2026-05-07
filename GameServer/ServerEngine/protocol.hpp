#ifndef protocol_hpp
#define protocol_hpp

#include "macro.hpp"
#include "types.hpp"
#include "../common/mathUtil.hpp"

constexpr const char* serverIp = "127.0.0.1";
constexpr uint16 serverPort = 9000;

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
	S_TimeSync,

	S_NpcRespawn,
};

enum class ObjectType : uint16 {
	Player,
	Goblin,
	Ground,
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

#pragma pack(push, 1)

struct CEnterPacket : public PacketHeader {

};

struct PlayerInfo {
	uint16 playerId;
	uint16 materialSetIdx;
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 orient;
	DirectX::XMFLOAT3 scale;
	// 추후에 player 고유 정보 추가 필요
};

struct ObjectInfo {
	ObjectType type;
	uint16 objectId;
	uint16 materialSetIdx;
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

struct STimeSyncPacket : public PacketHeader {
	uint64 serverMs;
};

struct SNpcRespawnPacket : public PacketHeader {
	uint16 npcId;
	int32 newHp;
	DirectX::XMFLOAT3 spawnPos;
};

#pragma pack(pop)

#endif // protocol_hpp