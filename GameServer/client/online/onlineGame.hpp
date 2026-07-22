#ifndef __Online_game_HPP
#define __Online_game_HPP

#include <atomic>
#include <array>
#include <unordered_map>
#include <unordered_set>

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
#include "../../common/arenaWall.hpp"
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
#include "../energyOrbSystem.hpp"
#include "../pathGuideSystem.hpp"
#include "../mesh.hpp"
#include "../ui/widgets/KillCountWidget.hpp"
#include "../ui/skillDialHUD.hpp"
#include "../ui/minimapHUD.hpp"
#include "../ui/intro/TacticalZoneIntro.hpp"
#include "../ui/dialogue/DialogueSystem.hpp"
#include "../ui/dialogue/TacticalDialogueOverlay.hpp"
#include "../debugBVView.hpp"
#include "../skill/skillSystem.hpp"
#include "../skill/skillLoadout.hpp"

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

	void prepareInGamePartyRoster(uint16 myPlayerId, const std::vector<uint16>& existingPlayerIds);
	void setupPlayer(const PlayerInfo& playerInfo);
	void setParticle();
	void setupGround(const ObjectInfo& groundInfo);
	void createOtherPlayer(const ObjectInfo& otherPlayerInfo);
	void createOtherPlayer(const PlayerInfo& otherPlayerInfo);
	void createGoblin(const ObjectInfo& goblinInfo);
	void createHobgoblin(const ObjectInfo& hobgoblinInfo);
	void createSnake(const ObjectInfo& info);
	void createMushroom(const ObjectInfo& info);
	// Newer monster types each have their own typed vector + per-type loops (like goblins_).
	// configureNetMonster fills the shared state; each create pushes into its own vector.
	void createBomber(const ObjectInfo& info);
	void createBirdy(const ObjectInfo& info);
	void createSlime(const ObjectInfo& info);
	void createTreant(const ObjectInfo& info);
	// Mid-boss bosses: dedicated models, routed through the Treant/Birdy corpse kind so death FX work.
	void createGrandbaum(const ObjectInfo& info);
	void createIsys(const ObjectInfo& info);
	// Final boss: own 14-clip rig (AnimBlenderBoss), 1:1 combat, its own MonsterKind::Boss.
	void createBoss(const ObjectInfo& info);
	void createStronghold(const ObjectInfo& strongholdInfo);

	void removePlayer( i32t playerId );
	void movePlayer(uint16 playerId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 velocity);
	void rotatePlayer(uint16 playerId, float yawRad);

	void moveGoblin(uint16 npcId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 orient, DirectX::XMFLOAT3 velocity);

	// 서버가 전술 차단벽(barrier)을 토글. npcIds는 플레이어 차단+impulse 면역,
	// impulseOnlyNpcId는 차단벽 등록 없이 impulse 면역만 설정한다.
	void setNpcBarrier(bool active, const std::vector<uint16>& npcIds, uint16 impulseOnlyNpcId);

	// 서버가 지정한 NPC들을 즉시 숨김(비표시) 처리. 사망과 달리 시체/래그돌 없이 화면에서 제거하며,
	// 복귀는 onNpcRespawn이 hidden을 해제한다. PacketManager가 S_NpcHide 수신 시 호출.
	// id 기반 조회라 향후 전용 NPC 타입이 분리돼도 이 조회만 통합하면 그대로 동작한다.
	void hideNpcs(const std::vector<uint16>& npcIds);

	void onNpcAttack(uint16 npcId);
	void onPlayerAttack(uint16 attackerId);
	void applyHit(uint16 targetId, int32 newHp, int32 attackerId = -1, uint8 hitAnimIndex = 0);
	void onNpcRespawn( uint16 npcId, int32 newHp, DirectX::XMFLOAT3 spawnPos );
	void onStrongholdState( uint16 strongholdId, int32 hp, uint8 state );
	void onZoneState( uint16 zoneId, uint8 state );
	void onTacticalDialogue( uint16 zoneId, TacticalDialogueId dialogueId );
	void onSkillStart( uint16 ownerId, uint32 skillAssetId, uint16 elapsedMs, uint32 skillSeed );
	void onSkillHit( uint16 attackerId, uint16 targetId, int32 newHp, uint32 skillAssetId, DirectX::XMFLOAT3 targetVelocity, uint8 hitAnimIndex = 0 );
	// Stack-charge skill system (server-authoritative state -> dial / teammate HUD / combo).
	void onSkillCharge( uint16 playerId, uint8 slot, float charge );
	void onSkillSelect( uint16 playerId, uint8 slot );
	void onSkillUseReject( uint8 slot );
	void onComboState( uint16 playerId, uint16 comboCount, float windowMs );
	// Server-authoritative HP push (regen): apply newHp directly, no hit event/animation.
	void onPlayerHp( uint16 playerId, int32 newHp );
	void onPlayerKnockback( uint16 playerId, float dirX, float dirZ, float speed, uint16 knockMs, uint16 postLockMs );
	void onDebugHitboxes( SDebugHitboxPacket* pkt );
	void beginServerTimeSync();
	void onServerTimeSync(uint64 clientSendMs, uint64 serverReceiveMs, uint64 serverSendMs);

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
	// Debug teleport (F5/F6/F7/F9): jump the player to an arena zone center to test arena triggers.
	// Looks up the zone center by tag, sets the local predicted pos, and sends C_DebugTeleport so the
	// server moves its authoritative pos (bypassing the anti-cheat clamp) and the zone Enter fires.
	bool findZoneCenter(const std::string& tag, mu::Vec3& out) const;
	void debugTeleportToArena(const std::string& tag);

	// [임시 디버그] F8 토글로 켜는 로컬 플레이어 이동 속도 배율(1x↔5x). 서버 C_Move(20Hz)×7m/packet
	// 클램프 한도(≈140m/s) 안이라 온라인에서도 러버밴딩 없음. 제거 시 이 멤버 + processInput의 적용부/F8 핸들러만 삭제.
	float debugSpeedMultiplier_{ 1.f };
	void sendAttackPacket();
	void sendSkillStartPacket(uint32 skillAssetId, uint32 skillSeed);
	void sendSelectSkillPacket(uint8 slot);
	void updateServerTimeSync();
	void requestServerTimeSync();
	uint64 estimatedServerTimeMs() const;
	int64  serverClockOffsetMs_{ 0 };
	uint64 nextTimeSyncClientMs_{ 0 };
	bool   serverClockSynchronized_{ false };
	void setupSkillDial(PlayerWeaponType weaponType);   // builds the dial loadout after skills register
	void createOtherPlayerHud(uint16 playerId, Player* player, PlayerWeaponType weaponType);
	// 오른손 소켓에 weaponType에 해당하는 무기를 (재)장착한다. 인게임/로비 포트레이트 공용.
	void equipPlayerWeapon(Object& obj, PlayerWeaponType weaponType);
	// lobbyChars_[i]를 lobbyPlayers_[i].weaponType과 동기화한다(인덱스 1:1 대응).
	void syncLobbyCharacterWeapons();
	void updatePartyHpHudLayout();
	void updatePartyHpHudValues();
	void registerInGamePartyPlayer(uint16 playerId);
	void unregisterInGamePartyPlayer(uint16 playerId);
	std::wstring partyDisplayName(uint16 playerId) const;
	void refreshSkillCtx();

	// Starts a skill locally (prediction visuals) with a fresh per-cast seed
	// and notifies the server via C_SkillStart. No-op if the asset is missing
	// or the player already has an active skill.
	void castSkillByName(std::string_view name);

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
	void setupBossHpHud();
	void showBossHpHud();
	void hideBossHpHud();
	void updateBossHpHud();

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
	void lobbySelectWeapon(int direction);

