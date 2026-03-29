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
#include "combatSystem.hpp"

#include "../billboard.hpp"
#include "../spriteAnimation.hpp"
#include "../basicPlayerHpUI.hpp"
#include "../event.hpp"
#include "../crosshair.hpp"
#include "../debugBVView.hpp"
#include "../particleSystem.hpp"
#include "../meshParticleSystem.hpp"

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

	// 게임의 업데이트는 다음 순서대로 이루어진다.
	// 입력 처리
	// 이벤트 처리
	// 물리 업데이트 루틴
	// 객체별 업데이트 루틴
	// 애니메이션 업데이트
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
	void importCube(std::ifstream& ifs, Cube& cube);
	void importPlayerStart(std::ifstream& ifs, Player& player);
	void importGoblinSpawner(std::ifstream& ifs, Goblin& goblin);
	void importAnubisSpawner(std::ifstream& ifs, Anubis& anubis);
	void importBatSpawner(std::ifstream& ifs, Bat& bat);
	void importBomberSpawner(std::ifstream& ifs, Bomber& bomber);
	void importDemonSpawner(std::ifstream& ifs, Demon& demon);
	void importDragonSpawner(std::ifstream& ifs, Dragon& dragon);
	void importEyeballSpawner(std::ifstream& ifs, Eyeball& eyeball);
	void importFishmanSpawner(std::ifstream& ifs, Fishman& fishman);
	void importGargoyleSpawner(std::ifstream& ifs, Gargoyle& gargoyle);
	void importTerrain(std::ifstream& ifs, TerrainObject& terrain);

	AssetManager assetManager_{};

	AnimSystem animSystem_{};

	PhysicSystem physicSystem_{};
	CombatSystem combatSystem_{};
	DebugBVView  debugBVView_{};
	Seconds physicUpdateAcc_{0s};	// 물리 업데이트를 위한 시간 누산기
	Seconds physicUpdateInterval{1s/60.f};	// 60fps로 물리 업데이트

	GFX gfx_{};
	ThreadPool threadPool_{};

	EventList eventList_{};
	Timer* pTimer_ = nullptr;

	std::shared_ptr<Player> player_{};
	std::shared_ptr<Goblin> goblin_{};
	std::shared_ptr<Anubis> anubis_{};
	std::shared_ptr<Bat> bat_{};
	std::shared_ptr<Bomber> bomber_{};
	std::shared_ptr<Demon> demon_{};
	std::shared_ptr<Dragon> dragon_{};
	std::shared_ptr<Eyeball> eyeball_{};
	std::shared_ptr<Fishman> fishman_{};
	std::shared_ptr<Gargoyle> gargoyle_{};

	SkyboxObject skybox_{};
	std::shared_ptr<TerrainObject> terrain_{};

	Camera camera_{};
	mu::Radian cameraPitch_ = 0.f;
	// 카메라 yaw는 기본적으로 플레이어에 대한 오프셋으로만 작동하지만,
	// 플레이어 사망 이후에는 이 변수로 작동한다.
	mu::Radian cameraYaw_ = 0.f;
	CameraMode cameraMode_ = CameraMode::ThirdPerson;

	Light dirLight_{};
	AssetConfigs assetConfigs_{};
	bool playerSpawned_ = false;

	bool playerDead_{};

	BasicPlayerHpUI playerHpUI_{};
	TextImage textFPS_{};

	ParticleSystem flameParticleSystem_{};
	ParticleSystem smokeParticleSystem_{};
	EmitterConfig flameEmitterConfig_{};
	EmitterConfig smokeEmitterConfig_{};

	MeshParticleSystem  swordSlashSystem_{};
	MeshEmitterConfig   swordSlashConfig_{};

	LONG mouseDeltaX_{};
	LONG mouseDeltaY_{};
	bool cursorCaptureEnabled_ = false;
	bool cursorShowEnabled_ = true;

	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStateCurr_{};
	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStatePrev_{};
};

}	// namespace StandAlone

#endif	// __StandAlone_game_HPP