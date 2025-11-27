#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>
#include <array>

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

	csMoveStart,
	csMoveStop,
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

};

struct SCSetupPacket {
	std::int32_t objectCount;
	std::array<XMFLOAT3, 6> pos;
	std::array<XMFLOAT3, 6> orient;
	std::array<XMFLOAT3, 6> scale;
	std::array<std::int32_t, 6> materialSetIdx;
};

struct SCEnterPacket {
	std::int32_t playerCount;
	std::array<std::int32_t, maxUserCount> pIds;
	std::array<float, maxUserCount> x;
	std::array<float, maxUserCount> z;
};

struct CSLeavePacket {

};

struct SCLeavePacket {
	std::int32_t playerId;
};

struct CSMoveStartPacket {
	Direction dir;
};

struct CSMoveStopPacket {
	Direction dir;
};

struct SCMovePacket {
	std::int32_t playerId;
	float x;
	float z;
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

		CSMoveStartPacket csMoveStart;
		CSMoveStopPacket csMoveStop;
		SCMovePacket scMove;

		CSFindRoomPacket csFindRoom;
	};
};

#pragma pack( pop )

#endif // PROTOCOL_HPP