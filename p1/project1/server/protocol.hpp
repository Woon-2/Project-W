#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>
#include <array>

constexpr const char* serverIp = "127.0.0.1";
constexpr int serverPort = 7777;

#pragma pack( push, 1 )

struct PacketHeader {
	std::uint16_t size;
	std::uint16_t id;	// packet type (protocol id)
};

enum class direction : std::uint8_t {
	w, a, s, d
};

enum class PacketType : std::uint16_t {
	csSignup,
	scSignup,
	
	csLogin,
	scLogin,

	csEnter,
	scAssignId,
	scEnter,

	csLeave,
	scLeave,

	csMove,
	scMove
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

struct CSEnterPacket {

};

struct SCAssignIdPacket {
	std::int32_t playerId;
};

struct SCEnterPacket {
	std::int32_t playerCount;
	std::array<std::int32_t, 50> pIds;
	std::array<float, 50> x;
	std::array<float, 50> y;
	std::array<float, 50> z;
};

struct CSLeavePacket {

};

struct SCLeavePacket {
	std::int32_t playerId;
};

struct CSMovePacket {
	direction dir;
};

struct SCMovePacket {
	std::int32_t playerId;
	float x;
	float y;
	float z;
};

struct Packet {
	PacketHeader header;
	union {
		CSSignupPacket csSignup;
		SCSignupPacket scSignup;
		CSLoginPacket csLogin;
		SCLoginPacket scLogin;
		CSEnterPacket csEnter;
		SCAssignIdPacket scAssignId;
		SCEnterPacket scEnter;
		CSLeavePacket csLeave;
		SCLeavePacket scLeave;
		CSMovePacket csMove;
		SCMovePacket scMove;
	};
};

#pragma pack( pop )

#endif // PROTOCOL_HPP