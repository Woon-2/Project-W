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
#include "../terrainChunkManager.hpp"
#include "combatSystem.hpp"
#include "../skill/skillSystem.hpp"

#include "../billboard.hpp"
#include "../spriteAnimation.hpp"
#include "../event.hpp"
#include "../crosshair.hpp"
#include "../debugBVView.hpp"
#include "../physicsTestObject.hpp"
#include "../particleSystem.hpp"
#include "../particleEffect.hpp"
#include "../ui/UIManager.hpp"
#include "../ui/widgets/ProgressBar.hpp"
#include "../ui/widgets/Dropdown.hpp"
#include "../ui/widgets/Label.hpp"
#include "../ui/widgets/Image.hpp"
#include "../ui/dialogue/DialogueSystem.hpp"
#include "../ui/dialogue/TacticalDialogueOverlay.hpp"

#include "../editor/editorController.hpp"

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
	void toggleFullscreen();
	// Debug: toggle ragdoll on/off for the currently controlled object (editor caster).
	// Rebuilds the ragdoll for the caster's current model (covers hot-swapped rigs, e.g. Boss).
	void toggleCasterRagdoll();

	void cullObjects();
	void cullObjectsForShadow();
	// Hi-Z/frustum 컬링 결과를 Object::hiZCulled_ 및 AnimBlender::culled_에 반영한다.
	// (이전 이름: applyHiZCulling — 컬링 자체를 수행하는 게 아니라 readback 결과를
	//  애니메이션 시스템으로 피드백하는 역할이라 이름을 바꿈)
	void feedbackCullResultToAnim();

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
	void configureGoblin(Goblin& goblin);

	// Physics test object debug tools (keys 1-5, K, R, comma, period, M, V, I, P)
	void spawnTestObject(int kind);   // 1=pendulum 2=doublePendulum 3=hingeDoor 4=coneTwistArm 5=coneTwistChain
	void clearTestObjects();
	void fireImpulseRay();

	AssetManager assetManager_{};

	AnimSystem animSystem_{};

	PhysicsWorld physicsWorld_{};
	CombatSystem combatSystem_{};
	DebugBVView  debugBVView_{};

	SkillSystem          skillSystem_{};
	SkillDispatchContext skillCtx_{};
	GroundSampler        groundSampler_{};  // bound to chunkManager_; referenced by skillCtx_.ground
	std::vector<Object*>         skillObjectById_{};
	std::vector<ParticleEffect*> skillVfxById_{};
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

	// IBL 반사 확인용 금속 구 (StandAlone). Model은 Game이 소유, Object는 포인터로 참조.
	Model metalSphereModel_{};
	std::shared_ptr<Cube> metalSphere_{};

	SkyboxObject skybox_{};
	TerrainChunkManager chunkManager_{};

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

	enum class SwordEffect { SlashWave, SlashCombo, Slash7, Slash1, Spikes, CrystalsFrontAttack, AoESlashGreen, RedEnergyExplosion, CrystalsCrossFade, Arrow, ArrowVolley, ArrowRain, EnergyExplosionArrow, TornadoShot, Piercing, PiercingSlash, PiercingCircleSlash, PiercingMulti };

	UI::UIManager    uiManager_{};
	UI::DialogueSystem dialogueSystem_{};
	UI::TacticalDialogueOverlay tacticalDialogueOverlay_{};
	uint8 tacticalDialoguePreviewIndex_ = 0;
	UI::Image*       playerHpHeart_  = nullptr;  // owned by uiManager_
	UI::ProgressBar* playerHpBar_    = nullptr;  // owned by uiManager_
	UI::Dropdown*    effectDropdown_ = nullptr;  // owned by uiManager_
	SwordEffect      currentEffect_  = SwordEffect::SlashWave;
	UI::Label*       hiZStatsLabel_ = nullptr;  // owned by uiManager_

	// In-game skill / monster-pattern authoring tool. The standalone mode boots
	// straight into this editor (see CLAUDE editor design doc).
	Editor::Controller editor_{};

	ParticleSystem flameParticleSystem_{};
	ParticleSystem smokeParticleSystem_{};
	ParticleEffect bloodEffect_{};   // 칼/창/완드 공통 피격 혈흔 (vfxId 0)
	ParticleEffect swordSlash1Effect_{};
	ParticleEffect swordSlash7Effect_{};
	ParticleEffect swordSlashComboEffect_{};
	ParticleEffect slashWaveEffect_{};
	ParticleEffect spikesAttackEffect_{};
	ParticleEffect piercingEffect_{};
	ParticleEffect piercingMultiEffect_{};
	ParticleEffect piercingSlashEffect_{};
	ParticleEffect piercingCircleSlashEffect_{};
	ParticleEffect crystalsFrontAttackEffect_{};
	ParticleEffect aoESlashGreenEffect_{};
	ParticleEffect crystalsCrossFadeEffect_{};
	ParticleSystem dustParticleSystem_{};
	ParticleEffect redEnergyExplosionEffect_{};
	ParticleEffect arrowEffect_{};
	ParticleEffect arrowVolleyMuzzleEffect_{};
	ParticleEffect arrowVolleyEffect_{};
	ParticleEffect arrowRainMuzzleEffect_{};
	ParticleEffect arrowRainEffect_{};
	ParticleEffect energyExplosionArrowEffect_{};
	ParticleEffect tornadoShotEffect_{};
	ParticleEffect tornadoMuzzleEffect_{};
	ParticleEffect tornadoHitEffect_{};

	bool      tornadoShotActive_   = false;
	mu::Vec3  tornadoShotPos_{};
	mu::Vec3  tornadoShotDir_{};
	mu::NQuat tornadoShotOrient_{};
	Seconds   tornadoShotElapsed_{ 0s };
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
	bool gravityEnabled_ = true;

	// F9: debug toggle for the boss heat-distortion effect (no server needed).
	// When on, a heat source is attached to goblin_ each frame so the effect can be
	// tuned standalone. J/K adjust warp strength, -/= adjust glow strength.
	bool  heatDebugEnabled_   = false;
	float heatDebugWarpScale_ = 1.0f;
	float heatDebugGlowScale_ = 1.0f;

	// --- Physics constraint debug state ---
	std::vector<PhysicsTestObject> rdObjects_{};
	float rdImpulseStrength_ = 5.f;    // N*s applied by R key
	bool fullscreen_ = false;
	float rdDebugTimeScale_  = 1.0f;   // physics time multiplier: 1.0 / 0.25 / 0.05
	bool  rdShowBodies_      = false;  // V key: push body OBBs to debugBVView_ each frame
	bool  rdFrozen_          = false;  // P key: zero all test-body velocities each step
	bool  skillDebugBV_      = false;  // H key: push skill hitbox OBBs to debugBVView_ each frame

	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStateCurr_{};
	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStatePrev_{};
};

}	// namespace StandAlone

#endif	// __StandAlone_game_HPP
