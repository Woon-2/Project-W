#ifndef __PROTOCOL_HPP
#define __PROTOCOL_HPP

#include <cstdint>

constexpr const char* serverIp = "127.0.0.1";
constexpr std::uint16_t serverPort = 8000;

constexpr std::uint16_t bufferSize = 1024;

#pragma pack(push, 1)

struct RigidXform {
	float translation[ 3 ];
	float rotation[ 3 ];
};

enum class PacketType : std::uint8_t {
	SC_InitInfo,
	SC_Enter,
	SC_Move,
	SC_Leave,
	CS_Login,
	CS_Move,
};

enum class ObjectType : std::uint8_t {
	Player,
	AI,
};

enum class NetExCategory : std::uint32_t {
	Player,
	AI,
	Character,
	Helicopter
};

struct SCInitInfoPacket {
	std::uint16_t size;
	PacketType packetType;

	std::int16_t id;
	ObjectType objType;
	RigidXform xform;
};

struct SCEnterPacket {
	std::uint16_t size;
	PacketType packetType;

	std::int16_t id;
	ObjectType objType;
	RigidXform xform;
};

struct SCMovePacket {
	std::uint16_t size;
	PacketType packetType;

	RigidXform xform;
};

struct SCLeavePacket {
	std::uint16_t size;
	PacketType packetType;

	std::int16_t id;
};

struct CSLoginPacket {
	std::uint16_t size;
	PacketType packetType;
};

struct CSMovePacket {
	std::uint16_t size;
	PacketType packetType;
};

#pragma pack(pop)

#endif	// __PROTOCOL_HPP