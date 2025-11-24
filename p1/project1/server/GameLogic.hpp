#ifndef GAME_LOGIC_HPP
#define GAME_LOGIC_HPP

class GameLogic {
public:
	GameLogic() : logicThread_(), running_(false), rooms_() {}

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

	std::vector<SPRoom> rooms_;
};

#endif // GAME_LOGIC_HPP