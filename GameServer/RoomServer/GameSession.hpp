#ifndef game_session_hpp
#define game_session_hpp

#include "Session.hpp"

class Room;
class Player;

class GameSession : public PacketSession {
public:
	GameSession() : myRoom_(nullptr), myPlayer_(nullptr) {}
	virtual ~GameSession();

	Room* room() { return myRoom_; }
	Player* player() { return myPlayer_; }

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