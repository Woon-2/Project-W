#ifndef lobby_room_hpp
#define lobby_room_hpp

class GameSession;
class SendBuffer;

class LobbyRoom {
public:
	LobbyRoom( const std::string& code ) : code_( code ), hostId_( 0u ), players_(), mutex_() {}

	// 입장 - 방이 꽉 찼으면 false 반환
	bool enter( GameSession* session );
	// 퇴장 - 방이 비면 LobbyManager::removeRoom 호출
	void leave( GameSession* session );
	// 게임 시작 - 방 안 전원에게 S_GameStart 브로드캐스트
	void startGame();

	const std::string& code() const { return code_; }
	uint16 hostId() const;
	std::vector<uint16> playerIds() const;
	std::vector<LobbyPlayerInfo> playerInfos() const;
	bool isFull() const;
	int32 playerCnt() const;

private:
	void broadcast( const std::shared_ptr<SendBuffer>& buf );

	static constexpr int32 maxPlayers = 4;

	std::string code_;
	uint16 hostId_;
	std::vector<GameSession*> players_;
	mutable std::mutex mutex_;
};

#endif // lobby_room_hpp
