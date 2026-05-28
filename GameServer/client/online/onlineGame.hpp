#ifndef __Online_game_HPP
#define __Online_game_HPP

#include "../IGame.hpp"

#include "../gfx.hpp"
#include "../object.hpp"
#include "../camera.hpp"
#include "../skybox.hpp"
#include "../light.hpp"
#include "../AssetManager.hpp"
#include "../physicsWorld.hpp"
#include "../animation.hpp"
#include "../event.hpp"
#include "../ui/UIManager.hpp"
#include "../ui/widgets/ProgressBar.hpp"
#include "../ui/widgets/Label.hpp"
#include "../spriteAnimation.hpp"
#include "../crosshair.hpp"
#include "../particleSystem.hpp"
#include "../particleEffect.hpp"
#include "../ui/widgets/Dropdown.hpp"
#include "../debugBVView.hpp"
#include "../skill/skillSystem.hpp"

class Timer;
class SendBuffer;

namespace Online {

class Game : public IGame {
public:
	// 사용자 입력을 받아 스레드 풀과 GFX 객체를 초기화한다.
	Game();

	GameType type() const override { return GameType::Online; }

	void setTimer(Timer* pTimer) { pTimer_ = pTimer; }

	// 객체들을 생성한다.
	void setupStage();

	void setupPlayer(const PlayerInfo& playerInfo);
	void setParticle();
	void setupGround(const ObjectInfo& groundInfo);
	void createOtherPlayer(const ObjectInfo& otherPlayerInfo);
	void createOtherPlayer(const PlayerInfo& otherPlayerInfo);
	void createGoblin(const ObjectInfo& goblinInfo);

	void removePlayer( i32t playerId );
	void movePlayer(uint16 playerId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 velocity);
	void rotatePlayer(uint16 playerId, float yawRad);

	void moveGoblin(uint16 npcId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 orient, DirectX::XMFLOAT3 velocity);

	void onNpcAttack(uint16 npcId);
	void onPlayerAttack(uint16 attackerId);
	void applyHit(uint16 targetId, int32 newHp);
	void onNpcRespawn( uint16 npcId, int32 newHp, DirectX::XMFLOAT3 spawnPos );
	void onSkillStart( uint16 ownerId, uint32 skillAssetId, uint16 elapsedMs );
	void onSkillHit( uint16 attackerId, uint16 targetId, int32 newHp, uint32 skillAssetId );
	void onDebugHitboxes( SDebugHitboxPacket* pkt );

	// 게임의 업데이트는 다음 순서대로 이루어진다.
	// 네트워크 패킷 처리(SleepEx)
	// 입력 처리
	// 이벤트 처리
	// 물리 업데이트 루틴
	// 객체별 업데이트 루틴
	// 애니메이션 업데이트
	void update(Milliseconds deltaTime) override;
	void render() override;
	// 윈도우 프로시저에서 특정한 메시지 처리를 위임받는다.
	LRESULT receiveWndMsg( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) override;

private:
	struct SpriteAnimationOwned {
		SpriteAnimation anim;
		std::shared_ptr<Object> pOwner;
	};

	void sendMovePacket();
	void sendMouseMovePacket();
	void sendAttackPacket();
	void sendSkillStartPacket(uint32 skillAssetId);

	void processInput(Milliseconds deltaTime);
	void processInputLobby(Milliseconds deltaTime);
	void processInputGame(Milliseconds deltaTime);

	void cullObjects();
	void applyHiZCulling();

	// 커서가 클라이언트 영역 바깥으로 나가지 못하도록 한다.
	// 한번 설정해놓으면, releaseCursor를 호출하기 전까지 커서는 계속 클라이언트 영역에 갇혀있는다.
	void captureCursor();
	// 커서 캡처가 설정되어있다면 해제한다.
	// captureCursor로 활성화된 커서 캡처를 해제하는 역할을 한다.
	void releaseCursor();
	void hideCursor();
	void showCursor();

	void importNode(std::ifstream& ifs);
	void importTerrain(std::ifstream& ifs, TerrainObject& terrain);

	AssetManager assetManager_{};

	SkillSystem          skillSystem_{};
	SkillDispatchContext skillCtx_{};
	std::vector<Object*>          skillObjectById_{};
	std::vector<ParticleEffect*>  skillVfxById_{};

