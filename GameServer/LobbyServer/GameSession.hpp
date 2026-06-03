#ifndef game_session_hpp
#define game_session_hpp

#include "Session.hpp"

class LobbyRoom;

class GameSession : public PacketSession {
public:
	GameSession() = default;
	virtual ~GameSession() = default;

	LobbyRoom* myRoom_ = nullptr;

protected:
	virtual void onConnected() override;
	virtual void onDisconnected() override;
	virtual void processPacket( byte* buffer, int32 len ) override;
	virtual void onSend( int32 len ) override {}
};

#endif // game_session_hpp
