#ifndef game_session_hpp
#define game_session_hpp

#include "Session.hpp"
#include "protocol.hpp"

class LobbyRoom;

class GameSession : public PacketSession {
public:
	GameSession() = default;
	virtual ~GameSession();

	LobbyRoom* myRoom_ = nullptr;
	PlayerWeaponType selectedWeaponType_ = PlayerWeaponType::Katana;

protected:
	virtual void onConnected() override;
	virtual void onDisconnected() override;
	virtual void processPacket( byte* buffer, int32 len ) override;
	virtual void onSend( int32 len ) override {}
};

#endif // game_session_hpp
