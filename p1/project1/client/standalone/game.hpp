#ifndef __StandAlone_game_HPP
#define __StandAlone_game_HPP

#include "../IGame.hpp"

#include "../AssetManager.hpp"
#include "../gfx.hpp"
#include "../object.hpp"
#include "../skybox.hpp"
#include "../camera.hpp"
#include "../light.hpp"
#include "../animation.hpp"

#include "physics.hpp"

#include "../billboard.hpp"
#include "../spriteAnimation.hpp"
#include "../basicPlayerHpUI.hpp"
#include "../event.hpp"

class Timer;

namespace StandAlone {

class Game : public IGame {
public:
	// 사용자 입력을 받아 스레드 풀과 GFX 객체를 초기화한다.
	Game();

	GameType type() const override { return GameType::StandAlone; }

	void setTimer(Timer* pTimer) { pTimer_ = pTimer; }
	// 객체들을 생성한다.
	void setupStage();

	void update(Milliseconds deltaTime) override;
	void render() override;

	// 윈도우 프로시저에서 특정한 메시지 처리를 위임받는다.
	LRESULT receiveWndMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
	enum class CameraMode {
		FirstPerson,
		ThirdPerson
	};

	void processInput(Milliseconds deltaTime);

	// 커서가 클라이언트 영역 바깥으로 나가지 못하도록 한다.
	// 한번 설정해놓으면, releaseCursor를 호출하기 전까지 커서는 계속 클라이언트 영역에 갇혀있는다.
	void captureCursor();
	// 커서 캡처가 설정되어있다면 해제한다.
	// captureCursor로 활성화된 커서 캡처를 해제하는 역할을 한다.
	void releaseCursor();
	void hideCursor();
	void showCursor();

	void importNode(std::ifstream& ifs);
	void importCube(std::ifstream& ifs, Object& cube);
	void importPlayerStart(std::ifstream& ifs, Object& player);

	AssetManager assetManager_{};

	AnimSystem animSystem_{};

	PhysicSystem physicSystem_{};
	Seconds physicUpdateAcc_{0s};	// 물리 업데이트를 위한 시간 누산기
	Seconds physicUpdateInterval{1s/60.f};	// 60fps로 물리 업데이트

	GFX gfx_{};
	ThreadPool threadPool_{};

	EventList eventList_{};
	Timer* pTimer_ = nullptr;

	std::vector<Object> cubes_{};
	std::shared_ptr<Object> player_{};
	SkyboxObject skybox_{};

	Camera camera_{};
	mu::Radian cameraPitch_ = 0.f;
	CameraMode cameraMode_ = CameraMode::ThirdPerson;

	Light dirLight_{};
	bool playerSpawned_ = false;

	Billboard billboard_{};
	SpriteAnimation slimeSprite_{};
	std::deque<SpriteAnimation> muzzleFlashes_{};
	Milliseconds fireCooldown_{};
	std::vector<BasicPlayerHpUI> playerHpUIs_{};

	LONG mouseDeltaX_{};
	LONG mouseDeltaY_{};
	bool cursorCaptureEnabled_ = false;
	bool cursorShowEnabled_ = true;

	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStateCurr_{};
	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStatePrev_{};
};

}	// namespace StandAlone

#endif	// __StandAlone_game_HPP