#ifndef GAME_SESSION_HPP
#define GAME_SESSION_HPP

#include "Session.hpp"

class GameSession : public PacketSession {
public:
	GameSession()
		: keyMask_(), x_(0.0f), oldX_(0.f), z_(0.0f), oldZ_(0.f),
		radius_(0.1f), signupAndLoginMtx_(), myRoomId_(-1) {}

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

	// setter, getter 늘어나는 거 방지하기 위해 public으로 배치함
	// user(player)의 좌표의 스냅샷을 저장하기 위해 oldX_, oldZ_ 추가
	uint8 keyMask_;
	float x_;
	float oldX_;
	float z_;
	float oldZ_;
	float radius_;

private:
	int32 myRoomId_;
	std::mutex signupAndLoginMtx_;
};

#endif // GAME_SESSION_HPP