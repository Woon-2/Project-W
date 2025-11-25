#ifndef GAME_SESSION_HPP
#define GAME_SESSION_HPP

#include "Session.hpp"

class GameSession : public PacketSession {
public:
	GameSession()
		: x_(0.0f), y_(0.0f), z_(0.0f),
		radius_(0.1f), signupAndLoginMtx_(), myRoomId_(-1) {
	}

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

	bool checkCollision(const GameSession& other) const;

	float x() const { return x_; }
	float y() const { return y_; }
	float z() const { return z_; }

private:
	float x_;
	float y_;
	float z_;
	float radius_;

	std::mutex signupAndLoginMtx_;

	int32 myRoomId_;
};

#endif // GAME_SESSION_HPP