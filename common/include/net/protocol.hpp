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
    SCAssign,
    CSLeave,
    SCWorld,
    CSWorld,
};

enum class ObjectType : std::uint8_t {
    Helicopter,
    Tree0,
    Tree1,
    Tree2,
};

struct CSHello {
 
};

struct SCAssign {
    std::uint32_t netId;
};

struct SCCreate {
    std::uint32_t netId;
    ObjectType objType;
    RigidXform xform;
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
        SCAssign scAssign;
        CSLeave csLeave;
        SCWorld scWorld;
        CSWorld csWorld;
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