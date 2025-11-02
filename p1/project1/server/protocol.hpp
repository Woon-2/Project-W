#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>

constexpr const char* serverIp = "127.0.0.1";
constexpr int serverPort = 7777;

struct PacketHeader {
	std::uint16_t size;
	std::uint16_t id;	// packet type (protocol id)
};

#endif // PROTOCOL_HPP