public:
	// LobbyServer 응답 패킷 핸들러 (PacketManager가 메인 스레드 alertable 대기에서 호출).
	void onLobbyCreated(const std::string& code, uint16 myId);
	void onLobbyJoined(bool success, uint16 hostId, uint16 myId, const std::string& code, const std::vector<LobbyPlayerInfo>& playerInfos);
	void onLobbyPlayerJoined(const LobbyPlayerInfo& info);
	void onLobbyPlayerLeft(uint16 sessionId);
	void onLobbyWeaponSelected(uint16 sessionId, PlayerWeaponType weaponType);
	void onGameStart(const std::string& roomServerIp, uint16 roomServerPort, const std::string& lobbyCode);

private:
	// sessionId → 표시 이름(본인은 "나", 그 외 "Player_<id>").
	std::wstring lobbyDisplayName(uint16 sessionId) const;

	void cullObjects();
	// Hi-Z/frustum 컬링 결과를 Object::hiZCulled_ 및 AnimBlender::culled_에 반영한다.
	// (이전 이름: applyHiZCulling — 컬링 자체를 수행하는 게 아니라 readback 결과를
	//  애니메이션 시스템으로 피드백하는 역할이라 이름을 바꿈)
	void feedbackCullResultToAnim();

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

	// 아레나 후방 Wall 일방향 벽 (클라 예측). 전투 활성 중, 양끝 Wall을 바깥으로 통과하려는
	// 로컬 플레이어만 평면으로 되돌린다. 안쪽 입장·측면 이동은 통과 → 후발 파티원도 합류 가능.
	// 직전 프레임 위치(arenaPrevPlayerPos_) 대비 횡단 방향으로 일방향 판정. 물리 step 루프 뒤 1회 호출.
	void resolveArenaWallLeash();

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
	std::vector<std::shared_ptr<Goblin>>   goblins_{};
	std::vector<std::shared_ptr<Snake>>    snakes_{};
	std::vector<std::shared_ptr<Mushroom>> mushrooms_{};
	// Newer monster types each get their own typed vector + per-type loops, mirroring
	// goblins_/snakes_/mushrooms_. (Grandbaum is stored here as a Treant, Isys as a Birdy —
	// the boss variants route through the Treant/Birdy MonsterKind for corpse/respawn.)
	std::vector<std::shared_ptr<Bomber>>   bombers_{};
	std::vector<std::shared_ptr<Birdy>>    birdys_{};
	std::vector<std::shared_ptr<Slime>>    slimes_{};
	std::vector<std::shared_ptr<Treant>>   treants_{};
	std::vector<std::shared_ptr<Boss>>     bosses_{};
	std::unordered_map<uint16, std::shared_ptr<Goblin>>   idGoblinMap_{};
	std::unordered_map<uint16, std::shared_ptr<Snake>>    idSnakeMap_{};
	std::unordered_map<uint16, std::shared_ptr<Mushroom>> idMushroomMap_{};
	// Non-owning unified monster lookup (all monster types).
	// Object* 로 두어 고블린/뱀/버섯 등 종류와 무관하게 통합 순회한다.
	// ragdoll 등 몬스터 공통 동작은 Object의 가상 접근자로 접근.
	std::unordered_map<uint16, Object*> idMonsterMap_{};
	// 보스/중간보스(Boss/Grandbaum/Isys) npc id 집합. 스폰 시(서버 ObjectType 권위) 기록하며,
	// 미니맵 아이콘이 일반 몬스터(빨강)와 보스(주황)를 구별하는 데 쓴다(RTTI/dynamic_cast 무의존).
	// idMonsterMap_에 없는 id는 순회되지 않으므로 죽은 보스의 잔존 엔트리는 무해(리스폰 시 재삽입).
	std::unordered_set<uint16> bossNpcIds_{};

	// Heat-distortion (boss intimidation) per-boss config + runtime fade state. Keyed by
	// npc id; registered at spawn (createGrandbaum/createIsys/createBoss) with a distinct
	// tint per boss. The emission loop in renderInGame projects each live boss to a
	// screen-space HeatSource (spawn fade-in) and keeps emitting a fading source after the
	// boss leaves idMonsterMap_ (death fade-out), erasing the entry once fully faded.
	struct BossHeatState {
		mu::Vec3 tint{ 0.6f, 0.18f, 0.9f };   // HDR tint color
		float intensity   = 1.0f;             // peak intensity
		float worldRadius  = 2.2f;            // halo radius (meters) at the boss
		float heightBias   = 1.4f;            // center raised above the pivot (meters)
		float aspectY      = 1.7f;            // vertical stretch (rising plume)
		float warpAmp      = 0.006f;          // refraction amplitude (UV)
		float shimmerSpeed = 0.18f;           // upward scroll speed
		// runtime
		mu::Vec3 lastPos{};                   // last known world pivot (for death fade)
		float bornSec      = -1.0f;           // first-seen time (lazy init); spawn fade-in anchor
		float lastSeenSec  = 0.0f;            // last time seen alive (death fade-out clock)
		uint64 lastSeenStamp = 0ull;          // == heatFrame_ when seen alive this frame
	};
	std::unordered_map<uint16, BossHeatState> bossHeatProfiles_{};
	uint64 heatFrame_ = 0ull;   // monotonically increasing render frame stamp for liveness

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
	// Presentation ownership is client-local. Only a local ZoneSystem::Enter
	// may select an arena here; replicated S_ZoneState packets never start UI/BGM.
	int localArenaPresentationZoneId_{ -1 };
	// One-shot latch per arena encounter. Crossing the small authored trigger
	// volume again must not replay the entry presentation during the encounter.
	std::unordered_set<int> localPresentedArenaZoneIds_{};
	// Arenas whose active encounter has ended. Their local trigger remains
	// disabled until the server reports a genuine new 0 -> 1 encounter cycle.
	std::unordered_set<int> completedArenaZoneIds_{};
	void bindZoneHandlers();
	void rebuildBarrierMagicCircleQuads();
	void renderBarrierMagicCircleQuads();
	// Projects each live/dying boss to a screen-space HeatSource and pushes it to GFX
	// (boss intimidation heat distortion). Handles spawn fade-in / death fade-out and
	// erases faded-out entries. Called once per frame from renderInGame.
	void submitBossHeatSources();

	// 아레나 후방 Wall 일방향 벽 상태(S_ZoneState로 토글). 물리 벽 대신 위치 클램프로
	// "들어오기 자유 / 나가기 차단"을 구현한다(서버 Room::move()가 권위로 미러).
	bool arenaLeashActive_{ false };
	std::vector<OneWayWall> arenaWalls_{};   // 활성 아레나의 양끝 일방향 슬랩
	mu::Vec3 arenaPrevPlayerPos_{};          // 직전 프레임 로컬 플레이어 위치(횡단 판정용)

	// Virtual walls built locally on S_ZoneState (collision-only, not rendered) so
	// the predicted local player cannot pass. Geometry comes from "Wall" markers.
	std::vector<std::shared_ptr<Cube>> barriers_{};
	struct BarrierMagicCircleQuad {
		mu::Mat4x4 world{};
		mu::Mat4x4 rotation{};
		mu::Vec3   sortPos{};
		// Color is decided per-frame from the LOCAL player's side of this wall, not the
		// shared S_ZoneState alone: a one-way wall lets latecomers walk in, so a player
		// still outside (passable) must see blue even while another player triggered the
		// arena (state==1). wallCenter/wallOutward define the plane; zoneId picks the state.
		mu::Vec3   wallCenter{};
		mu::Vec3   wallOutward{};   // unit; player on interior side (can't exit) => blocked tint
		uint16     zoneId{ 0 };
	};
	std::vector<BarrierMagicCircleQuad> barrierMagicCircleQuads_{};

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

	// --- Energy orb death FX: client-authored corpse pipeline ---
	// On death a monster is DETACHED from server sync into a corpse (gets a fresh
	// RenderObjectId). The corpse stays a ragdoll for kCorpseRagdollSeconds, then
	// dissolves into energy orbs, and is only removed once all its orbs are absorbed.
	// Server respawns borrow a fresh object from a per-kind pool, so corpse animation
	// is never cut short by a respawn packet.
	EnergyOrbSystem orbSystem_{};

	// Path guidance (cosmetic, client-only): flowing HDR ribbon + guiding wisp.
	// orbProxyMesh_ is the shared free-orb proxy used to render the wisp.
	PathGuideSystem pathGuide_{};
	Mesh            orbProxyMesh_{};

	enum class MonsterKind { Goblin, Snake, Mushroom, Bomber, Birdy, Slime, Treant, Boss };

	// HP bar tracking entry for non-goblin monsters (declared here so configureNetMonster's
	// signature can reference it). Per-type maps below reuse this shape.
	struct MonsterHpEntry {
		Object*          monster;              // non-owning; lifetime owned by typed shared_ptr vectors
		UI::ProgressBar* hpBar;                // owned by uiManager_
		float            worldYOffset;
		float            hpBarVisibleSeconds = 0.f;
	};

	// Shared spawn wiring for the newer monster types (model/blender/body/HP bar + id maps).
	// The caller creates the typed shared_ptr and pushes it into its own vector after this
	// returns; this fills the common state (incl. the HP bar into the passed-in per-type map).
	// Declared after MonsterKind so its signature can reference the enum.
	void configureNetMonster(const std::shared_ptr<Object>& obj, const ObjectInfo& info,
	                         const Model* model, MonsterKind kind, float mass,
	                         std::unordered_map<uint16, MonsterHpEntry>& hpBars,
	                         bool isNamed = false);
	struct PooledMonster { std::shared_ptr<Object> obj; UI::ProgressBar* hpBar = nullptr; };
	struct Corpse {
		std::shared_ptr<Object> obj;        // detached monster (owns ragdoll + mesh)
		UI::ProgressBar* hpBar = nullptr;   // hidden during death; returns to the pool with obj
		MonsterKind kind   = MonsterKind::Goblin;
		uint16 origId      = 0;             // server npc id this corpse came from
		u32t   corpseId    = 0;             // unique id for orb <-> corpse association
		float  age         = 0.f;           // seconds since death
		enum class Phase { Ragdoll, Orb } phase = Phase::Ragdoll;
		bool   orbsSpawned = false;
		float  totalCharge = 0.f;           // credited charge (0 if not a contributor)
		int    slot        = 0;
	};
	std::vector<Corpse> corpses_;
	std::vector<PooledMonster> goblinPool_;
	std::vector<PooledMonster> snakePool_;
	std::vector<PooledMonster> mushroomPool_;
	std::vector<PooledMonster> bomberPool_;
	std::vector<PooledMonster> birdyPool_;
	std::vector<PooledMonster> slimePool_;
	std::vector<PooledMonster> treantPool_;
	std::vector<PooledMonster> bossPool_;
	std::unordered_map<uint16, MonsterKind> respawnKind_;       // npc id -> kind (respawn routing)
	std::unordered_map<uint16, ObjectInfo>  monsterSpawnInfo_;  // npc id -> spawn info (respawn fallback)

	// Network id for a detached corpse. A corpse is client-authored and absent from every
	// id map (idMonsterMap_/idPlayerMap_/skillObjectById_), so it is never looked up by id —
	// every corpse can share ONE fixed sentinel in a range disjoint from server npc ids
	// (small uint16). No per-corpse id allocation -> no counter overflow. The corpse keeps
	// its renderObjectId (stable per object) and reuses it as the orb-association corpseId.
	// On respawn reinitFromPool restores the real server npc id. origId preserves the npc id.
	static constexpr i32t kDetachedCorpseId = 0x40000000;
	// npc ids whose live entity is currently detached as a corpse (no active mapping).
	// Server packets (move/attack) for these arrive during the death->respawn window
	// and are silently ignored instead of logged as "NPC not found". Erased on respawn.
	std::unordered_set<uint16> detachedNpcIds_;

	// Detach a dead monster into corpses_ with a fresh RenderObjectId; removes it from
	// the active server-synced containers (carrying its HP bar). Returns the corpse id.
	u32t migrateToCorpse(const std::shared_ptr<Object>& obj, MonsterKind kind, uint16 npcId);
	// Advances corpses (ragdoll hold -> orb dissolve -> pool return) each frame.
	void updateCorpses(Milliseconds deltaTime, float tPhysicInterp);
	// Reuse a pooled object for a respawn (true), or report the pool empty (false).
	bool reinitFromPool(MonsterKind kind, uint16 npcId, const mu::Vec3& pos, int32 hp);
	// Return a finished corpse's object + HP bar to the per-kind pool for reuse.
	void returnMonsterToPool(Corpse& corpse);
	// Charge credits awaiting their corpse (S_SkillCharge may arrive before migration).
	struct PendingOrbCharge { int slot = 0; float delta = 0.f; float age = 0.f; };
	std::vector<PendingOrbCharge> pendingOrbCharges_;
	float prevServerCharge_[3] = { 0.f, 0.f, 0.f };  // last S_SkillCharge per slot (delta calc)

	// --- Stack-charge skill HUD ---
	SkillDialHUD skillDial_{};
	SkillLoadout skillLoadout_{};

	// --- Minimap (top-left, North-up; background cache re-baked on chunk load/unload) ---
	MinimapHUD minimap_{};
	std::vector<MinimapEntityIcon> minimapIcons_{};
	// World-fixed bake region of the current minimap cache (player-centered + fixed coverage);
	// the HUD scrolls it via a per-frame UV sub-rect. Re-baked (single shared RT) on chunk
	// load/unload or when the player drifts > kMinimapRebakeMoveThreshold from this center.
	mu::Vec3 minimapBakedCenter_{};
	float    minimapBakedCoverage_ = 0.f;
	unsigned     myWeaponOrdinal_    = 0;
	int          dialSlotAssetId_[3] = { -1, -1, -1 };
	int          basicSkillAssetId_  = -1;
	int          wheelAccum_         = 0;     // accumulated WM_MOUSEWHEEL delta
	uint16       comboCount_         = 0;     // last S_ComboState (own combo)
	float        comboWindowMs_      = 0.f;
	float        comboSecLeft_       = 0.f;   // local countdown of the combo window
	// Teammate charge/selection mirror (for the party HP HUD stack indicator).
	std::unordered_map<uint16, std::array<float, 3>> teammateCharge_{};
	std::unordered_map<uint16, uint8>                teammateSelected_{};

	UI::UIManager    uiManager_{};
	UI::Image*       playerHpHeart_  = nullptr;  // owned by uiManager_
	UI::Image*       playerWeaponIcon_ = nullptr;  // owned by uiManager_ (하트 위에 겹쳐 그리는 무기 아이콘)
	UI::ProgressBar* playerHpBar_    = nullptr;  // owned by uiManager_
	UI::Label*       playerHpText_   = nullptr;  // owned by uiManager_
	UI::Label*       playerNameText_ = nullptr;  // owned by uiManager_
	UI::KillCountWidget* killCountWidget_ = nullptr;  // owned by uiManager_
	DamageNumberSystem   damageNumberSystem_{};

	// Final-boss HUD. Presentation is armed only by this client's local Arena_Boss
	// enter callback, so another player entering the room cannot reveal it here.
	UI::UIElement*   bossHpRoot_    = nullptr;  // owned by uiManager_
	UI::ProgressBar* bossHpBar_     = nullptr;  // owned by bossHpRoot_
	UI::Image*       bossHpFrame_   = nullptr;  // owned by bossHpRoot_
	UI::Image*       bossHpEmblem_  = nullptr;  // owned by bossHpRoot_
	Object*          bossHpTarget_  = nullptr;  // non-owning; kept alive by active/corpse/pool storage
	bool             bossHpHudActive_ = false;

	// Tactical arena entry title card (self-contained overlay module; the boss
	// arena adds a WARNING phase). onlineGame only owns it and delegates.
	UI::TacticalZoneIntro tacticalZoneIntro_{};
	UI::TacticalDialogueOverlay tacticalDialogueOverlay_{};

	// Event-driven dialogue/monologue windows (loaded from dialogues.json).
	// Shown when the local player finishes spawning in-game (sample_intro).
	UI::DialogueSystem dialogueSystem_{};

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
		PlayerWeaponType weaponType = PlayerWeaponType::Katana;
		UI::UIElement*   partyRoot = nullptr;       // owned by uiManager_
		UI::Image*       partyHeart = nullptr;      // owned by partyRoot
		UI::Image*       partyWeaponIcon = nullptr; // owned by partyHeart
		UI::Label*       partyNameLabel = nullptr;  // owned by partyRoot
		UI::ProgressBar* partyHpBar = nullptr;      // owned by partyRoot
	};
	std::unordered_map<i32t, OtherPlayerHpEntry> otherPlayerHpBars_{};
	std::vector<uint16> inGamePartyPlayerIds_{};
	// Stable display names ("playerN"), frozen at registration; a member leaving
	// must never renumber the remaining members (see registerInGamePartyPlayer).
	std::unordered_map<uint16, std::wstring> inGamePartyNameById_{};
	uint32 inGamePartyNameSeq_ = 0;

	struct GoblinHpEntry {
		Goblin*          goblin;               // non-owning; lifetime owned by shared_ptr in goblins_
		UI::ProgressBar* hpBar;                // owned by uiManager_
		float            worldYOffset;
		float            hpBarVisibleSeconds = 0.f;
	};
	std::unordered_map<uint16, GoblinHpEntry> goblinHpBars_{};

	// MonsterHpEntry is declared above (near MonsterKind) so configureNetMonster can use it.
	std::unordered_map<uint16, MonsterHpEntry> snakeHpBars_{};
	std::unordered_map<uint16, MonsterHpEntry> mushroomHpBars_{};
	std::unordered_map<uint16, MonsterHpEntry> bomberHpBars_{};
	std::unordered_map<uint16, MonsterHpEntry> birdyHpBars_{};
	std::unordered_map<uint16, MonsterHpEntry> slimeHpBars_{};
	std::unordered_map<uint16, MonsterHpEntry> treantHpBars_{};
	std::unordered_map<uint16, MonsterHpEntry> bossHpBars_{};

	struct StrongholdHpEntry {
		Stronghold*      obj;          // non-owning; owned by shared_ptr in strongholds_
		UI::ProgressBar* hpBar;        // owned by uiManager_
		float            worldYOffset;
		float            hpBarVisibleSeconds = 0.f;  // remaining seconds to show after a hit
	};
	std::unordered_map<uint16, StrongholdHpEntry> strongholdHpBars_{};

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
		PlayerWeaponType weaponType = PlayerWeaponType::Katana;
	};
	std::string              roomCode_{};
	bool                     isHost_ = false;
	uint16                   hostId_ = 0;
	uint16                   myLobbyId_ = 0;
	PlayerWeaponType         selectedLobbyWeapon_ = PlayerWeaponType::Katana;
	std::vector<LobbyPlayer> lobbyPlayers_{};

	// Grandbaum 넉백/이동잠금(로컬 플레이어). 서버 S_PlayerKnockback로 트리거. 이동 권한은 클라에
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
