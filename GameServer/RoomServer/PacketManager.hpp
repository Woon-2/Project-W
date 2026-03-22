#ifndef packet_manager_hpp
#define packet_manager_hpp

class SendBuffer;

/**
* @brief SingletonBase
*/
class PacketManager {
public:
	static void handlePacket(byte* buffer, int32 len);

	static SendBuffer* makeSEnterPacket(int32 playerId, int32 playerCnt);
};

#endif // packet_manager_hpp