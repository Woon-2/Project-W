#ifndef __PROTOCOL_HPP
#define __PROTOCOL_HPP

#include "stdafx.hpp"
#include "netInclude.hpp"

inline constexpr u16t PORT = 7777;
inline constexpr auto maxConnection = 5;

#pragma pack(push, 1)
struct RigidXform {
    float translation[3];
    float rotation[4];
};

enum class PacketType : std::uint8_t {
    CSHello,
    CSLeave,
    SCMove,
    SCAssign,
    SCEnter,
    SCLeave,
    CSInput
};

enum class ObjectType : std::uint8_t {
    Character,
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

struct SCEnter {
    u32t netId;
    RigidXform xform;
    ObjectType objType;
};

struct SCLeave {
    static constexpr u8t maxLeaveCnt = 64u;
    u8t leaveCnt;
    std::uint32_t leavedIds[maxLeaveCnt];
};

struct CSLeave {

};

struct SCMove {
    static constexpr u8t maxMoveCnt = 16u;

    struct Value {
        u32t netId;
        u64t compressedDeltaPos;
        u64t compressedDeltaRot;
    };

    u8t moveCnt;
    Value moves[maxMoveCnt];
};

enum class InputEventType : std::uint8_t {
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    Rotation
};

struct InputEvent {
    InputEventType type;
    float floatVal0;
    float floatVal1;
};

struct CSInput {
    static constexpr std::uint8_t maxEventCnt = 16;

    std::uint8_t eventCnt;
    InputEvent events[maxEventCnt];
};

struct Packet {
    u16t size;
    PacketType type;
    union {
        CSHello csHello;
        CSLeave csLeave;
        SCMove scMove;
        SCAssign scAssign;
        CSInput csInput;
        SCEnter scEnter;
        SCLeave scLeave;
    };
};
#pragma pack(pop)

template <class T>
constexpr u16t calcPacketSize() {
    return sizeof(PacketType) + sizeof(u16t) + sizeof(T);
}

template <class TPacket>
constexpr u16t calcPacketSize(u8t u8Val) {
	return 3u + sizeof(TPacket);
}

template<>
constexpr u16t calcPacketSize<CSInput>(u8t inputCnt) {
	return 3u + sizeof(u8t) + inputCnt * sizeof(InputEvent);
}

template<>
constexpr u16t calcPacketSize<SCLeave>(u8t leaveCnt) {
	return 3u + sizeof(u8t) + leaveCnt * sizeof(u32t);
}

template<>
constexpr u16t calcPacketSize<SCMove>(u8t moveCnt) {
	return 3u + sizeof(u8t) + moveCnt * sizeof(SCMove::Value);
}

template <>
constexpr u16t calcPacketSize<CSHello>() {
    return sizeof(PacketType) + sizeof(u16t);
}

template <>
constexpr u16t calcPacketSize<CSLeave>() {
    return sizeof(PacketType) + sizeof(u16t);
}

inline std::string loadServerIP() {
    auto iniFile = std::ifstream("network.ini");
    if (!iniFile) {
        throw std::runtime_error("Failed to open network.ini");
    }

    std::string line;
    std::getline(iniFile, line);
    if (!line.starts_with("serverIP=\"")) {
        throw std::runtime_error("Invalid format in network.ini");
    }
    line = line.substr(10, line.size() - 11); // Remove "serverIP=\"" and "\""
    return line;
}

#endif // __PROTOCOL_HPP