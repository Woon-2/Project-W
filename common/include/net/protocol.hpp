#ifndef __PROTOCOL_HPP
#define __PROTOCOL_HPP

#include "netInclude.hpp"

#ifndef DXMATH_VEC_UTIL
#define DXMATH_VEC_UTIL
#endif
#ifndef DXMATH_MAT_UTIL
#define DXMATH_MAT_UTIL
#endif
#ifndef DXMATH_QUAT_UTIL
#define DXMATH_QUAT_UTIL
#endif
#include "mathUtil.hpp"

#include <cstdint>

inline constexpr const char* SERVERIP = "127.0.0.1";
inline constexpr std::uint16_t PORT = 7777;
inline constexpr auto maxConnection = 5;

struct RigidXform {
    mu::Vec3 translation;
    mu::NQuat rotation;
};

enum class PacketType : std::uint16_t {
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

// #pragma pack(push, 1)
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
// #pragma pack(pop)

#endif // __PROTOCOL_HPP