#include "pch.hpp"
#include "PacketManager.hpp"

void PacketManager::handlePacket(byte* buffer, int32 len) {
	auto header = reinterpret_cast<PacketHeader*>(buffer);

	switch (header->type) {
	case PacketType::S_Enter:
		handleSEnterPacket(buffer, len);
		break;

	case PacketType::S_Enter_Other:
		handleSEnterOtherPacket(buffer, len);
		break;

	default:
		std::cout << "Unknown packet type received. Type: " << static_cast<uint16>(header->type) << '\n';
		break;
	}
}

void PacketManager::handleSEnterPacket(byte* buffer, int32 len) {
}

void PacketManager::handleSEnterOtherPacket(byte* buffer, int32 len) {
}
