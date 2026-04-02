#ifndef packet_manager_hpp
#define packet_manager_hpp

class SendBuffer;
class GameSession;

/**
* @brief SingletonBase
*/
class PacketManager {
public:
	static void handlePacket(GameSession* session, byte* buffer, int32 len);
	static void handleCMovePacket(GameSession* session, byte* buffer, int32 len);
	static void handleCMouseMovePacket(GameSession* session, byte* buffer, int32 len);

	static std::shared_ptr<SendBuffer> makeSEnterPacket(const PlayerInfo& playerInfo, const std::vector<ObjectInfo>& objInfos);
	static std::shared_ptr<SendBuffer> makeSEnterOtherPacket(const PlayerInfo& playerInfo);
	static std::shared_ptr<SendBuffer> makeSLeavePacket(uint16 playerId);
	static std::shared_ptr<SendBuffer> makeSMovePacket(uint16 playerId, DirectX::XMFLOAT3 pos);
	static std::shared_ptr<SendBuffer> makeSMouseMovePacket(uint16 playerId, float yawRad);
};

#endif // packet_manager_hpp