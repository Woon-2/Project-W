#ifndef __PROTOCOL_HPP
#define __PROTOCOL_HPP

#include <cstdint>

constexpr const char* serverIp = "127.0.0.1";
constexpr std::uint16_t serverPort = 8000;

constexpr std::uint16_t bufferSize = 4096u;

#pragma pack(push, 1)

struct RigidXform {
	float translation[ 3 ];
	float rotation[ 3 ];
};

enum class PacketType : std::uint8_t {
	SC_InitInfo,
	SC_InitCreate,
	SC_InitAssign,
	SC_Enter,
	SC_Move,
	SC_Leave,
	SC_World,

	CS_Login,
	CS_Move,
	CS_World,
};

enum class ObjectType : std::uint8_t {
	Character,
	Helicopter,
};

enum class NetExCategory : std::uint32_t {
	Player,
	AI,
	Character,
	Helicopter
};

struct SCInitInfo {
	std::uint32_t packetCnt;
};

struct SCCreate {
	std::uint32_t netId;
	ObjectType objType;
	RigidXform xform;
};

struct SCInitAssign {
	std::uint32_t netId;
};

struct SCEnter {
	std::int32_t netId;
};

struct SCMove {
	std::int32_t netId;
};

struct SCLeave {
	std::int32_t netId;
};

struct CSLogin {
	
};

struct CSMove {
	std::int32_t netId;
};

struct SCWorld {
	std::uint32_t netId;
	RigidXform xform;
};

struct CSWorld {
	std::uint32_t netId;
	RigidXform xform;
};

struct Packet {
	std::uint16_t size;
	PacketType type;
	union {
		SCInitInfo scInitInfo;
		SCCreate scInitCreate;
		SCCreate scCreate;
		SCInitAssign scInitAssign;
		SCEnter scEnter;
		SCMove scMove;
		SCLeave scLeave;

		CSLogin csLogin;
		CSMove csMove;

		SCWorld scWorld;
		CSWorld csWorld;
	};
};

#pragma pack(pop)

template<class T>
constexpr std::uint16_t calcPacketSize( ) {
	return sizeof( std::uint16_t ) + sizeof( PacketType ) + sizeof( T );
}

#endif	// __PROTOCOL_HPP