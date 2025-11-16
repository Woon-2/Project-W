#ifndef __StandAlone_game_HPP
#define __StandAlone_game_HPP

#include "../pch.hpp"
#include "../IGame.hpp"

#include "../AssetManager.hpp"
#include "../gfx.hpp"
#include "../object.hpp"
#include "../skybox.hpp"
#include "../camera.hpp"
#include "../light.hpp"

#include "../physics.hpp"

namespace StandAlone {

class Game : public IGame {
public:
	// 사용자 입력을 받아 스레드 풀과 GFX 객체를 초기화한다.
	Game();
	// 객체들을 생성한다.
	void setupStage();

	void update(Milliseconds deltaTime) override;
	void render() override;

	// 윈도우 프로시저에서 특정한 메시지 처리를 위임받는다.
	LRESULT receiveWndMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
	void processInput(Milliseconds deltaTime);

	void importNode(std::ifstream& ifs);
	void importCube(std::ifstream& ifs, Object& cube);
	void importPlayerStart(std::ifstream& ifs, Object& player);

	AssetManager assetManager_{};

	PhysicSystem physicSystem_{};
	Seconds physicUpdateAcc_{0s};	// 물리 업데이트를 위한 시간 누산기
	Seconds physicUpdateInterval{1s/60.f};	// 60fps로 물리 업데이트

	GFX gfx_{};
	ThreadPool threadPool_{};

	std::vector<Object> cubes_{};
	std::shared_ptr<Object> player_{};
	SkyboxObject skybox_{};
	Camera camera_{};
	mu::Radian cameraPitch_ = 0.f;
	Light dirLight_{};
	bool playerSpawned_ = false;

	LONG mouseDeltaX_{};
	LONG mouseDeltaY_{};
	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardState_{};
};

}	// namespace StandAlone

#endif	// __StandAlone_game_HPP