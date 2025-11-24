#include "pch.hpp"
#include "GameLogic.hpp"

void GameLogic::run() {
	while (running_) {
		logicTimer_.tick();
		accTime_ += logicTimer_.deltaTime<Milliseconds>().count();

		while( auto msg = msgQueue_.try_dequeue_bulk()) {
			// Process logic messages here
		}

		while (accTime_ >= logicUpdateInterval_) {
			for (auto& room : rooms_) {
				// Update each room's game logic here
			}
			accTime_ -= logicUpdateInterval_;
		}

		std::this_thread::yield();

		//-------------------------------------------------------------------

		//logicTimer_.tick();
		//accTime_ += logicTimer_.deltaTime<Milliseconds>().count();

		//if (accTime_ < logicUpdateInterval_) {
		//	const auto sleepTime = logicUpdateInterval_ - accTime_;
		//	std::this_thread::sleep_for(Milliseconds(sleepTime));
		//}

		//while (auto msg = msgQueue_.try_dequeue_bulk()) {
		//	// Process logic messages here
		//}

		//for (auto& room : rooms_) {
		//	// Update each room's game logic here
		//}

		//accTime_ = 0.f;
	}
}

const float GameLogic::logicUpdateInterval_ = 16.6667f;
