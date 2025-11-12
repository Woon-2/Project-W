#ifndef __Online_game_HPP
#define __Online_game_HPP

#include "../pch.hpp"
#include "../IGame.hpp"

#include "../gfx.hpp"
#include "../object.hpp"
#include "../camera.hpp"
#include "../light.hpp"

namespace Online {

class OnlineGame : public IGame {
public:
	// 사용자 입력을 받아 스레드 풀과 GFX 객체를 초기화한다.
	OnlineGame();
	// 객체들을 생성한다.
	void setupStage();

	void update(Milliseconds deltaTime) override;
	void render() override;

private:
	void processInput(Milliseconds deltaTime);

	//GFX gfx_{};
	ThreadPool threadPool_{};

	std::vector<std::vector<std::vector<Object>>> cubes_{};
	//std::shared_ptr<Object> player_{};
	Camera camera_{};
	Light dirLight_{};
};

}	// namespace Online

#endif	// __Online_game_HPP