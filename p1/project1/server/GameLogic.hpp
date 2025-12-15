#ifndef GAME_LOGIC_HPP
#define GAME_LOGIC_HPP

#include "timer.hpp"

enum class LogicMsgType : uint8 {
	None,
	AddRoom,
	RemoveRoom,
	UserEnter,
	UserLeave,
	UserMoveInput,
	UserMouseMove,
	UserMoveState,
};

struct LogicMessage {
	LogicMsgType type{LogicMsgType::None};
	int32 userId{};
	int32 roomId{};
	int16 moveXSign{};
	int16 moveZSign{};
	float playerYawRadian{};
	float cameraPitchRadian{};
	DirectX::XMFLOAT3 position{};
	DirectX::XMFLOAT3 velocity{};
	DirectX::XMFLOAT3 forward{};
	std::uint32_t timeStamp{};
};

class GameLogic {
public:
	GameLogic() : logicThread_(), running_(false), msgQueue_(),
		rooms_(), idRoomMap_(), logicTimer_(), accTime_(0.f), logicUpdateInterval_(16.6667f) {}

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

	void enqueueMessage(const LogicMessage& msg) { msgQueue_.enqueue(msg); }
	void processMessage();

private:
	std::thread logicThread_;
	std::atomic_bool running_;

	CCQueue<LogicMessage> msgQueue_;
	std::vector<SPRoom> rooms_;
	static const int32 maxRoomCount_;
	std::unordered_map<int32, SPRoom> idRoomMap_;

	Timer logicTimer_;
	Milliseconds accTime_;
	Milliseconds logicUpdateInterval_;
};

#endif // GAME_LOGIC_HPP