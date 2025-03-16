#ifndef __PROTOCOL_HPP
#define __PROTOCOL_HPP

#include "netInclude.hpp"

#include "mathUtil.hpp"

#include <cstdint>

inline constexpr const char* SERVERIP = "127.0.0.1";
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

#pragma pack(push, 1)
struct Packet {
    std::uint16_t size;
    PacketType type;
    union {
        CSHello csHello;
        SCAssign scAssign;
        CSLeave csLeave;
        SCWorld scWorld;
        CSWorld csWorld;
    };
};
#pragma pack(pop)

#endif // __PROTOCOL_HPP