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
#include "../animation.hpp"
#include "../event.hpp"
#include "../ui/UIManager.hpp"
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

	void onNpcAttack(uint16 npcId);
	void onPlayerAttack(uint16 attackerId);
	void applyHit(uint16 targetId, int32 newHp);
	void onNpcRespawn( uint16 npcId, int32 newHp, DirectX::XMFLOAT3 spawnPos );
	void onStrongholdState( uint16 strongholdId, int32 hp, uint8 state );
	void onSkillStart( uint16 ownerId, uint32 skillAssetId, uint16 elapsedMs );
	void onSkillHit( uint16 attackerId, uint16 targetId, int32 newHp, uint32 skillAssetId, DirectX::XMFLOAT3 targetVelocity );
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

	// 씬별 프레임 루틴. update()/render()가 scene_에 따라 분기 호출한다.
	void LobbyScene(Milliseconds deltaTime);
	void renderLobby();
	void renderWaitingRoom();   // 대기실: 3D 맵 배경 + 반투명 UI 오버레이
	void setupLobbyCharacters();   // 대기실 전시 캐릭터 생성(무대 위 일렬)
	void updateLobbyCharacterTransforms();   // 카메라 sway 중에도 화면 슬롯 위치 유지
	void clearLobbyCharacters();   // 전시 캐릭터 제거 + animSystem 트랙 해제
	void InGameScene(Milliseconds deltaTime);
	void renderInGame();

	// 로비 -> 인게임 전환. 로비 UI를 숨기고 스테이지/플레이어를 생성한다.
	void enterInGame();

	// 인게임 리소스 백그라운드 로드 시작 (ThreadPool).
	void startInGameAssetLoad();

	// effects/*_ParticleSystems.json을 미리 파싱해 캐시에 적재한다 (백그라운드 워커에서 호출).
	// setParticle()이 디스크 재파싱 없이 캐시에서 config를 꺼내 쓰도록 한다.
	void prefetchParticleConfigs();

	// 로비 UI 텍스처(배경/로고)를 메인 스레드에서 즉시 로드한다.
	void loadLobbyTextures();

	// 로비 UI 구성/갱신 (LobbyServer 룸 상태 기반).
	void buildLobbyUI();
	void refreshLobbyUI();

	// 로비 액션: LobbyServer로 요청 패킷을 보낸다(상태 변경은 응답 패킷 수신 시).
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

	// 커서가 클라이언트 영역 바깥으로 나가지 못하도록 한다.
	// 한번 설정해놓으면, releaseCursor를 호출하기 전까지 커서는 계속 클라이언트 영역에 갇혀있는다.
	void captureCursor();
	// 커서 캡처가 설정되어있다면 해제한다.
	// captureCursor로 활성화된 커서 캡처를 해제하는 역할을 한다.
	void releaseCursor();
	void hideCursor();
	void showCursor();

	void importNode(std::ifstream& ifs);

	AssetManager assetManager_{};

	// 파티클 이펙트 JSON 파싱 결과 캐시. key = "<파일명>|<relativePath>".
	// 백그라운드 워커(prefetchParticleConfigs)가 채우고, setParticle(메인)이 읽는다.
	std::unordered_map<std::string, ps::ParticleSystemConfig> particleConfigCache_{};

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
	TerrainChunkManager chunkManager_{};
	std::vector<std::shared_ptr<Goblin>> goblins_{};
	std::unordered_map<uint16, std::shared_ptr<Goblin>> idGoblinMap_{};

	// Strongholds are server-authoritative structures; the client renders them as
	// placeholder cubes and tracks HP/destroyed state from server packets.
	std::vector<std::shared_ptr<Cube>> strongholds_{};

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

	enum class SwordEffect { SlashWave, SlashCombo, Slash7, Slash1, Spikes, CrystalsFrontAttack, AoESlashGreen, RedEnergyExplosion, CrystalsCrossFade, Arrow, ArrowVolley, ArrowRain, EnergyExplosionArrow, TornadoShot, Piercing, PiercingSlash, PiercingCircleSlash, PiercingMulti };
	SwordEffect currentEffect_ = SwordEffect::SlashWave;

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
		Cube*            obj;          // non-owning; owned by shared_ptr in strongholds_
		UI::ProgressBar* hpBar;        // owned by uiManager_
		float            worldYOffset;
		bool             destroyed = false;
		float            hpBarVisibleSeconds = 0.f;  // remaining seconds to show after a hit
	};
	std::unordered_map<uint16, StrongholdHpEntry> strongholdHpBars_{};

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
	// 로딩 중 종료 시 set. prefetchParticleConfigs 의 파싱 워커가 폴링해 조기 중단한다.
	std::atomic<bool> assetLoadAbort_{ false };
	bool inGameLoadStarted_ = false;
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

	// 로비 UI 텍스처 (메인 스레드에서 즉시 로드, 인게임 백그라운드 로드와 분리)
	std::unordered_map<std::string, Texture> lobbyTexHashMap_{};
	Texture lobbyBgTex_{};
	Texture lobbyLogoTex_{};
	Texture lobbyPanelTex_{};
	Texture lobbyBtnPrimaryTex_{};
	Texture lobbyBtnSecondaryTex_{};
	Texture lobbyInputTex_{};

	// 로비 UI (소유권은 uiManager_)
	UI::UIElement* lobbyRoot_       = nullptr;
	UI::Button*    lobbySkyBg_      = nullptr;
	UI::Image*     lobbyBgImage_    = nullptr;
	UI::Image*     lobbyLogoImage_  = nullptr;
	UI::UIElement* mainMenuRoot_    = nullptr;
	UI::UIElement* waitingRoomRoot_ = nullptr;
	UI::TextInput* roomCodeInput_   = nullptr;
	UI::Label*     mainMenuMsgLabel_= nullptr;
	UI::Label*     roomCodeLabel_   = nullptr;
	UI::Label*     playerCountLabel_= nullptr;
	// 스쿼드 스테이지 슬롯(4칸). 반투명 배경은 제거하고 번호/이름/뱃지 텍스트만 유지.
	std::array<UI::UIElement*, kMaxLobbyPlayers> slotBays_{};          // 모델 베이(rect만 사용)
	std::array<UI::Label*,     kMaxLobbyPlayers> slotNumberLabels_{};  // "01".."04"
	std::array<UI::Label*,     kMaxLobbyPlayers> slotNameLabels_{};    // 플레이어 이름
	std::array<UI::Label*,     kMaxLobbyPlayers> slotHostBadgeLabels_{};
	std::array<std::array<UI::Button*, 4>, kMaxLobbyPlayers> slotNameBorders_{};  // 호스트 이름 사각 테두리(상/하/좌/우)
	UI::Button*    startGameButton_ = nullptr;
	UI::Label*     startGameLabel_  = nullptr;
	UI::Button*    waitMessageBg_   = nullptr;
	UI::Label*     hostStatusLabel_ = nullptr;

	// 대기실 3D 맵 배경
	Camera lobbyCamera_{};
	float  lobbyCameraTime_ = 0.f;                  // 대기실 카메라 연출 시간(느린 좌우 패닝)
	bool   stageVisualReady_ = false;
	std::vector<mu::Vec3> stageSpawnPositions_{};  // level.bin PlayerStart 노드 위치
	mu::Vec3 stageFocus_{};                         // 대기실 카메라 포커스(스폰 중심, 지형 높이)
	std::vector<std::shared_ptr<Player>> lobbyChars_{};  // 대기실 전시 캐릭터(물리 없음, idle)
	// 전시 캐릭터 배치 튜닝값(대기실에서 키로 런타임 조정 가능).
	float lobbyCharStandDist_  = 4.0f;   // 카메라 앞 거리
	float lobbyCharSpacing_    = 1.5f;   // 좌우 슬롯 간격
	float lobbyCharFootOffset_ = -1.3f;  // 발 높이 오프셋
};

}	// namespace Online

#endif	// __Online_game_HPP
