#ifndef server_packet_handler_hpp
#define server_packet_handler_hpp

class ServerPacketHandler {
public:
	static void handlePacket(uint8* buffer, int32 len);
};

#endif	// server_packet_handler_hpp