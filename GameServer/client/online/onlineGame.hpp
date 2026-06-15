#ifndef __Online_game_HPP
#define __Online_game_HPP

#include <atomic>

#include "../IGame.hpp"

#include "../gfx.hpp"
#include "../object.hpp"
#include "../camera.hpp"
#include "../skybox.hpp"
#include "../light.hpp"
#include "../AssetManager.hpp"
#include "../physicsWorld.hpp"
#include "../terrainChunkManager.hpp"
#include "../zone.hpp"
#include "../animation.hpp"
#include "../event.hpp"
#include "../ui/UIManager.hpp"
#include "../ui/settingsPanel.hpp"
#include "lobbyUI.hpp"
#include "../ui/widgets/ProgressBar.hpp"
#include "../ui/widgets/Label.hpp"
#include "../ui/widgets/Button.hpp"
#include "../ui/widgets/Panel.hpp"
#include "../ui/widgets/Image.hpp"
#include "../ui/widgets/TextInput.hpp"
#include "../spriteAnimation.hpp"
#include "../crosshair.hpp"
#include "../particleSystem.hpp"
#include "../particleEffect.hpp"
#include "../damageNumberSystem.hpp"
#include "../ui/widgets/KillCountWidget.hpp"
#include "../debugBVView.hpp"
#include "../skill/skillSystem.hpp"

class Timer;
class SendBuffer;

namespace Online {

class Game : public IGame {
public:
	// 사용자 입력을 받아 스레드 풀과 GFX 객체를 초기화한다.
	Game();

	// 백그라운드 에셋 로딩 중 종료될 경우, 로딩 워커가 빠르게 빠져나오도록 중단을 요청한다.
	~Game() override;

	GameType type() const override { return GameType::Online; }

	void setTimer(Timer* pTimer) { pTimer_ = pTimer; }

	// 로비 씬으로 진입한다. 최소 UI 리소스만 사용하며,
	// 인게임 리소스는 ThreadPool로 백그라운드 로드를 시작한다.
	void enterLobby();

	// 객체들을 생성한다.
	void setupStage();

	// 3D 비주얼 리소스(지형 청크매니저 + 스카이박스 + 방향광)만 1회 초기화한다.
	// 대기실 3D 배경과 인게임이 공유하며, stageVisualReady_로 중복 init을 막는다.
	void setupStageVisual();

	void setupPlayer(const PlayerInfo& playerInfo);
	void setParticle();
	void setupGround(const ObjectInfo& groundInfo);
	void createOtherPlayer(const ObjectInfo& otherPlayerInfo);
	void createOtherPlayer(const PlayerInfo& otherPlayerInfo);
	void createGoblin(const ObjectInfo& goblinInfo);
	void createStronghold(const ObjectInfo& strongholdInfo);

	void removePlayer( i32t playerId );
	void movePlayer(uint16 playerId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 velocity);
	void rotatePlayer(uint16 playerId, float yawRad);

	void moveGoblin(uint16 npcId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 orient, DirectX::XMFLOAT3 velocity);

	// 서버가 전술 차단벽(barrier)을 토글. active NPC들을 클라에서 '플레이어를 막는 벽'으로
	// 설정/해제한다(resolveBarrierSeparation 대상). PacketManager가 S_NpcBarrier 수신 시 호출.
	void setNpcBarrier(bool active, const std::vector<uint16>& npcIds);

	// 서버가 지정한 NPC들을 즉시 숨김(비표시) 처리. 사망과 달리 시체/래그돌 없이 화면에서 제거하며,
	// 복귀는 onNpcRespawn이 hidden을 해제한다. PacketManager가 S_NpcHide 수신 시 호출.
	// id 기반 조회라 향후 전용 NPC 타입이 분리돼도 이 조회만 통합하면 그대로 동작한다.
	void hideNpcs(const std::vector<uint16>& npcIds);

	void onNpcAttack(uint16 npcId);
	void onPlayerAttack(uint16 attackerId);
	void applyHit(uint16 targetId, int32 newHp);
	void onNpcRespawn( uint16 npcId, int32 newHp, DirectX::XMFLOAT3 spawnPos );
	void onStrongholdState( uint16 strongholdId, int32 hp, uint8 state );
	void onZoneState( uint16 zoneId, uint8 state );
	void onSkillStart( uint16 ownerId, uint32 skillAssetId, uint16 elapsedMs );
	void onSkillHit( uint16 attackerId, uint16 targetId, int32 newHp, uint32 skillAssetId, DirectX::XMFLOAT3 targetVelocity );
	void onPlayerKnockback( uint16 playerId, float dirX, float dirZ, float speed, uint16 knockMs, uint16 postLockMs );
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
	void processInputGame(Milliseconds deltaTime);

