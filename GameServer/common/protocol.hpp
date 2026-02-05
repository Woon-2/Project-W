#ifndef protocol_hpp
#define protocol_hpp

#include <cstdint>

constexpr const char* serverIp = "127.0.0.1";
constexpr int serverPort = 7777;

struct PacketHeader {
	std::uint16_t size;
	std::uint16_t id;
};

#pragma pack(push, 1)



#pragma pack(pop)

#endif	// protocol_hpp