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

	static std::shared_ptr<SendBuffer> makeSCreateRoomPacket( std::string_view code );
	static std::shared_ptr<SendBuffer> makeSJoinRoomPacket( bool success, uint16 hostId, std::string_view code, const std::vector<LobbyPlayerInfo>& playerInfos );
	static std::shared_ptr<SendBuffer> makeSLobbyRoomPlayerJoinedPacket( const LobbyPlayerInfo& info );
	static std::shared_ptr<SendBuffer> makeSLobbyRoomPlayerLeftPacket( uint16 sessionId );
	static std::shared_ptr<SendBuffer> makeSGameStartPacket( std::string_view roomServerIp, uint16 roomServerPort, std::string_view lobbyCode );
};

#endif // lobby_packet_manager_hpp