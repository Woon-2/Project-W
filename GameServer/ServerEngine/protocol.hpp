#ifndef protocol_hpp
#define protocol_hpp

#include "types.hpp"

constexpr const char* serverIp = "127.0.0.1";
constexpr uint16 serverPort = 9000;

struct PacketHeader {
	uint16 size;
	uint16 id;
};

#endif // protocol_hpp