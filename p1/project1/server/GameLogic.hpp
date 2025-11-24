#ifndef GAME_LOGIC_HPP
#define GAME_LOGIC_HPP

#include "timer.hpp"

struct LogicMessage {

};

class GameLogic {
public:
	GameLogic() : logicThread_(), running_(false), msgQueue_(),
		rooms_(), logicTimer_(), accTime_(0.f) {}


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

private:
	std::thread logicThread_;
	std::atomic_bool running_;

	CCQueue<LogicMessage> msgQueue_;
	std::vector<SPRoom> rooms_;

	Timer logicTimer_;
	float accTime_;
	static const float logicUpdateInterval_;
};

#endif // GAME_LOGIC_HPP