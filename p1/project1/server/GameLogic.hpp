#ifndef GAME_LOGIC_HPP
#define GAME_LOGIC_HPP

#include "timer.hpp"

enum class LogicMsgType : uint8 {
	AddRoom,
	RemoveRoom,
	UserEnter,
	UserLeave,
	UserMoveStart,
	UserMoveStop
};

struct LogicMessage {
	LogicMsgType type{};
	Direction dir{};
	int32 userId{};
	int32 roomId{};
	DirectX::XMFLOAT3 forward{};
	float cameraPitch{};
};

class GameLogic {
public:
	GameLogic() : logicThread_(), running_(false), msgQueue_(),
		rooms_(), idRoomMap_(), logicTimer_(), accTime_(0.f) {}

	void start() {
		running_ = true;
		logicThread_ = std::thread(&GameLogic::run, this);
	}

	void stop() {
		running_ = false;

		if (logicThread_.joinable()) {
			logicThread_.join();
		}
	}

	void run();

	void enqueueMessage(const LogicMessage& msg) {	msgQueue_.enqueue(msg); }
	void processMessage();

private:
	std::thread logicThread_;
	std::atomic_bool running_;

	CCQueue<LogicMessage> msgQueue_;
	std::vector<SPRoom> rooms_;
	static const int32 maxRoomCount_;
	std::unordered_map<int32, SPRoom> idRoomMap_;

	Timer logicTimer_;
	float accTime_;
	static const float logicUpdateInterval_;
};

#endif // GAME_LOGIC_HPP