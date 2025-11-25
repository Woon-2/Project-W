#ifndef GAME_LOGIC_MANAGER_HPP
#define GAME_LOGIC_MANAGER_HPP

#include "GameLogic.hpp"

class GameLogicManager {
public:
	static void init(int32 logicThreadCount) {
		logics_.reserve(logicThreadCount);
		for (int32 i = 0; i < logicThreadCount; ++i) {
			logics_.emplace_back(std::make_unique<GameLogic>());
		}
	}

	static void release() {
		logics_.clear();
	}

	static void startAll() {
		for (auto& logic : logics_) {
			logic->start();
		}
	}

	static void stopAll() {
		for (auto& logic : logics_) {
			logic->stop();
		}
	}

	static void dispatchMessage(const LogicMessage& msg) {
		int32 idx = msg.roomId % static_cast<int32>(logics_.size());
		logics_[idx]->pushMessage(msg);
	}

private:
	static std::vector<std::unique_ptr<GameLogic>> logics_;
};

#endif // GAME_LOGIN_MANAGER_HPP