	// 씬별 프레임 루틴. update()/render()가 scene_에 따라 분기 호출한다.
	void LobbyScene(Milliseconds deltaTime);
	void renderLobby();
	void renderWaitingRoom();   // 대기실: 3D 맵 배경 + 반투명 UI 오버레이
	void setupLobbyCharacters();   // 대기실 전시 캐릭터 생성(무대 위 일렬)
	void updateLobbyCharacterTransforms();   // 카메라 sway 중에도 화면 슬롯 위치 유지
	void clearLobbyCharacters();   // 전시 캐릭터 제거 + animSystem 트랙 해제
	void InGameScene(Milliseconds deltaTime);
	void renderInGame();
	void updatePlayerHpHudLayout();

	// 로비 -> 인게임 전환. 로비 UI를 숨기고 스테이지/플레이어를 생성한다.
	void enterInGame();

	// 인게임 리소스 백그라운드 로드 시작 (ThreadPool).
	void startInGameAssetLoad();

	// effects/*_ParticleSystems.json을 미리 파싱해 캐시에 적재한다 (백그라운드 워커에서 호출).
	// setParticle()이 디스크 재파싱 없이 캐시에서 config를 꺼내 쓰도록 한다.
	void prefetchParticleConfigs();

	// 로비 UI(메인메뉴/스쿼드/로딩)와 설정창은 LobbyUI/SettingsPanel로 분리됨.
	// refreshLobbyUI는 현재 씬/세션 상태로 ViewState 스냅샷을 만들어 lobbyUI_에 위임한다.
	void refreshLobbyUI();
	float loadProgress01() const;

	// 로비 버튼 액션을 lobbyUI_에 연결하는 콜백 묶음을 만든다.
	LobbyUI::Callbacks makeLobbyCallbacks();

	// 설정창에서 바뀐 디스플레이 설정(해상도/전체화면)을 프레임 안전 지점에서 실제 적용한다.
	// update() 진입부에서 1프레임 지연 호출 — 버튼 콜백 안에서 위젯을 재빌드하면
	// UIManager의 입력 포인터가 dangling되므로 반드시 콜백 밖에서 적용한다.
	void applyPendingDisplaySettings();
	// 창모드/전체화면 전환 + 윈도우/스왑체인/GBuffer/HiZ 재생성 + UIManager 재설정 + 로비/설정 UI 재빌드.
	void applyDisplaySettings();

	// 로비 mock 액션 (script.js 프로토타입 이식).
	void lobbyCreateRoom();
	void lobbyJoinRoom(const std::string& code);
	void lobbyLeaveRoom();
	void lobbyStartGame();

public:
	// LobbyServer 응답 패킷 핸들러 (PacketManager가 메인 스레드 alertable 대기에서 호출).
	void onLobbyCreated(const std::string& code, uint16 myId);
	void onLobbyJoined(bool success, uint16 hostId, uint16 myId, const std::string& code, const std::vector<uint16>& playerIds);
	void onLobbyPlayerJoined(uint16 sessionId);
	void onLobbyPlayerLeft(uint16 sessionId);
	void onGameStart(const std::string& roomServerIp, uint16 roomServerPort, const std::string& lobbyCode);

private:
	// sessionId → 표시 이름(본인은 "나", 그 외 "Player_<id>").
	std::wstring lobbyDisplayName(uint16 sessionId) const;

	void cullObjects();
	void applyHiZCulling();

	// 플레이어 간 reciprocal soft separation (클라 예측).
	// 로컬 플레이어를 다른 플레이어와의 수평 침투량의 "절반"만큼만 밀어낸다.
	// 상대의 절반은 상대 클라가 동일 규칙으로 처리하며, 결과는 기존 C_Move/S_Move로
	// 전파된다(신규 패킷 없음). 매 물리 step마다 step() 직후 호출된다.
	void resolvePlayerSeparation(Seconds dt);

	// 차단벽(barrier) 분리 (클라 예측). barrier 활성 NPC와 수평으로 겹치면 로컬 플레이어를
	// "전체" 침투량만큼 밖으로 밀어낸다(barrier는 움직이지 않는 서버 권위 객체 → 100%를 플레이어가
	// 받음). 위치(setCurrPos)만 보정하므로 임펄스 튕김이 없다. step() 직후 resolvePlayerSeparation
	// 다음에 호출된다.
	void resolveBarrierSeparation(Seconds dt);

	// 커서가 클라이언트 영역 바깥으로 나가지 못하도록 한다.
	// 한번 설정해놓으면, releaseCursor를 호출하기 전까지 커서는 계속 클라이언트 영역에 갇혀있는다.
	void captureCursor();
	// 커서 캡처가 설정되어있다면 해제한다.
	// captureCursor로 활성화된 커서 캡처를 해제하는 역할을 한다.
	void releaseCursor();
	void hideCursor();
	void showCursor();
	void applyCursorPolicy();

