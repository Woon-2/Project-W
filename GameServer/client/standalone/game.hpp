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

#include "../physicsWorld.hpp"
#include "combatSystem.hpp"

#include "../billboard.hpp"
#include "../spriteAnimation.hpp"
#include "../event.hpp"
#include "../crosshair.hpp"
#include "../debugBVView.hpp"
#include "../particleSystem.hpp"
#include "../particleEffect.hpp"
#include "../ui/UIManager.hpp"
#include "../ui/widgets/ProgressBar.hpp"
#include "../ui/widgets/Dropdown.hpp"
#include "../ui/widgets/Label.hpp"

class Timer;

namespace StandAlone {

class Game : public IGame {
public:
	// 사용자 입력을 받아 스레드 풀과 GFX 객체를 초기화한다.
	Game();
	~Game();

	GameType type() const override { return GameType::StandAlone; }

	void setTimer(Timer* pTimer) { pTimer_ = pTimer; }
	// 객체들을 생성한다.
	void setupStage();
	void setParticle();

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
	void processInput(Milliseconds deltaTime);
	
	void cullObjects();
	void applyHiZCulling();

	void setupMonsterHpBars();

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
	void importTerrain(std::ifstream& ifs, TerrainObject& terrain);

	AssetManager assetManager_{};

	AnimSystem animSystem_{};

	PhysicsWorld physicsWorld_{};
	CombatSystem combatSystem_{};
	DebugBVView  debugBVView_{};
	Seconds physicUpdateAcc_{0s};	// 물리 업데이트를 위한 시간 누산기
	Seconds physicUpdateInterval{1s/60.f};	// 60fps로 물리 업데이트

	bool skipNextRender_ = false;
	int  physicUpdateScaleK_ = 1;
	int  consecutiveLagFrames_ = 0;
	int  consecutiveNonLagFrames_ = 0;

	GFX gfx_{};
	ThreadPool threadPool_{};

	EventList eventList_{};
	Timer* pTimer_ = nullptr;

	std::shared_ptr<Player> player_{};
	std::vector< std::shared_ptr<Goblin> > goblins_{};
	std::shared_ptr<Goblin> goblin_{};

	SkyboxObject skybox_{};
	std::shared_ptr<TerrainObject> terrain_{};

	Camera camera_{};
	mu::Radian cameraPitch_ = 0.f;
	// 카메라 yaw는 기본적으로 플레이어에 대한 오프셋으로만 작동하지만,
	// 플레이어 사망 이후에는 이 변수로 작동한다.
	mu::Radian cameraYaw_ = 0.f;

	Light dirLight_{};
	AssetConfigs assetConfigs_{};
	bool playerSpawned_ = false;

	bool playerDead_{};

	TextImage textFPS_{};

	enum class SwordEffect { SlashWave, SlashCombo, Slash7, Slash1, Spikes, CrystalsFrontAttack, AoESlashGreen, RedEnergyExplosion, CrystalsCrossFade };

	UI::UIManager    uiManager_{};
	UI::ProgressBar* playerHpBar_    = nullptr;  // owned by uiManager_
	UI::Dropdown*    effectDropdown_ = nullptr;  // owned by uiManager_
	SwordEffect      currentEffect_  = SwordEffect::SlashWave;
	UI::Label*       hiZStatsLabel_ = nullptr;  // owned by uiManager_

	ParticleSystem flameParticleSystem_{};
	ParticleSystem smokeParticleSystem_{};
	ParticleEffect swordSlash1Effect_{};
	ParticleEffect swordSlash7Effect_{};
	ParticleEffect swordSlashComboEffect_{};
	ParticleEffect slashWaveEffect_{};
	ParticleEffect spikesAttackEffect_{};
	ParticleEffect crystalsFrontAttackEffect_{};
	ParticleEffect aoESlashGreenEffect_{};
	ParticleEffect crystalsCrossFadeEffect_{};
	ParticleSystem dustParticleSystem_{};
	ParticleEffect redEnergyExplosionEffect_{};
	int            footBoneIdxLeft_  = -1;
	int            footBoneIdxRight_ = -1;
	Seconds        prevAnimTimeRun_  = 0s;

	struct MonsterHpEntry {
		Object*          monster;               // non-owning; lifetime owned by shared_ptr in Game
		UI::ProgressBar* hpBar;                 // owned by uiManager_
		float            worldYOffset;          // monster pos()로부터 HP바를 붙일 월드Y 오프셋
		float            hpBarVisibleSeconds = 0.f; // 피격 후 HP바 표시 잔여 시간 (초)
	};
	std::unordered_map<int, MonsterHpEntry> monsterHpBars_{}; // key: monster ID

	LONG mouseDeltaX_{};
	LONG mouseDeltaY_{};
	bool cursorCaptureEnabled_ = false;
	bool cursorShowEnabled_ = true;

	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStateCurr_{};
	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStatePrev_{};
};

}	// namespace StandAlone

#endif	// __StandAlone_game_HPP
