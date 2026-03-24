#ifndef client_packet_manager_hpp
#define client_packet_manager_hpp

class PacketManager {
public:
	static void handlePacket(byte* buffer, int32 len);
	static void handleSEnterPacket(byte* buffer, int32 len);
	static void handleSEnterOtherPacket(byte* buffer, int32 len);
};

#endif // client_packet_manager_hpp