#ifndef game_session_hpp
#define game_session_hpp

#include "Session.hpp"

class Room;
class Player;

class GameSession : public PacketSession {
public:
	GameSession() : myRoom_(nullptr), myPlayer_(nullptr) {}
	virtual ~GameSession();

	Room*   room()   const { return myRoom_;   }
	Player* player() const { return myPlayer_; }

	// C_Enter(lobbyCode) 수신 시 호출. 코드로 방을 find-or-create 하고 입장 잡을 건다.
	void enterRoom(const std::string& lobbyCode);

protected:
	virtual void onConnected() override;
	virtual void onDisconnected() override;
	virtual void processPacket(byte* buffer, int32 len) override;
	virtual void onSend(int32 len) override;

private:
	Room* myRoom_;
	Player* myPlayer_;
};

#endif // game_session_hpp