	void importNode(std::ifstream& ifs);

	AssetManager assetManager_{};

	// 파티클 이펙트 JSON 파싱 결과 캐시. key = "<파일명>|<relativePath>".
	// 백그라운드 워커(prefetchParticleConfigs)가 채우고, setParticle(메인)이 읽는다.
	std::unordered_map<std::string, ps::ParticleSystemConfig> particleConfigCache_{};

	SkillSystem          skillSystem_{};
	SkillDispatchContext skillCtx_{};
	GroundSampler        groundSampler_{};  // bound to chunkManager_; referenced by skillCtx_.ground
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
	TerrainChunkManager chunkManager_{};
	std::vector<std::shared_ptr<Goblin>> goblins_{};
	std::unordered_map<uint16, std::shared_ptr<Goblin>> idGoblinMap_{};
	// 차단벽 barrier 활성 객체(non-owning; 수명은 goblins_ 등이 소유). resolveBarrierSeparation 대상.
	// Object* 로 두어 고블린 외 몬스터 종류에도 일반화.
	std::vector<Object*> barrierObjects_{};

	// Strongholds are server-authoritative structures; the client renders them as
	// placeholder cubes and tracks HP/destroyed state from server packets. They are
	// Object-derived with an EventBus (no AnimBlender) so Hit/Death route like goblins.
	std::vector<std::shared_ptr<Stronghold>> strongholds_{};

	// Client-local cosmetic trigger zones (BGM/camera/post-fx). Built from
	// chunks_index.bin after the terrain index loads; tested each frame against
	// the predicted local player. zoneStates_ caches server-driven S_ZoneState.
	ZoneSystem clientZoneSystem_{};
	std::unordered_map<uint16, uint8> zoneStates_{};
	void bindZoneHandlers();

	// Virtual walls built locally on S_ZoneState (collision-only, not rendered) so
	// the predicted local player cannot pass. Geometry comes from "Wall" markers.
	std::vector<std::shared_ptr<Cube>> barriers_{};

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
	UI::Image*       playerHpHeart_  = nullptr;  // owned by uiManager_
	UI::Image*       playerWeaponIcon_ = nullptr;  // owned by uiManager_ (하트 위에 겹쳐 그리는 무기 아이콘)
	UI::ProgressBar* playerHpBar_    = nullptr;  // owned by uiManager_
	UI::Label*       playerHpText_   = nullptr;  // owned by uiManager_
	UI::KillCountWidget* killCountWidget_ = nullptr;  // owned by uiManager_
	DamageNumberSystem   damageNumberSystem_{};

	// 로비 2D UI / 재사용 설정창 / 공유 설정 값. 위젯은 uiManager_ 트리가 소유한다.
	GameSettings         settings_{};
	LobbyUI              lobbyUI_{};
	UI::SettingsPanel    settingsPanel_{};
	// 현재 화면에 적용된 디스플레이 설정. settings_와 다르면 applyPendingDisplaySettings가 적용.
	int                  appliedResolutionIndex_ = 0;
	bool                 appliedFullscreen_      = false;  // GameSettings::fullscreen 기본값과 일치
	// 현재 모니터에 맞게 필터된 창모드 해상도 목록. settings_.resolutionIndex가 이 목록을 가리킨다.
	std::vector<Resolution> availableResolutions_{};
	// 현재 모니터 크기로 availableResolutions_를 다시 구성한다(후보 중 모니터에 들어가는 것만).
	void rebuildAvailableResolutions();

	ParticleSystem flameParticleSystem_{};
	ParticleSystem smokeParticleSystem_{};
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

	struct StrongholdHpEntry {
		Stronghold*      obj;          // non-owning; owned by shared_ptr in strongholds_
		UI::ProgressBar* hpBar;        // owned by uiManager_
		float            worldYOffset;
		float            hpBarVisibleSeconds = 0.f;  // remaining seconds to show after a hit
	};
	std::unordered_map<uint16, StrongholdHpEntry> strongholdHpBars_{};

	UI::Label*       hiZStatsLabel_ = nullptr;  // owned by uiManager_

	LONG mouseDeltaX_{};
	LONG mouseDeltaY_{};
	bool cursorCaptureEnabled_ = false;
	bool cursorShowEnabled_ = true;
	// 인게임 설정창(ESC) 열림 전이 추적. 열림→커서 해제/표시, 닫힘→게임플레이 커서 모드 복원.
	bool settingsOpenPrev_ = false;

	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStateCurr_{};
	std::array<BYTE, std::numeric_limits<u8t>::max()> keyboardStatePrev_{};

	bool inRoom_ = false;

	mu::Vec3 prevVelocity_{};
	mu::Vec3 currVelocity_{};

	u32t nextRenderObjId_ = 0u;

