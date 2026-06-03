#ifndef lobby_packet_manager_hpp
#define lobby_packet_manager_hpp

class SendBuffer;
class GameSession;

class PacketManager {
public:
	static void handlePacket( GameSession* session, byte* buffer, int32 len );
	static void handleCCreateRoomPacket( GameSession* session, byte* buffer, int32 len );
	static void handleCJoinRoomPacket( GameSession* session, byte* buffer, int32 len );
	static void handleCLeaveRoomPacket( GameSession* session, byte* buffer, int32 len );
	static void handleCGameStartPacket( GameSession* session, byte* buffer, int32 len );

	static std::shared_ptr<SendBuffer> makeSCreateRoomPacket( const std::string& code );
	static std::shared_ptr<SendBuffer> makeSJoinRoomPacket( bool success, uint16 hostId, const std::string& code, const std::vector<LobbyPlayerInfo>& playerInfos );
	static std::shared_ptr<SendBuffer> makeSLobbyRoomPlayerJoinedPacket( const LobbyPlayerInfo& info );
	static std::shared_ptr<SendBuffer> makeSLobbyRoomPlayerLeftPacket( uint16 sessionId );
	static std::shared_ptr<SendBuffer> makeSGameStartPacket( const std::string& roomServerIp, uint16 roomServerPort, const std::string& lobbyCode );
};

#endif // lobby_packet_manager_hpp