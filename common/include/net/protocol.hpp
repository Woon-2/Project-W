#ifndef __PROTOCOL_HPP
#define __PROTOCOL_HPP

#include "netInclude.hpp"

#include <cstdint>

inline constexpr const char* SERVERIP = "127.0.0.1";
inline constexpr std::uint16_t PORT = 7777;
inline constexpr auto maxConnection = 5;

#pragma pack(push, 1)
struct RigidXform {
    float translation[3];
    float rotation[3];
};

enum class PacketType : std::uint8_t {
    CSHello,
    SCCreate,
    CSLeave,
    SCWorld,
    CSWorld,
    SCInitInfo,
    SCInitCreate,
    SCInitAssign,
};

enum class ObjectType : std::uint8_t {
    Helicopter,
    Tree0,
    Tree1,
    Tree2,
};

struct CSHello {
 
};

struct SCInitAssign {
    std::uint32_t netId;
};

struct SCCreate {
    std::uint32_t netId;
    ObjectType objType;
    RigidXform xform;
};

using SCInitCreate = SCCreate;

struct SCInitInfo {
    std::uint32_t packetCnt;
};

struct CSLeave {

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
        CSHello csHello;
        SCCreate scCreate;
        CSLeave csLeave;
        SCWorld scWorld;
        CSWorld csWorld;
        SCInitInfo scInitInfo;
        SCInitCreate scInitCreate;
        SCInitAssign scInitAssign;
    };
};
#pragma pack(pop)

template <class T>
constexpr std::uint16_t calcPacketSize() {
    return sizeof(PacketType) + sizeof(std::uint16_t) + sizeof(T);
}

template <>
constexpr std::uint16_t calcPacketSize<CSHello>() {
    return sizeof(PacketType) + sizeof(std::uint16_t);
}

template <>
constexpr std::uint16_t calcPacketSize<CSLeave>() {
    return sizeof(PacketType) + sizeof(std::uint16_t);
}

#endif // __PROTOCOL_HPP