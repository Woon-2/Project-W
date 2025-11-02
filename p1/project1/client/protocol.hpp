#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>
#include <array>

constexpr const char* serverIp = "127.0.0.1";
constexpr int serverPort = 7777;

struct PacketHeader {
	std::uint16_t size;
	std::uint16_t id;	// packet type (protocol id)
};

enum class direction : std::uint8_t {
	w, a, s, d
};

enum class PacketType : std::uint16_t {
	csEnter,
	scAssignId,
	scEnter,

	csLeave,
	scLeave,

	csMove,
	scMove
};

struct CSEnterPacket {

};

struct SCAssignIdPacket {
	std::int32_t playerId;
};

struct SCEnterPacket {
	std::array<std::int32_t, 100> pIds;
	float x;
	float y;
	float z;
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
		CSEnterPacket csEnter;
		SCAssignIdPacket scAssignId;
		SCEnterPacket scEnter;
		CSLeavePacket csLeave;
		SCLeavePacket scLeave;
		CSMovePacket csMove;
		SCMovePacket scMove;
	};
};

#endif // PROTOCOL_HPP