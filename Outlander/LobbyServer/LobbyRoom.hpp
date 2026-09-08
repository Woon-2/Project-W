#ifndef lobby_room_hpp
#define lobby_room_hpp

#include "networkConfig.hpp"
#include "protocol.hpp"

class GameSession;
class SendBuffer;

class LobbyRoom {
public:
	LobbyRoom( const std::string& code, const NetworkEndpoint& roomServerEndpoint )
		: code_( code ), hostId_( 0u ), players_(), mutex_(),
		  roomServerEndpoint_( roomServerEndpoint ) {}

	// 입장 - 방이 꽉 찼으면 false 반환. 방이 멤버를 공동 소유하도록 shared_ptr로 보관한다.
	bool enter( const std::shared_ptr<GameSession>& session );
	// 퇴장 - 방이 비면 LobbyManager::removeRoom 호출
	void leave( GameSession* session );
	void selectWeapon( GameSession* session, PlayerWeaponType weaponType );
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
	std::vector<std::shared_ptr<GameSession>> players_;
	bool closed_ = false;   // 비어서 삭제 예정인 방. true면 enter 거부(삭제 경쟁 차단).
	mutable std::mutex mutex_;
	NetworkEndpoint roomServerEndpoint_;
};

#endif // lobby_room_hpp
