#ifndef __StandAlone_game_HPP
#define __StandAlone_game_HPP

#include "../pch.hpp"
#include "../IGame.hpp"

#include "../AssetManager.hpp"
#include "../gfx.hpp"
#include "../object.hpp"
#include "../camera.hpp"
#include "../light.hpp"

namespace StandAlone {

class Game : public IGame {
public:
	// 사용자 입력을 받아 스레드 풀과 GFX 객체를 초기화한다.
	Game();
	// 객체들을 생성한다.
	void setupStage();

	void update(Milliseconds deltaTime) override;
	void render() override;

private:
	void processInput(Milliseconds deltaTime);

	void importNode(std::ifstream& ifs);
	void importCube(std::ifstream& ifs, Object& cube);
	void importPlayerStart(std::ifstream& ifs, Object& player);

	AssetManager assetManager_{};

	GFX gfx_{};
	ThreadPool threadPool_{};

	std::vector<Object> cubes_{};
	std::shared_ptr<Object> player_{};
	Camera camera_{};
	Light dirLight_{};
	bool playerSpawned_ = false;
};

}	// namespace StandAlone

#endif	// __StandAlone_game_HPP