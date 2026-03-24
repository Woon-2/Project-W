#ifndef packet_manager_hpp
#define packet_manager_hpp

class SendBuffer;

/**
* @brief SingletonBase
*/
class PacketManager {
public:
	static void handlePacket(byte* buffer, int32 len);

	static SendBuffer* makeSEnterPacket(const PlayerInfo& playerInfo, const std::vector<ObjectInfo>& objInfos);
	static SendBuffer* makeSEnterOtherPacket(const PlayerInfo& playerInfo);
	static SendBuffer* makeSLeavePacket(uint16 playerId);
};

#endif // packet_manager_hpp