	// ---------------------------------------------------------------------
	// Scene / Lobby
	// ---------------------------------------------------------------------
	enum class Scene      { Lobby, InGame };
	enum class LobbyState { MainMenu, WaitingRoom };

	static constexpr int kMaxLobbyPlayers = 4;

	Scene      scene_      = Scene::Lobby;
	LobbyState lobbyState_ = LobbyState::MainMenu;

	// 인게임 리소스 백그라운드 로드 상태.
	// inGameAssetsLoaded_ 는 워커 스레드가 set, 메인 스레드가 read.
	std::atomic<bool> inGameAssetsLoaded_{ false };
	// Phase 1 (lobby/waiting-room 3D assets) done; Phase 2 mesh/texture load done.
	std::atomic<bool> lobbyVisualAssetsLoaded_{ false };
	std::atomic<bool> inGameMeshAssetsLoaded_{ false };
	// Particle prefetch progress (Phase 2 second half).
	std::atomic<u32t> particleFilesTotal_{ 0u };
	std::atomic<u32t> particleFilesDone_{ 0u };
	// 로딩 중 종료 시 set. prefetchParticleConfigs 의 파싱 워커가 폴링해 조기 중단한다.
	std::atomic<bool> assetLoadAbort_{ false };
	bool inGameLoadStarted_ = false;
	bool pendingStart_      = false;   // start pressed while in-game assets still loading
	bool inGameAssetsReady_ = false;   // 메인 스레드에서 1회 처리용
	bool uiBaseReady_       = false;   // UIManager 기본 리소스 초기화 여부

	// LobbyServer 룸 상태
	struct LobbyPlayer {
		uint16       sessionId;
		std::wstring name;
	};
	std::string              roomCode_{};
	bool                     isHost_ = false;
	uint16                   hostId_ = 0;
	uint16                   myId_   = 0;
	std::vector<LobbyPlayer> lobbyPlayers_{};

	// GrandBaum 넉백/이동잠금(로컬 플레이어). 서버 S_PlayerKnockback로 트리거. 이동 권한은 클라에
	// 있으므로 여기서 직접 강제 이동/입력잠금을 실행한다(processInputGame).
	float    knockbackTimer_         = 0.f;   // 남은 강제 이동 시간(s)
	float    knockbackSpeed_         = 0.f;
	mu::Vec3 knockbackDir_           = {};
	float    postKnockbackLockTimer_ = 0.f;   // 넉백 후 입력잠금 남은 시간(s)

	// S_GameStart 핸드오프 요청(onGameStart가 적재, LobbyScene이 에셋 로드 후 실행).
	bool        pendingHandoff_ = false;
	std::string handoffIp_{};
	uint16      handoffPort_ = 0;
	std::string handoffCode_{};

	// 로비 UI 텍스처/위젯/설정 상태는 lobbyUI_ · settingsPanel_ · settings_로 이동했다.
	// 대기실 3D 준비(stageVisualReady_) 후, 로딩 오버레이 뒤에서 실제로 렌더된 프레임 수.
	// 이 값이 kWarmupFrames에 도달해야 오버레이를 내려 팝인(첫 프레임 그려지는 과정)을 가린다.
	// 메인 스레드 전용(render에서 증가, LobbyScene update에서 읽음) — atomic 불필요.
	int              waitingRoomWarmupFrames_ = 0;

	// 대기실 3D 맵 배경
	Camera lobbyCamera_{};
	float  lobbyCameraTime_ = 0.f;                  // 대기실 카메라 연출 시간(느린 좌우 패닝)
	// cross-thread 신호(워커가 Phase 2 시작 전 대기) — atomic.
	std::atomic<bool> stageVisualReady_{ false };
	std::vector<mu::Vec3> stageSpawnPositions_{};  // level.bin PlayerStart 노드 위치
	mu::Vec3 stageFocus_{};                         // 대기실 카메라 포커스(스폰 중심, 지형 높이)
	std::vector<std::shared_ptr<Player>> lobbyChars_{};  // 대기실 전시 캐릭터(물리 없음, idle)
	// 슬롯별 포트레이트 카메라(배경 카메라와 무관, 오프스크린 RT로 렌더). 각 캐릭터는 자기 셀에만
	// 그려지므로 모두 원점에 두고 동일 프레이밍 카메라를 쓴다.
	std::array<Camera, kMaxLobbyPlayers> lobbyPortraitCams_{};
	// 포트레이트 프레이밍 설정값.
	float lobbyPortraitCamDist_   = 3.7f;   // 캐릭터 앞 거리(↑일수록 작게 보임)
	float lobbyPortraitCamHeight_ = 0.95f;  // 카메라 높이(시선)
	float lobbyPortraitLookHeight_= 0.95f;  // 바라보는 높이
	float lobbyPortraitFovYDeg_   = 32.f;   // 수직 FOV(도)
};

}	// namespace Online

#endif	// __Online_game_HPP