	PhysicsWorld physicsWorld_{};
	Seconds physicUpdateAcc_{ 0s };				// 물리 업데이트를 위한 시간 누산기
	Seconds physicUpdateInterval{ 1s / 60.f };	// 60fps로 물리 업데이트

	bool skipNextRender_ = false;
	int  physicUpdateScaleK_{ 1 };
	int  consecutiveLagFrames_{ 0 };
	int  consecutiveNonLagFrames_{ 0 };

	bool moveChange_{};
	Seconds moveStateSendAcc_{0s};				// move 패킷 전송을 위한 시간 누산기
	Seconds moveStateSendInterval_{1s / 20.f};	// 50ms(20Hz)마다 move 패킷 전송
	
	AnimSystem animSystem_{};

	GFX gfx_{};
	ThreadPool threadPool_{};
	DebugBVView debugBVView_{};

	EventList eventList_{};
	Timer* pTimer_ = nullptr;

	std::shared_ptr<Cube> ground_{};
	std::shared_ptr<TerrainObject> terrain_{};
	std::vector<std::shared_ptr<Goblin>> goblins_{};
	std::unordered_map<uint16, std::shared_ptr<Goblin>> idGoblinMap_{};

	std::shared_ptr<Player> player_{};
	std::vector<std::shared_ptr<Player>> otherPlayers_{ };
	std::unordered_map<i32t, std::shared_ptr<Player>> idPlayerMap_{ };

	SkyboxObject skybox_{};

	Camera camera_{};
	mu::Radian cameraPitch_ = 0.f;
	// 카메라 yaw는 기본적으로 플레이어에 대한 오프셋으로만 작동하지만,
	// 플레이어 사망 이후에는 이 변수로 작동한다.
	mu::Radian cameraYaw_ = 0.f;

	Light dirLight_{};
	AssetConfigs assetConfigs_{};

	bool playerDead_{};

	UI::UIManager    uiManager_{};
	UI::ProgressBar* playerHpBar_    = nullptr;  // owned by uiManager_
	UI::Dropdown*    effectDropdown_ = nullptr;  // owned by uiManager_

	enum class SwordEffect { SlashWave, SlashCombo, Slash7, Slash1, Spikes, CrystalsFrontAttack, AoESlashGreen, RedEnergyExplosion, CrystalsCrossFade, Arrow, ArrowVolley, ArrowRain, EnergyExplosionArrow, TornadoShot };
	SwordEffect currentEffect_ = SwordEffect::SlashWave;

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
	ParticleSystem dustParticleSystem_{};
	bool      tornadoShotActive_   = false;
	mu::Vec3  tornadoShotPos_{};
	mu::Vec3  tornadoShotDir_{};
	mu::NQuat tornadoShotOrient_{};
	Seconds   tornadoShotElapsed_{ 0s };
	int            footBoneIdxLeft_  = -1;
	int            footBoneIdxRight_ = -1;
	Seconds        prevAnimTimeRun_  = 0s;

	struct OtherPlayerHpEntry {
		Player*          player;       // non-owning; lifetime owned by shared_ptr in otherPlayers_
		UI::ProgressBar* hpBar;        // owned by uiManager_
	};
	std::unordered_map<i32t, OtherPlayerHpEntry> otherPlayerHpBars_{};

	struct GoblinHpEntry {
		Goblin*          goblin;               // non-owning; lifetime owned by shared_ptr in goblins_
		UI::ProgressBar* hpBar;                // owned by uiManager_
		float            worldYOffset;
		float            hpBarVisibleSeconds = 0.f; // 피격 후 HP바 표시 잔여 시간 (초)
	};
	std::unordered_map<uint16, GoblinHpEntry> goblinHpBars_{};

	UI::Label*       hiZStatsLabel_ = nullptr;  // owned by uiManager_

	LONG mouseDeltaX_{};
	LONG mouseDeltaY_{};
	bool cursorCaptureEnabled_ = false;
	bool cursorShowEnabled_ = true;

	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStateCurr_{};
	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStatePrev_{};

	bool inRoom_ = false;

	mu::Vec3 prevVelocity_{};
	mu::Vec3 currVelocity_{};

	u32t nextRenderObjId_ = 0u;
};

}	// namespace Online

#endif	// __Online_game_HPP