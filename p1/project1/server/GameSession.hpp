#ifndef GAME_SESSION_HPP
#define GAME_SESSION_HPP

#include "Session.hpp"

class GameSession : public PacketSession {
public:
	GameSession()
		: myRoomId_(-1), signupAndLoginMtx_() {}

	virtual ~GameSession() {
		std::cout << "GameSession " << getId() << " destructed.\n";
	}

	virtual void onConnected() override;
	virtual void onDisconnected() override;
	virtual int32 onRecvPacket(uint8* buffer, int32 len) override;
	virtual void onSend(int32 len) override {
		// std::cout << "GameSession sent " << len << " bytes.\n";
	}

	bool signupUser(const std::string& id, const std::string& pw, std::string& err);
	bool loginUser(const std::string& id, const std::string& pw, std::string& err);

	void move(float deltaTime) {

	}

	float radius_;

private:
	int32 myRoomId_;
	std::mutex signupAndLoginMtx_;
};

#endif // GAME_SESSION_HPP