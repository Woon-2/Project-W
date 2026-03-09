#ifndef game_session_hpp
#define game_session_hpp

#include "Session.hpp"

class GameSession : public PacketSession {
public:
	GameSession() = default;
	
	virtual ~GameSession() {
		std::cout << "GameSession destroyed. ID: " << id() << '\n';
	}

protected:
	virtual void onConnected() override;
	virtual void onDisconnected() override;
	virtual void processPacket(byte* buffer, int32 len) override;
	virtual void onSend(int32 len) override;

private:

};

#endif // game_session_hpp