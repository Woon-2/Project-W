#ifndef client_packet_manager_hpp
#define client_packet_manager_hpp

namespace Online { class Game; }

class PacketManager {
public:
	static void handlePacket(Online::Game* game, byte* buffer, int32 len);
	static void handleSEnterPacket(Online::Game* game, byte* buffer, int32 len);
	static void handleSEnterOtherPacket(Online::Game* game, byte* buffer, int32 len);
	static void handleSLeavePacket(Online::Game* game, byte* buffer, int32 len);
};

#endif // client_packet_manager_hpp