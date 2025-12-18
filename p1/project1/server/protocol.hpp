#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>
#include <array>

#include "../common/mathUtil.hpp"

constexpr const char* serverIp = "127.0.0.1";
constexpr int serverPort = 7777;

// 한 방에 들어갈 수 있는 최대 유저 수
constexpr std::uint16_t maxUserCount = 4u;

#pragma pack( push, 1 )

struct PacketHeader {
	std::uint16_t size;
	std::uint16_t id;	// packet type (protocol id)
};

enum class Direction : std::uint8_t {
	w, a, s, d
};

enum class PacketType : std::uint16_t {
	csSignup,
	scSignup,

	csLogin,
	scLogin,

	scAssignId,
	csEnter,
	scSetup,
	scEnter,

	csLeave,
	scLeave,

	csMoveInput,
	csMouseMove,
	csMoveState,
	scMouseMove,
	scRollback,
	scMove,

	csFindRoom,
};

struct CSSignupPacket {
	std::array<char, 20> id;
	std::array<char, 20> pw;
};

struct SCSignupPacket {
	bool isOk;
	std::array<char, 50> reason;
};

struct CSLoginPacket {
	std::array<char, 20> id;
	std::array<char, 20> pw;
};

struct SCLoginPacket {
	bool isOk;
	std::array<char, 50> reason;
};

struct SCAssignIdPacket {
	std::int32_t playerId;
};

struct CSEnterPacket {

};

enum class ObjectType : std::uint8_t {
	Player,
	Cube,
};

struct ObjectData {
	ObjectType type;
	std::int32_t objectId;
	std::uint32_t materialSetIdx;
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 orient;
	DirectX::XMFLOAT3 scale;
};

struct SCSetupPacket {
	std::int32_t objectCount;
};

struct SCEnterPacket {
	std::int32_t playerCount;
	std::array<std::int32_t, maxUserCount> pIds;
	std::array<float, maxUserCount> x;
	std::array<float, maxUserCount> y;
	std::array<float, maxUserCount> z;
};

struct CSLeavePacket {

};

struct SCLeavePacket {
	std::int32_t playerId;
};

struct CSMouseMovePacket {
	float playerYawRadian;
	float cameraPitchRadian;
	std::uint32_t timeStamp;
};

struct CSMoveStatePacket {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 velocity;
	DirectX::XMFLOAT3 forward;
	std::uint32_t timeStamp;
};

struct SCMouseMovePacket {
	std::int32_t playerId;
	float playerYawRadian;
	float cameraPitchRadian;
};

struct SCRollbackPacket {
	std::int32_t playerId;
	DirectX::XMFLOAT3 pos;
};

struct SCMovePacket {
	std::int32_t playerId;
	DirectX::XMFLOAT3 pos;
	float playerYawRadian;
	float cameraPitchRadian;
};

struct CSFindRoomPacket {
	std::int32_t roomId;
};

struct Packet {
	PacketHeader header;
	union {
		CSSignupPacket csSignup;
		SCSignupPacket scSignup;

		CSLoginPacket csLogin;
		SCLoginPacket scLogin;

		SCAssignIdPacket scAssignId;
		CSEnterPacket csEnter;
		SCSetupPacket scSetup;
		SCEnterPacket scEnter;

		CSLeavePacket csLeave;
		SCLeavePacket scLeave;

		CSMouseMovePacket csMouseMove;
		CSMoveStatePacket csMoveState;
		SCMouseMovePacket scMouseMove;
		SCRollbackPacket scRollback;
		SCMovePacket scMove;

		CSFindRoomPacket csFindRoom;
	};
};

#pragma pack( pop )

#endif // PROTOCOL_HPP