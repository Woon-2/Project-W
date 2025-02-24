#ifndef __PROTOCOL_HPP
#define __PROTOCOL_HPP

#include "netInclude.hpp"

#include "mathUtil.hpp"

#include <cstdint>

inline constexpr std::uint16_t PORT = 7777;
inline constexpr auto maxConnection = 5;

struct RigidXform {
    mu::Vec3 pos;
    mu::NQuat rot;
};

enum class PacketType : std::uint16_t {
    CSHello,
    SCAssign,
    CSLeave,
    SCWorld,
    CSWorld,
};

struct CSHello {
 
};

struct SCAssign {
    std::uint32_t id;
};

struct CSLeave {

};

struct SCWorld {
    RigidXform xforms[maxConnection];
};

struct CSWorld {
    RigidXform xform;
};

struct Packet {
    PacketType type;
    std::uint16_t size;
    union {
        CSHello csHello;
        SCAssign scAssign;
        CSLeave csLeave;
        SCWorld scWorld;
        CSWorld csWorld;
    };
};

#endif // __PROTOCOL_HPP