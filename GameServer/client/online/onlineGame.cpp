#include "pch.hpp"
#include "onlineGame.hpp"
#include "../errorHandling.hpp"
#include "../binaryImport.hpp"
#include "../timer.hpp"
#include "../particleImporter.hpp"
#include "SendBuffer.hpp"
#include "../PacketManager.hpp"
#include "../ClientApp.hpp"
#include "../sound/soundManager.hpp"
#include "../ui/widgets/Button.hpp"
#include "../ui/digitAtlas.hpp"
#include "../skill/skillCompiler.hpp"

extern HWND ghWnd;
extern RECT gClientRect;
// main.cpp: 창모드/전체화면(borderless)을 전환하며 윈도우·전역 RECT를 갱신하고
// 결과 클라이언트 크기를 out*에 돌려준다(런타임 디스플레이 모드 변경).
extern void applyDisplayMode(bool fullscreen, int windowedW, int windowedH, int* outClientW, int* outClientH);
// main.cpp: 현재 윈도우가 걸친 모니터의 전체 해상도를 돌려준다.
extern void getCurrentMonitorSize(int* outW, int* outH);

namespace Online {

// ---------------------------------------------------------------------------
// Player movement parameters — tweak these to adjust game feel.
// See standalone/game.cpp for the derivation of kPlayerAccelRate.
// ---------------------------------------------------------------------------
static constexpr float kPlayerMaxSpeed      = 10.f;
static constexpr float kPlayerLinearDamping = 12.f;
static constexpr float kPlayerAccelRate     = kPlayerMaxSpeed * kPlayerLinearDamping;

// ---------------------------------------------------------------------------
// 충돌 레이어 — 플레이어끼리는 hard contact(ContactConstraint)를 만들지 않고
// resolvePlayerSeparation()의 soft separation으로 처리한다.
// generateContacts()의 (group & mask)==0 필터로 플레이어-플레이어 쌍을 제외한다.
// ---------------------------------------------------------------------------
static constexpr uint16_t kLayerDefault        = 1 << 0;
static constexpr uint16_t kLayerPlayer         = 1 << 1;
static constexpr uint16_t kPlayerCollisionMask = static_cast<uint16_t>(0xFFFF & ~kLayerPlayer);

// ---------------------------------------------------------------------------
// 플레이어 간 reciprocal soft separation 파라미터 (게임 느낌 튜닝).
//   - 반경을 시각 캡슐보다 살짝 작게 잡으면 가벼운 스침을 허용해 더 자연스럽다.
//   - 분리 속도 상한으로 깊은 침투에서도 "튕김" 없이 부드럽게 밀려난다.
// ---------------------------------------------------------------------------
static constexpr float kPlayerSeparationRadius = 0.4f;  // 플레이어 수평(XZ) 반경 (m)
static constexpr float kMaxSeparationSpeed     = 4.f;   // 분리 보정 속도 상한 (m/s)
static constexpr float kSeparationStiffness    = 1.0f;  // 0~1, 1=이번 step에 절반 전부 보정

// ---------------------------------------------------------------------------
// 차단벽(barrier) 분리 파라미터.
//   - barrier는 움직이지 않는 서버 권위 객체 → 플레이어가 침투량 "전체"를 받는다(절반 아님).
//   - 부드러운 4m/s cap이 아니라 전체 침투를 해소해야 고속 충돌에도 벽을 못 뚫는다.
//     단, 패킷 지터/순간 큰 겹침에서 순간이동을 막기 위한 넉넉한 상한만 둔다.
// ---------------------------------------------------------------------------
static constexpr float kBarrierRadius          = 0.6f;  // 차단벽 NPC 수평(XZ) 반경 (m)
static constexpr float kMaxBarrierPushPerStep  = 0.5f;  // step당 최대 보정 (m) — 순간이동 방지용 상한
// 인접 barrier를 선분으로 잇는 최대 거리(m). NPC-NPC 충돌 바닥(~1.6m)+편차(최대 ~2.7m)를 덮어 틈을 봉합하되,
// 죽은 NPC 양옆 간격(~3.2m)·좌우 라인(회랑 폭 ≫)은 안 이어 구멍/탈출구를 보존하는 사이값.
static constexpr float kBarrierLinkDist        = 2.9f;

// ---------------------------------------------------------------------------
// 최종 보스 분리 파라미터. 보스는 밀리지 않는 서버 권위 객체(거대한 나무)라 barrier와 동일하게
// 플레이어가 침투량 전체를 받는다. 서버는 플레이어↔보스 접촉을 필터링하므로(Room::setupFinalBoss)
// 여기가 유일한 차단 수단이다.
//
// ⚠ 불변식: (kPlayerSeparationRadius + kBossSeparationRadius) 는 보스가 접근해 멈추려는
// 거리보다 작아야 한다 — 서버 `attackCommitRange(1.8m) × PRESS_HOLD_FRACTION(0.7) = 1.26m`.
// 크면 보스가 목표 거리까지 붙지 못해 플레이어를 계속 밀어붙이며 아레나를 배회한다.
//   현재: 0.4 + 0.7 = 1.1m  <  1.26m  (여유 0.16m)
// 몸통 굵기를 키우려면 서버의 두 상수도 같이 올려야 한다.
// ---------------------------------------------------------------------------
static constexpr float kBossSeparationRadius   = 0.7f;  // 보스 몸통(trunk) 수평 반경 (m)
static constexpr float kMaxBossPushPerStep     = 0.5f;  // step당 최대 보정 (m) — 순간이동 방지 상한

static constexpr int     kRenderSkipLagFrames = 4;
static constexpr int     kMaxPhysicsStepsPerFrame = 3;
static constexpr Seconds kMaxPhysicsDeltaTime{ 1.f / 60.f * kMaxPhysicsStepsPerFrame };
static constexpr int     kMaxPhysicsScaleK    = 4;
static constexpr int     kLagScaleUpFrames    = 2;
static constexpr int     kLagScaleDownFrames  = 100;
static constexpr uint64  kTimeSyncIntervalMs  = 10000;
static constexpr int64   kMaxAcceptedSyncRttMs = 2000;

// S_NpcMoveBatch cadence: RoomServer broadcasts it once per room tick (60Hz), unlike the
// 20Hz S_Move stream for remote players. Monster network interpolation must use THIS window,
// not Object's 20Hz default -- see configureNetMonster.
static constexpr Seconds kNpcMoveInterval{ 1.f / 60.f };
// How long a monster may go without a move packet before its velocity is treated as stale and
// zeroed (stops the anim blender from walking in place forever). Deliberately NOT derived from
// the interpolation window: at 60Hz that would be a 33ms deadline, so any ordinary network
// jitter would blink the monster into its idle pose.
static constexpr Seconds kNpcMoveStaleTimeout{ 0.15f };

static uint64 networkNowMs() {
	return static_cast<uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
}

static const mu::Vec3 kLobbyCameraBaseEye(5142.34f, 84.0199f, 5125.55f);
static const mu::Vec3 kLobbyCameraBaseAt(5140.71f, 83.0828f, 5123.8f);

static const mu::Vec4 kBarrierMagicBlockedColor{ 1.0f, 0.12f, 0.08f, 0.82f };
static const mu::Vec4 kBarrierMagicPassableColor{ 0.10f, 0.48f, 1.0f, 0.72f };
static constexpr float    kBarrierMagicMinDiameter = 8.f;
static constexpr float    kBarrierMagicMaxDiameter = 24.f;
static constexpr int      kBarrierMagicRenderOrder = 4;
static const mu::Mat4x4   kBarrierMagicQuadPlaneFix = mu::rotateYH(mu::Degree(-90.f));
// 로컬 플레이어가 벽 평면의 안/바깥 어느 쪽인지 판정하는 임계값(clampOneWayWall과 동일 부호 규칙).
// outward 기준 부호거리 <= eps 면 안쪽 = 나갈 수 없음(빨강), 초과면 바깥 = 입장 가능(파랑).
static constexpr float    kBarrierMagicBlockedSideEps = 0.01f;

static float barrierMagicDiameter(const MarkerDef& m) {
	const float wallHeight = std::fabs(m.scale.y());
	return std::clamp(wallHeight * 0.82f, kBarrierMagicMinDiameter, kBarrierMagicMaxDiameter);
}

// 입장 표시 마법진(magic circle)의 지역별 위치/회전 보정 테이블.
//
// Wall 마커 하나가 ① 물리 일방향 벽(makeOneWayWall, 실제 벽 슬랩 위)과 ② 입장 표시 마법진
// (실제 통과 길목 중앙)을 겸한다. 벽 슬랩이 길목보다 넓거나 한쪽으로 치우치면 둘이 어긋나
// 마법진이 길목에서 벗어나고(위치), 마커 방향에 따라 비뚤게 보일 수도 있다(회전). 그 차이를
// 마커 이름별 로컬 오프셋 + 추가 회전으로 보정한다.
//
// offset    : 마커 로컬 위치 오프셋(circlePos에서 m.orient.rotate 적용). 보통 벽 폭(span) 축으로
//             밀어 길목 중앙에 맞춘다. y=높이, z/x=수평 슬라이드.
// rotateDeg : 마커 로컬 축 기준 추가 회전(도). x/y/z = 마커 로컬 X/Y/Z축 회전. planeFix와
//             markerOrient 사이에 적용되어 벽 방향을 기준으로 돈다. 원하는 각이 나올 때까지
//             축을 바꿔가며 조절. 둘 다 0이면 보정 없음(현재 동작 그대로).
struct BarrierMagicAdjust {
	mu::Vec3 offset{};      // 마커 로컬 위치 오프셋
	mu::Vec3 rotateDeg{};   // 마커 로컬 추가 회전(도, x/y/z축)
};

static const std::unordered_map<std::string, BarrierMagicAdjust>& barrierMagicAdjustTable() {
	// chunks_index.bin의 모든 Wall 마커. { offset(x,y,z), rotateDeg(x,y,z) }. 표에 없으면 보정 없음.
	static const std::unordered_map<std::string, BarrierMagicAdjust> kTable{
		// 보스 아레나(Arena_Boss)
		{ "WallBoss",         { mu::Vec3{ 0.f,   0.f,   0.f }, mu::Vec3{ 0.f, 0.f, 0.f } } },
		// 그랜드밤 아레나(Arena_Grandbaum)
		{ "WallGrandbaum_0",  { mu::Vec3{ 0.f,   0.f, -10.f }, mu::Vec3{ 0.f, 0.f, 0.f } } },
		{ "WallGrandbaum_1",  { mu::Vec3{ 0.f,   0.f,  0.f }, mu::Vec3{ 0.f, 0.f, 0.f } } },
		{ "WallGrandbaum_2",  { mu::Vec3{ 0.f,   0.f,  -5.0f }, mu::Vec3{ 0.f, 0.f, 0.f } } },
		// 홉고블린 아레나(Arena_Hobgoblin)
		{ "WallHobgoblin_0",  { mu::Vec3{ 0.f,   0.f,  12.f }, mu::Vec3{ 0.f, 0.f, 0.f } } },   // 길목 중앙으로 +12(기존 값)
		{ "WallHobgoblin_1",  { mu::Vec3{ 0.f,  -5.f,   7.f }, mu::Vec3{ 0.f, 0.f, 0.f } } },
		// 이시스 아레나(Arena_Isys)
		{ "WallIsys_0",       { mu::Vec3{ 0.f, -10.f, -20.f }, mu::Vec3{ 0.f, 45.f, 0.f } } },
		{ "WallIsys_1",       { mu::Vec3{ 0.f,  -5.f, -15.f }, mu::Vec3{ 0.f, 0.f, 0.f } } },
		{ "WallIsys_2",       { mu::Vec3{ 0.f,   0.f,   0.f }, mu::Vec3{ 0.f, 0.f, 0.f } } },
	};
	return kTable;
}

static BarrierMagicAdjust barrierMagicAdjust(const MarkerDef& m) {
	const auto& table = barrierMagicAdjustTable();
	if (auto it = table.find(m.name); it != table.end()) return it->second;
	return {};
}

// "Arena_X" zone 태그 -> "WallX" 마커 prefix. 아레나 후방 Wall 일방향 벽(arenaWalls_)과 장식용
// 마법진(rebuildBarrierMagicCircleQuads) 둘 다 같은 규칙으로 마커를 묶는다.
static std::string arenaWallPrefix(const ZoneDef& z) {
	constexpr char kArenaTag[] = "Arena_";
	if (z.tag.rfind(kArenaTag, 0) != 0) return {};
	return "Wall" + z.tag.substr(sizeof(kArenaTag) - 1);
}

static constexpr float   kArrowRainRadius          = 4.75f;
static constexpr int     kArrowVolleyCount          = 9;
static constexpr float   kPiercingMultiRadius       = 2.0f;
static constexpr int     kPiercingMultiWaveCount    = 10;
static constexpr int     kPiercingMultiBurstCount   = 2;
static constexpr float   kPiercingMultiInterval     = 0.06f;
static constexpr float   kPiercingMultiLifetime     = 0.42f;
static constexpr float   kPiercingMultiHalfWidth    = 2.5f;
static constexpr float   kPiercingMultiHalfHeight   = 1.25f;
static constexpr float   kArrowVolleySpreadDegrees  = 56.f;
static constexpr float   kPlayerHpUiX               = 20.f;
static constexpr float   kPlayerHpUiY               = 20.f;
static constexpr float   kPlayerWeaponBadgeSize     = 60.f;
static constexpr float   kPlayerWeaponIconScale     = 50.f;  // 배지 크기 대비 % (해상도 무관, 부모 비율로 스케일)
static constexpr float   kPlayerWeaponBadgeBarOverlap = 12.f;
static constexpr float   kPlayerHpBarHeight         = 18.f;
static constexpr float   kPlayerNameLabelHeight     = 20.f;
static constexpr float   kPlayerNameBadgeGap        = 4.f;
static constexpr float   kPartyHpStartYOffset       = 165.f;
static constexpr float   kPartyHpRowHeight          = 56.f;
static constexpr float   kPartyWeaponBadgeSize      = 48.f;
static constexpr float   kPartyWeaponIconScale      = 50.f;
static constexpr float   kPartyWeaponBadgeBarOverlap = 8.f;
static constexpr float   kPartyNameBadgeGap         = 4.f;
static constexpr float   kPartyHpBarWidth           = 230.f;
static constexpr float   kPartyHpBarHeight          = 14.f;
static constexpr float   kPartyHpNameHeight         = 20.f;
static constexpr float   kBossHpHudWidth            = 614.f;
static constexpr float   kBossHpHudHeight           = 66.f;
static constexpr float   kBossHpHudTop              = 28.f;
static constexpr float   kBossHpFillX               = 109.f;
static constexpr float   kBossHpFillY               = 8.f;
static constexpr float   kBossHpFillWidth           = 464.f;
static constexpr float   kBossHpFillHeight          = 56.f;
static constexpr float   kBossHpEmblemX              = 46.f;
static constexpr float   kBossHpEmblemY              = 12.f;
static constexpr float   kBossHpEmblemSize           = 41.f;
static const DirectX::XMFLOAT4 kNamedMonsterHpColor{ 0.62f, 0.24f, 0.90f, 1.f };
static constexpr float   kConfusionIconBaseSize      = 40.f;
static constexpr float   kConfusionIconWorldGap      = 0.65f;
static constexpr float   kConfusionIconPulseAmount   = 0.08f;
static constexpr float   kConfusionIconPulseHz       = 2.0f;
static constexpr float   kConfusionIconBobPixels     = 4.f;
static constexpr float   kConfusionIconBobHz         = 1.5f;
static constexpr float   kTwoPi                      = 6.28318530718f;

Game::Game() {
	// 스레드 풀 초기화
	std::cout << "----------[게임 초기화 설정]----------\n";
	std::cout << "스레드 풀에 사용할 스레드 수를 입력해 주세요.\n";
	std::cout << "컴퓨터의 물리 코어 수: " << numberOfPhysicalCores() << '\n';
	std::cout << "사용 가능한 물리 코어 수: " << numberOfPhysicalCores() - 1 << " (1개 - 메인 스레드)\n";
	std::cout << "스레드 수: ";

	std::size_t threadCnt{ 4u };
	std::cout << threadCnt << '\n';
	// std::cin >> threadCnt;

	threadPool_.run(threadCnt);

	// GFX 객체 초기화
	gfx_.setupDXGI(D3D_FEATURE_LEVEL_12_1);
	gfx_.init();
	gfx_.createSwapChain();
	gfx_.setThreadPool(&threadPool_);

	// 로비 표시에 필요한 공용 GPU 리소스만 즉시 초기화한다.
	// 무거운 인게임 리소스(모델/지형/파티클)는 로비 진입 후 백그라운드로 로드한다.
	gfx_.initSharedResources( assetConfigs_ );

	std::string inventoryError;
	DISPLAY_ERROR_STR(
		itemCatalog_.load("../resources/data/inventory.json", inventoryError),
		"[Inventory] config load failed: "s + inventoryError, true);
	DISPLAY_ERROR_STR(
		inventory_.initializeEmpty(itemCatalog_, &inventoryError),
		"[Inventory] initialization failed: "s + inventoryError, true);
}

Game::~Game() {
	// 소멸자 본문은 gfx_ 같은 멤버 소멸보다 먼저 실행된다. 종료 중 백그라운드 로딩/지형
	// 작업이 GFX 리소스를 계속 만지면 GPU 큐가 깨질 수 있으므로, 먼저 워커를 모두 멈춘다.
	assetLoadAbort_.store(true, std::memory_order_relaxed);
	threadPool_.stop();

	// 멤버 소멸 전에 GPU를 드레인한다. gfx_보다 뒤에 선언된 멤버(지형 청크, 파티클,
	// UI 텍스처 등)는 ~GFX의 펜스 대기보다 먼저 파괴되므로, 드레인 없이는 GPU가
	// 실행 중인 명령이 참조하는 리소스를 해제하게 된다. 이 디바이스 폴트가 TDR을
	// 일으켜 같은 GPU의 다른 클라이언트까지 디바이스 제거(DEVICE_HUNG)로 먹통이 됐다.
	gfx_.drainGpu();
}

void Game::setupStageVisual() {
	if (stageVisualReady_.load(std::memory_order_acquire)) {
		return;
	}

	// 레벨(level.bin) 파싱 — PlayerStart 등 노드 정보를 읽는다(스폰 위치 캡처).
	{
		const auto path = std::filesystem::path("../resources/levels/level.bin");
		auto ifs = std::ifstream(path, std::ios::binary);
		DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: setupStageVisual: "s + path.string() + " 파일을 열 수 없습니다."s, true);

		readHeadTag(ifs, "Level");
		const auto nodeCnt = readInteger(ifs, "NodeCnt");
		stageSpawnPositions_.clear();
		importNode(ifs);
		readTailTag(ifs, "Level");
	}

	// 지형 청크 스트리밍 매니저 초기화 (팔레트 + 인덱스 로드, 동기 baseline 로드).
	chunkManager_.init(gfx_, physicsWorld_, &threadPool_, "../resources/terrains/");

	// 트리거 존 빌드 (연출용 로컬 존). 게임플레이 존은 서버 권위라 클라는 핸들러 미바인딩.
	clientZoneSystem_.build(chunkManager_.zones());
	bindZoneHandlers();
	rebuildBarrierMagicCircleQuads();

	// Path guidance (cosmetic, client-only): build polylines from "PathPt" markers
	// and create the shared free-orb proxy mesh used to render the guiding wisp.
	pathGuide_.build(chunkManager_.markers());
	if (orbProxyMesh_.subMeshes.empty()) {
		gfx_.recordTerrainResourceLoad(
			[this](ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, DescriptorPool&, Fence& fence) {
				orbProxyMesh_ = buildOrbProxyMesh(device, cmdList, fence, 128u);
			},
			/*wait=*/ true
		);
	}

	skybox_.setModel( assetManager_.modelCube( ) );
	skybox_.setSkyboxMaterial( assetManager_.skyboxMaterial( ) );

	dirLight_.setOrient(mu::NQuat(mu::Degree(0.f), mu::Degree(122.f), mu::Degree(66.f)));
	// dirLight_.setOrient( mu::NQuat( mu::Degree( 0.f ), mu::Degree( 132.f ), mu::Degree( 180.f ) ) );
	dirLight_.color = mu::Vec3( 0.9f, 0.86f, 0.66f );
	dirLight_.intensity = 7.5f;
	dirLight_.type = PBRPipeline::LightData::Type::DirectionalLight;
	dirLight_.isMainDirectionalLight = true;

	// 대기실 카메라 포커스: 스폰 위치 평균 XZ를 지형 높이에 앉힌다(없으면 지형 중심).
	if (!stageSpawnPositions_.empty()) {
		float sx = 0.f, sz = 0.f;
		for (const auto& p : stageSpawnPositions_) { sx += p.x(); sz += p.z(); }
		const float n = static_cast<float>(stageSpawnPositions_.size());
		const float cx = sx / n, cz = sz / n;
		stageFocus_ = mu::Vec3(cx, chunkManager_.heightAtWorld(cx, cz), cz);
	} else {
		stageFocus_ = chunkManager_.worldCenter();
	}

	// 스킨드 객체의 renderObjectId 인덱싱이 안전하도록 가시성 배열을 확보한다. renderObjectId는
	// 객체당 1회만 발급되고(풀 재사용 시에도 유지) 범람하지 않으므로, 동시 객체 수 + 여유분으로 잡는다.
	gfx_.setMaxRenderObjectId(10000u);

	// release: 워커가 이 신호를 보고 Phase 2 로드를 시작한다(GFX 로딩 직렬화).
	stageVisualReady_.store(true, std::memory_order_release);
}

void Game::setupLobbyCharacters() {
	for (int i = 0; i < kMaxLobbyPlayers; ++i) {
		auto ch = std::make_shared<Player>();

		// 가시성 배열 인덱싱이 OOB가 되지 않도록 고유 renderObjectId를 부여한다.
		// (기본값은 UINT_MAX. 로비 동안엔 전시 캐릭터만 렌더되므로 0..kMaxLobbyPlayers-1 고정 사용.)
		ch->setRenderObjectId(static_cast<u32t>(i));

		ch->setScale(mu::Vec3(1.f, 1.f, 1.f));
		ch->setModel(assetManager_.modelPlayer());
		ch->setAnimBlender(animSystem_, assetManager_);   // AnimBlenderPlayer: 정지 시 idle
		lobbyChars_.push_back(std::move(ch));
	}

	updateLobbyCharacterTransforms();
	syncLobbyCharacterWeapons();
}

void Game::updateLobbyCharacterTransforms() {
	if (lobbyChars_.empty()) return;

	// 포트레이트는 슬롯마다 오프스크린 RT 셀에 '독립적으로' 렌더되므로(각 디스패처가 자기 슬롯
	// 캐릭터만 그림), 모든 캐릭터를 원점에 두고 동일한 정면 프레이밍 카메라를 쓴다. 배경 카메라
	// sway와 완전히 분리되며, 슬롯 정렬은 UI 쿼드(resolvedRect_)가 픽셀 단위로 보장한다.
	// 모델 forward는 +Z이고 카메라를 +Z쪽에 두므로, yaw=0이면 캐릭터 정면이 카메라를 향한다.
	const float aspect = static_cast<float>(GFX::kPortraitCellW) / static_cast<float>(GFX::kPortraitCellH);
	const mu::Vec3 eye(0.f, lobbyPortraitCamHeight_, lobbyPortraitCamDist_);
	const mu::Vec3 at (0.f, lobbyPortraitLookHeight_, 0.f);

	for (int i = 0; i < static_cast<int>(lobbyChars_.size()); ++i) {
		auto& ch = lobbyChars_[i];
		if (ch) {
			ch->setPos(mu::Vec3(0.f, 0.f, 0.f));
			ch->setOrient(mu::NQuat(mu::Radian(0.f), mu::Radian(0.f), mu::Radian(0.f)));
		}
		if (i < static_cast<int>(lobbyPortraitCams_.size())) {
			lobbyPortraitCams_[i].setPerspective(mu::Degree(lobbyPortraitFovYDeg_), aspect, 0.1f, 20.f);
			lobbyPortraitCams_[i].setView(eye, at);
		}
	}
}

void Game::clearLobbyCharacters() {
	for (auto& ch : lobbyChars_) {
		if (ch && ch->animBlender()) {
			animSystem_.untrackAnimBlender(ch->animBlender());
		}
	}
	lobbyChars_.clear();

	// 포트레이트 패스 비활성화(인게임/메인메뉴 프레임에서 스킵) + 슬롯 이미지 숨김.
	gfx_.setLobbyPortraitActive(false);
	lobbyUI_.hideAllSlotBays();
}

void Game::resetInGameSession() {
	// 이 Game 인스턴스는 LobbyServer 연결과 함께 여러 경기에 걸쳐 재사용된다.
	// 이전 경기 객체가 남으면 새 Room이 같은 npc id를 재사용했을 때 create*의 중복
	// 스폰 방지가 새 객체 생성을 건너뛰고, 이전 객체가 이동/피격에서 분리된 유령이 된다.
	const bool hadSessionObjects =
		player_ || !otherPlayers_.empty() || !idMonsterMap_.empty()
		|| !corpses_.empty() || !strongholds_.empty();
	if (hadSessionObjects) {
		// 객체와 그 UI가 직전 프레임 GPU 제출에서 참조됐을 수 있다.
		gfx_.drainGpu();
	}

	uiManager_.resetInteractionState();
	camera_.cancelFocusCinematic();
	camera_.setTargetObject(std::shared_ptr<Object>{});

	// HP/UI 엔트리는 Object*를 비소유로 들고 있으므로 객체보다 먼저 숨긴다.
	auto hideBars = [](auto& bars) {
		for (auto& [id, entry] : bars) {
			if (entry.hpBar) entry.hpBar->visible = false;
		}
	};
	hideBars(goblinHpBars_);
	hideBars(snakeHpBars_);
	hideBars(mushroomHpBars_);
	hideBars(bomberHpBars_);
	hideBars(birdyHpBars_);
	hideBars(slimeHpBars_);
	hideBars(treantHpBars_);
	hideBars(bossHpBars_);
	hideBars(strongholdHpBars_);
	for (auto& [id, entry] : otherPlayerHpBars_) {
		if (entry.hpBar) entry.hpBar->visible = false;
		if (entry.partyRoot) entry.partyRoot->visible = false;
	}
	for (auto& [id, entry] : npcStatusIcons_) {
		if (entry.icon) entry.icon->visible = false;
	}
	for (auto& corpse : corpses_) {
		if (corpse.hpBar) corpse.hpBar->visible = false;
	}
	auto hidePoolBars = [](auto& pool) {
		for (auto& entry : pool) {
			if (entry.hpBar) entry.hpBar->visible = false;
		}
	};
	hidePoolBars(goblinPool_);
	hidePoolBars(snakePool_);
	hidePoolBars(mushroomPool_);
	hidePoolBars(bomberPool_);
	hidePoolBars(birdyPool_);
	hidePoolBars(slimePool_);
	hidePoolBars(treantPool_);
	hidePoolBars(bossPool_);

	// AnimSystem/PhysicsWorld는 비소유 raw pointer를 보관한다. 모든 소유 컨테이너에서
	// 객체를 모아 중복 없이 등록을 해제한 뒤 shared_ptr을 파괴해야 한다.
	std::unordered_set<Object*> sessionObjects;
	auto remember = [&sessionObjects](const auto& object) {
		if (object) sessionObjects.insert(object.get());
	};
	remember(player_);
	for (const auto& object : otherPlayers_) remember(object);
	for (const auto& [id, object] : idPlayerMap_) remember(object);
	for (const auto& object : goblins_) remember(object);
	for (const auto& object : snakes_) remember(object);
	for (const auto& object : mushrooms_) remember(object);
	for (const auto& object : bombers_) remember(object);
	for (const auto& object : birdys_) remember(object);
	for (const auto& object : slimes_) remember(object);
	for (const auto& object : treants_) remember(object);
	for (const auto& object : bosses_) remember(object);
	for (const auto& object : strongholds_) remember(object);
	for (const auto& object : barriers_) remember(object);
	for (const auto& corpse : corpses_) remember(corpse.obj);
	auto rememberPool = [&remember](const auto& pool) {
		for (const auto& entry : pool) remember(entry.obj);
	};
	rememberPool(goblinPool_);
	rememberPool(snakePool_);
	rememberPool(mushroomPool_);
	rememberPool(bomberPool_);
	rememberPool(birdyPool_);
	rememberPool(slimePool_);
	rememberPool(treantPool_);
	rememberPool(bossPool_);

	for (Object* object : sessionObjects) {
		if (object->animBlender()) {
			animSystem_.untrackAnimBlender(object->animBlender());
		}
		if (Ragdoll* ragdoll = object->ragdoll()) {
			ragdoll->destroy(physicsWorld_);
		}
		physicsWorld_.unregisterBody(&object->body());
	}

	// SkillSystem의 실행 인스턴스에는 owner/target raw pointer가 있으므로 id 맵과
	// 객체를 비우기 전에 런타임 전체를 폐기한다. 다음 S_Enter의 setupPlayer가
	// 에셋과 dispatch context를 새 경기 객체로 다시 구성한다.
	skillSystem_ = SkillSystem{};
	skillCtx_ = SkillDispatchContext{};
	skillObjectById_.clear();
	skillVfxById_.clear();
	clearEvents(eventList_);

	player_.reset();
	otherPlayers_.clear();
	idPlayerMap_.clear();
	goblins_.clear();
	snakes_.clear();
	mushrooms_.clear();
	bombers_.clear();
	birdys_.clear();
	slimes_.clear();
	treants_.clear();
	bosses_.clear();
	idGoblinMap_.clear();
	idSnakeMap_.clear();
	idMushroomMap_.clear();
	idMonsterMap_.clear();
	bossNpcIds_.clear();
	bossHeatProfiles_.clear();
	barrierObjects_.clear();
	strongholds_.clear();
	barriers_.clear();

	corpses_.clear();
	goblinPool_.clear();
	snakePool_.clear();
	mushroomPool_.clear();
	bomberPool_.clear();
	birdyPool_.clear();
	slimePool_.clear();
	treantPool_.clear();
	bossPool_.clear();
	respawnKind_.clear();
	monsterSpawnInfo_.clear();
	detachedNpcIds_.clear();
	pendingOrbCharges_.clear();
	orbSystem_.clear();
	std::ranges::fill(prevServerCharge_, 0.f);

	otherPlayerHpBars_.clear();
	goblinHpBars_.clear();
	snakeHpBars_.clear();
	mushroomHpBars_.clear();
	bomberHpBars_.clear();
	birdyHpBars_.clear();
	slimeHpBars_.clear();
	treantHpBars_.clear();
	bossHpBars_.clear();
	strongholdHpBars_.clear();
	npcStatusIcons_.clear();
	teammateCharge_.clear();
	teammateSelected_.clear();
	inGamePartyPlayerIds_.clear();
	inGamePartyNameById_.clear();
	inGameMonsterKillsByPlayerId_.clear();
	inGameDamageByPlayerId_.clear();
	inGamePickedItemsByPlayerId_.clear();
	inGameBossLastHitPlayerId_ = -1;
	inGamePartyNameSeq_ = 0;

	zoneStates_.clear();
	localArenaPresentationZoneId_ = -1;
	localPresentedArenaZoneIds_.clear();
	completedArenaZoneIds_.clear();
	arenaLeashActive_ = false;
	arenaWalls_.clear();
	barrierMagicCircleQuads_.clear();
	bossHpTarget_ = nullptr;
	bossHpHudActive_ = false;

	damageNumberSystem_.clear();
	minimapIcons_.clear();
	minimapGuidePoly_.clear();
	lastNpcMoveAt_.clear();
	diagElapsed_ = 0ms;
	playerDead_ = false;
	inRoom_ = false;
	moveChange_ = false;
	physicUpdateAcc_ = 0s;
	moveStateSendAcc_ = 0s;
	prevVelocity_ = {};
	currVelocity_ = {};
	knockbackTimer_ = 0.f;
	knockbackSpeed_ = 0.f;
	knockbackDir_ = {};
	postKnockbackLockTimer_ = 0.f;
	comboCount_ = 0;
	comboWindowMs_ = 0.f;
	comboSecLeft_ = 0.f;
	tornadoShotActive_ = false;
	tornadoShotElapsed_ = 0s;
	nextRenderObjId_ = 0u;
}

void Game::setupStage() {
	// 레벨 파싱 + 지형/스카이박스/방향광 (대기실 배경과 공유, 1회만 init).
	setupStageVisual();

	// UIManager 기본 리소스(setScreenSize / requestDebugResources)는
	// enterLobby에서 이미 초기화되었다.
	auto* pLabel = static_cast<UI::Label*>(
		uiManager_.root()->addChild(std::make_unique<UI::Label>())
	);
	pLabel->name    = "hpLabel";
	pLabel->anchor  = UI::Anchors::Center; // 부모의 어느 점에 붙을 지
	pLabel->pivot   = UI::Pivots::Center;	 // 내 박스의 어느 점에 못을 걸지	
	pLabel->width   = UI::DimValue::px(1000.0f);
	pLabel->height  = UI::DimValue::px(500.0f);
	pLabel->offsetX = UI::DimValue::px( -225.f );
	pLabel->offsetY = UI::DimValue::px( -250.f );
	pLabel->setTextHAlign(UI::TextHAlign::Center);
	pLabel->setTextVAlign(UI::TextVAlign::Center);
	// pLabel->setText(L"U: UI영역 표시\nEnter: 마우스 포인터 캡처\nSpace: 마우스 포인터 감추기\nWASD: 이동\nG: GBuffer 버퍼내용 순환(0=None, 1=Albedo, ..., 7=Depth)\nH: Hi-Z Cull ON/OFF\n좌클릭: 공격 ");
	pLabel->setFontSize(20.0f);
	//pLabel->setAutoSize( true );
	pLabel->setTextColor( 1.0f, 1.0f, 1.0f, 1.0f );

	playerWeaponBadge_ = static_cast<UI::Image*>(
		uiManager_.root()->addChild(std::make_unique<UI::Image>())
	);
	playerWeaponBadge_->name    = "playerWeaponBadge";
	playerWeaponBadge_->anchor  = UI::Anchors::TopLeft;
	playerWeaponBadge_->pivot   = UI::Pivots::TopLeft;
	playerWeaponBadge_->width   = UI::DimValue::px(kPlayerWeaponBadgeSize);
	playerWeaponBadge_->height  = UI::DimValue::px(kPlayerWeaponBadgeSize);
	playerWeaponBadge_->zOrder  = 3;
	playerWeaponBadge_->texture = assetManager_.playerWeaponIconBackground();
	playerWeaponBadge_->colorMul = { 2.0f, 2.0f, 2.0f, 1.0f };

	// 내부 배경 위에 무기 아이콘을 그리고, 투명 중앙을 가진 외곽 프레임을 마지막에 겹친다.
	playerWeaponIcon_ = static_cast<UI::Image*>(
		playerWeaponBadge_->addChild(std::make_unique<UI::Image>())
	);
	playerWeaponIcon_->name    = "playerWeaponIcon";
	playerWeaponIcon_->anchor  = UI::Anchors::Center;
	playerWeaponIcon_->pivot   = UI::Pivots::Center;
	playerWeaponIcon_->width   = UI::DimValue::pct(kPlayerWeaponIconScale);
	playerWeaponIcon_->height  = UI::DimValue::pct(kPlayerWeaponIconScale);
	playerWeaponIcon_->texture = assetManager_.playerWeaponIcon(PlayerWeaponType::Katana);
	playerWeaponIcon_->zOrder  = 1;

	auto* playerWeaponFrame = static_cast<UI::Image*>(
		playerWeaponBadge_->addChild(std::make_unique<UI::Image>())
	);
	playerWeaponFrame->name    = "playerWeaponFrame";
	playerWeaponFrame->anchor  = UI::Anchors::TopLeft;
	playerWeaponFrame->pivot   = UI::Pivots::TopLeft;
	playerWeaponFrame->width   = UI::DimValue::pct(100.f);
	playerWeaponFrame->height  = UI::DimValue::pct(100.f);
	playerWeaponFrame->texture = assetManager_.playerWeaponIconFrame();
	playerWeaponFrame->zOrder  = 2;

	playerHpBar_ = static_cast<UI::ProgressBar*>(
		uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
	);
	playerHpBar_->name    = "playerHpBar";
	playerHpBar_->anchor  = UI::Anchors::TopLeft;
	playerHpBar_->pivot   = UI::Pivots::TopLeft;
	playerHpBar_->width   = UI::DimValue::px(300.f);
	playerHpBar_->height  = UI::DimValue::px(kPlayerHpBarHeight);
	playerHpBar_->backgroundTex = assetManager_.playerHpFrame();
	playerHpBar_->fillTex       = assetManager_.playerHpLine();
	playerHpBar_->drawBackgroundOnTop = true;
	playerHpBar_->setProgress(1.f);

	playerHpText_ = static_cast<UI::Label*>(
		uiManager_.root()->addChild(std::make_unique<UI::Label>())
	);
	playerHpText_->name    = "playerHpText";
	playerHpText_->anchor  = UI::Anchors::TopLeft;
	playerHpText_->pivot   = UI::Pivots::TopLeft;
	playerHpText_->width   = UI::DimValue::px(300.f);
	playerHpText_->height  = UI::DimValue::px(kPlayerHpBarHeight);
	playerHpText_->zOrder  = playerHpBar_->zOrder + 1;
	playerHpText_->setTextHAlign(UI::TextHAlign::Center);
	playerHpText_->setTextVAlign(UI::TextVAlign::Center);
	playerHpText_->setFontSize(14.0f);
	playerHpText_->setTextColor(1.0f, 1.0f, 1.0f, 1.0f);
	playerHpText_->setText(L"100 / 100");

	playerNameText_ = static_cast<UI::Label*>(
		uiManager_.root()->addChild(std::make_unique<UI::Label>())
	);
	playerNameText_->name    = "playerNameText";
	playerNameText_->anchor  = UI::Anchors::TopLeft;
	playerNameText_->pivot   = UI::Pivots::TopLeft;
	playerNameText_->width   = UI::DimValue::px(240.f);
	playerNameText_->height  = UI::DimValue::px(kPlayerNameLabelHeight);
	playerNameText_->zOrder  = playerWeaponBadge_->zOrder + 1;
	playerNameText_->setTextHAlign(UI::TextHAlign::Leading);
	playerNameText_->setTextVAlign(UI::TextVAlign::Center);
	playerNameText_->setFontSize(18.0f);
	playerNameText_->setTextColor(0.0f, 0.0f, 0.0f, 1.0f);
	updatePlayerHpHudLayout();

	// Kill Count HUD (top-center). Textures are bound by pointer; they may still
	// be loading (filled in-place later) — DigitAtlas skips drawing until ready.
	killCountWidget_ = static_cast<UI::KillCountWidget*>(
		uiManager_.root()->addChild(std::make_unique<UI::KillCountWidget>())
	);
	killCountWidget_->name    = "killCountWidget";
	killCountWidget_->anchor  = UI::Anchors::BottomRight;
	killCountWidget_->pivot   = UI::Pivots::BottomRight;
	killCountWidget_->offsetX = UI::DimValue::px(-200.f);  // 2nd-from-right slot (rightmost reserved for skill UI)
	killCountWidget_->offsetY = UI::DimValue::px(-28.f);   // slightly above the bottom edge
	killCountWidget_->width   = UI::DimValue::px(160.f);
	killCountWidget_->height  = UI::DimValue::px(48.f);
	killCountWidget_->setTextures(assetManager_.digitAtlasTex(), assetManager_.killIconTex());

	damageNumberSystem_.init(assetManager_.digitAtlasTex());
	setupBossHpHud();
	finalScoreboard_.build(
		uiManager_,
		UI::FinalScoreboard::Style{
			.panelTexture = lobbyUI_.panelTexture(),
			.buttonTexture = lobbyUI_.secondaryButtonTexture(),
			.titleBannerTexture = assetManager_.tacticalZoneTitleBanner(),
			.revealEasing = UI::FinalScoreboard::Easing::EaseOut
		},
		[this]() { requestLobbyReturnFromScoreboard(); });
	tacticalZoneIntro_.init(uiManager_, assetManager_);
	tacticalDialogueOverlay_.init(uiManager_, assetManager_);
	pathGuideHUD_.init(gfx_);   // creates the distance-label text target
	pickupPromptHUD_.init(gfx_);   // "[F] 줍기" 프롬프트 + 실패 안내 텍스트 타깃
	dialogueSystem_.init(uiManager_, "../resources/UI/dialogues/dialogues.json");

	// 1000개 이상의 render object가 필요하다면 여기를 수정
	// hi-z culling 대상 개수
	gfx_.setMaxRenderObjectId(1000u);
}

void Game::setupBossHpHud() {
	bossHpRoot_ = uiManager_.root()->addChild(std::make_unique<UI::UIElement>());
	bossHpRoot_->name    = "bossHpHud";
	bossHpRoot_->anchor  = UI::Anchors::TopCenter;
	bossHpRoot_->pivot   = UI::Pivots::TopCenter;
	bossHpRoot_->offsetY = UI::DimValue::px(kBossHpHudTop);
	bossHpRoot_->width   = UI::DimValue::px(kBossHpHudWidth);
	bossHpRoot_->height  = UI::DimValue::px(kBossHpHudHeight);
	bossHpRoot_->zOrder  = 20;
	bossHpRoot_->visible = false;

	bossHpBar_ = static_cast<UI::ProgressBar*>(
		bossHpRoot_->addChild(std::make_unique<UI::ProgressBar>())
	);
	bossHpBar_->name          = "bossHpBar";
	bossHpBar_->anchor        = UI::Anchors::TopLeft;
	bossHpBar_->pivot         = UI::Pivots::TopLeft;
	bossHpBar_->offsetX       = UI::DimValue::px(kBossHpFillX);
	bossHpBar_->offsetY       = UI::DimValue::px(kBossHpFillY);
	bossHpBar_->width         = UI::DimValue::px(kBossHpFillWidth);
	bossHpBar_->height        = UI::DimValue::px(kBossHpFillHeight);
	bossHpBar_->bgColor       = { 0.f, 0.f, 0.f, 0.f };
	bossHpBar_->fillTex       = assetManager_.monsterHpBar();
	bossHpBar_->zOrder        = 0;
	bossHpBar_->setProgress(1.f);

	bossHpFrame_ = static_cast<UI::Image*>(
		bossHpRoot_->addChild(std::make_unique<UI::Image>())
	);
	bossHpFrame_->name    = "bossHpFrame";
	bossHpFrame_->anchor  = UI::Anchors::TopLeft;
	bossHpFrame_->pivot   = UI::Pivots::TopLeft;
	bossHpFrame_->width   = UI::DimValue::pct(100.f);
	bossHpFrame_->height  = UI::DimValue::pct(100.f);
	bossHpFrame_->texture = assetManager_.monsterHpFrame();
	bossHpFrame_->zOrder  = 1;

	bossHpEmblem_ = static_cast<UI::Image*>(
		bossHpRoot_->addChild(std::make_unique<UI::Image>())
	);
	bossHpEmblem_->name    = "bossHpEmblem";
	bossHpEmblem_->anchor  = UI::Anchors::TopLeft;
	bossHpEmblem_->pivot   = UI::Pivots::TopLeft;
	bossHpEmblem_->offsetX = UI::DimValue::px(kBossHpEmblemX);
	bossHpEmblem_->offsetY = UI::DimValue::px(kBossHpEmblemY);
	bossHpEmblem_->width   = UI::DimValue::px(kBossHpEmblemSize);
	bossHpEmblem_->height  = UI::DimValue::px(kBossHpEmblemSize);
	bossHpEmblem_->texture = assetManager_.erdMoreEmblem();
	bossHpEmblem_->zOrder  = -1;
}

void Game::hideCombatHudForFinalScoreboard() {
	if (playerWeaponBadge_) playerWeaponBadge_->visible = false;
	if (playerHpBar_) playerHpBar_->visible = false;
	if (playerHpText_) playerHpText_->visible = false;
	if (playerNameText_) playerNameText_->visible = false;
	if (killCountWidget_) killCountWidget_->visible = false;
	hideBossHpHud();

	for (auto& [id, entry] : otherPlayerHpBars_) {
		if (entry.hpBar) entry.hpBar->visible = false;
		if (entry.partyRoot) entry.partyRoot->visible = false;
	}
	auto hideMonsterBars = [](auto& bars) {
		for (auto& [id, entry] : bars) {
			if (entry.hpBar) entry.hpBar->visible = false;
		}
	};
	hideMonsterBars(goblinHpBars_);
	hideMonsterBars(snakeHpBars_);
	hideMonsterBars(mushroomHpBars_);
	hideMonsterBars(bomberHpBars_);
	hideMonsterBars(birdyHpBars_);
	hideMonsterBars(slimeHpBars_);
	hideMonsterBars(treantHpBars_);
	hideMonsterBars(bossHpBars_);
	hideMonsterBars(strongholdHpBars_);
	for (auto& [id, entry] : npcStatusIcons_) {
		if (entry.icon) entry.icon->visible = false;
	}
}

void Game::showFinalScoreboard() {
	if (finalScoreboard_.isVisible()) {
		return;
	}

	std::vector<UI::FinalScoreboard::Entry> rows;
	rows.reserve(inGamePartyPlayerIds_.size());
	for (const uint16 playerId : inGamePartyPlayerIds_) {
		const auto valueOrZero = [playerId](const auto& values) {
			const auto it = values.find(playerId);
			return it != values.end() ? it->second : 0;
		};
		rows.push_back(UI::FinalScoreboard::Entry{
			partyDisplayName(playerId),
			valueOrZero(inGamePickedItemsByPlayerId_),
			valueOrZero(inGameDamageByPlayerId_),
			valueOrZero(inGameMonsterKillsByPlayerId_),
			inGameBossLastHitPlayerId_ == static_cast<int32>(playerId)
				? UI::FinalScoreboard::kBossLastHitBonus
				: 0
		});
	}
	std::stable_sort(rows.begin(), rows.end(),
		[](const UI::FinalScoreboard::Entry& lhs,
			const UI::FinalScoreboard::Entry& rhs) {
			return lhs.totalScore() > rhs.totalScore();
		});

	settingsPanel_.close();
	inventoryPanel_.close();
	if (!finalScoreboard_.show(rows)) {
		return;
	}
	hideCombatHudForFinalScoreboard();
	finalScoreboardPending_ = false;
	finalBossRewardCorpseTracked_ = false;
	finalBossRewardOrbsSpawned_ = false;
	mouseDeltaX_ = 0;
	mouseDeltaY_ = 0;
	applyCursorPolicy();
}

void Game::requestLobbyReturnFromScoreboard() {
	if (!finalScoreboard_.isVisible()) {
		return;
	}

	const bool started = lobbyReturnFade_.start([this]() {
		if (INet::ClientApp::returnToLobbyServer()) {
			finalScoreboard_.hide();
			lobbyLeaveRoom();
			enterLobby();
		}
		else {
			gSharedLog << "[Result] 인증된 LobbyServer 연결이 없어 로비로 복귀하지 못했습니다.\n";
		}
	});
	if (started) {
		uiManager_.resetInteractionState();
	}
}

void Game::showBossHpHud() {
	bossHpHudActive_ = true;
	bossHpTarget_ = nullptr;
	if (bossHpBar_) bossHpBar_->setProgress(1.f);
	if (bossHpRoot_) bossHpRoot_->visible = true;
}

void Game::hideBossHpHud() {
	bossHpHudActive_ = false;
	bossHpTarget_ = nullptr;
	if (bossHpRoot_) bossHpRoot_->visible = false;
}

void Game::updateBossHpHud() {
	if (!bossHpHudActive_) {
		if (bossHpRoot_) bossHpRoot_->visible = false;
		return;
	}

	if (!bossHpTarget_) {
		auto it = std::ranges::find_if(bosses_, [](const std::shared_ptr<Boss>& boss) {
			return boss && !boss->isDead() && boss->hp() > 0;
		});
		if (it != bosses_.end()) {
			bossHpTarget_ = it->get();
		}
	}

	if (bossHpTarget_) {
		if (bossHpTarget_->isDead() || bossHpTarget_->hp() <= 0) {
			hideBossHpHud();
			return;
		}
		if (bossHpBar_) {
			bossHpBar_->setProgress(
				static_cast<float>(bossHpTarget_->hp()) /
				static_cast<float>(std::max(1, bossHpTarget_->maxHp()))
			);
		}
	}

	if (bossHpRoot_) bossHpRoot_->visible = true;
}

void Game::updatePlayerHpHudLayout() {
	if (!playerWeaponBadge_ || !playerHpBar_) return;

	const float badgeY = kPlayerHpUiY - (kPlayerWeaponBadgeSize - kPlayerHpBarHeight) * 0.5f;
	const float barX = kPlayerHpUiX + kPlayerWeaponBadgeSize - kPlayerWeaponBadgeBarOverlap;

	playerWeaponBadge_->offsetX = UI::DimValue::px(uiManager_.screenLeftInsetToLayoutX(kPlayerHpUiX));
	playerWeaponBadge_->offsetY = UI::DimValue::px(uiManager_.screenTopInsetToLayoutY(badgeY));
	playerHpBar_->offsetX = UI::DimValue::px(uiManager_.screenLeftInsetToLayoutX(barX));
	playerHpBar_->offsetY = UI::DimValue::px(uiManager_.screenTopInsetToLayoutY(kPlayerHpUiY));

	if (playerHpText_) {
		playerHpText_->offsetX = playerHpBar_->offsetX;
		playerHpText_->offsetY = playerHpBar_->offsetY;
	}
	if (playerNameText_) {
		const float nameX = kPlayerHpUiX + kPlayerWeaponBadgeSize + kPlayerNameBadgeGap;
		const float nameY = kPlayerHpUiY - kPlayerNameLabelHeight;
		playerNameText_->offsetX = UI::DimValue::px(
			uiManager_.screenLeftInsetToLayoutX(nameX)
		);
		playerNameText_->offsetY = UI::DimValue::px(
			uiManager_.screenTopInsetToLayoutY(nameY)
		);
	}
	updatePartyHpHudLayout();
}

void Game::registerInGamePartyPlayer(uint16 playerId, const wchar_t* nickname) {
	if (std::ranges::find(inGamePartyPlayerIds_, playerId) == inGamePartyPlayerIds_.end()) {
		inGamePartyPlayerIds_.push_back(playerId);
		inGameMonsterKillsByPlayerId_.try_emplace(playerId, 0);
		inGameDamageByPlayerId_.try_emplace(playerId, 0);
		inGamePickedItemsByPlayerId_.try_emplace(playerId, 0);

		// The account nickname is authoritative when the server sent one. The roster
		// (S_Enter names section / S_Enter_Other) is seeded before the object list is
		// walked, and this function is a no-op for ids already present, so the later
		// nickname-less createOtherPlayer(ObjectInfo) path cannot clobber it.
		const size_t nickLen = nickname ? wcsnlen(nickname, kNicknameMax) : 0;
		if (nickLen > 0) {
			inGamePartyNameById_[playerId] = std::wstring(nickname, nickLen);
			return;
		}

		// Fallback for sessions with no nickname. Freeze the name at registration:
		// index-based names would shift on every leave, and my own label (refreshed
		// per frame) would diverge from the stale labels other clients keep showing
		// for me. Join order is identical on every client (server fills the S_Enter
		// objList from sessions_ in enter order, later joins append via
		// S_Enter_Other), so the numbering stays cross-client consistent.
		inGamePartyNameById_[playerId] = L"player" + std::to_wstring(++inGamePartyNameSeq_);
	}
}

void Game::unregisterInGamePartyPlayer(uint16 playerId) {
	std::erase(inGamePartyPlayerIds_, playerId);
	inGamePartyNameById_.erase(playerId);
}

std::wstring Game::partyDisplayName(uint16 playerId) const {
	if (auto it = inGamePartyNameById_.find(playerId); it != inGamePartyNameById_.end()) {
		return it->second;
	}

	return L"player";
}

void Game::syncLobbyCharacterWeapons() {
	if (lobbyChars_.empty()) return;

	for (std::size_t i = 0u; i < lobbyChars_.size(); ++i) {
		if (!lobbyChars_[i]) continue;
		if (i < lobbyPlayers_.size()) {
			equipPlayerWeapon(*lobbyChars_[i], assetManager_, lobbyPlayers_[i].weaponType);
		}
		else {
			lobbyChars_[i]->disequip(Bone::SocketType::RightHand);
			lobbyChars_[i]->disequip(Bone::SocketType::LeftHand);
		}
	}
}

void Game::createOtherPlayerHud(uint16 playerId, Player* player, PlayerWeaponType weaponType) {
	auto* root = uiManager_.root();
	if (auto it = otherPlayerHpBars_.find(playerId); it != otherPlayerHpBars_.end()) {
		// Live widget destruction frees GPU resources (the party Label's
		// TextImage); drain so in-flight frames can't reference freed memory.
		gfx_.drainGpu();
		if (it->second.hpBar) {
			root->removeChild(it->second.hpBar);
		}
		if (it->second.partyRoot) {
			root->removeChild(it->second.partyRoot);
		}
		otherPlayerHpBars_.erase(it);
	}

	auto* worldBar = static_cast<UI::ProgressBar*>(
		root->addChild(std::make_unique<UI::ProgressBar>())
	);
	worldBar->anchor    = UI::Anchors::TopLeft;
	worldBar->pivot     = UI::Pivots::TopLeft;
	worldBar->width     = UI::DimValue::px(80.f);
	worldBar->height    = UI::DimValue::px(8.f);
	worldBar->fillColor = { 0.2f, 0.6f, 1.0f, 1.f };
	worldBar->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
	worldBar->visible   = false;

	auto* partyRoot = root->addChild(std::make_unique<UI::UIElement>());
	partyRoot->name    = "partyPlayerHud";
	partyRoot->anchor  = UI::Anchors::TopLeft;
	partyRoot->pivot   = UI::Pivots::TopLeft;
	partyRoot->width   = UI::DimValue::px(kPartyWeaponBadgeSize + kPartyHpBarWidth);
	partyRoot->height  = UI::DimValue::px(kPartyHpRowHeight);
	partyRoot->zOrder  = 2;

	auto* partyWeaponBadge = static_cast<UI::Image*>(
		partyRoot->addChild(std::make_unique<UI::Image>())
	);
	partyWeaponBadge->name    = "partyWeaponBadge";
	partyWeaponBadge->anchor  = UI::Anchors::TopLeft;
	partyWeaponBadge->pivot   = UI::Pivots::TopLeft;
	partyWeaponBadge->width   = UI::DimValue::px(kPartyWeaponBadgeSize);
	partyWeaponBadge->height  = UI::DimValue::px(kPartyWeaponBadgeSize);
	partyWeaponBadge->texture = assetManager_.playerWeaponIconBackground();
	partyWeaponBadge->colorMul = { 2.0f, 2.0f, 2.0f, 1.0f };
	partyWeaponBadge->zOrder  = 3;

	auto* partyWeaponIcon = static_cast<UI::Image*>(
		partyWeaponBadge->addChild(std::make_unique<UI::Image>())
	);
	partyWeaponIcon->name    = "partyWeaponIcon";
	partyWeaponIcon->anchor  = UI::Anchors::Center;
	partyWeaponIcon->pivot   = UI::Pivots::Center;
	partyWeaponIcon->width   = UI::DimValue::pct(kPartyWeaponIconScale);
	partyWeaponIcon->height  = UI::DimValue::pct(kPartyWeaponIconScale);
	partyWeaponIcon->texture = assetManager_.playerWeaponIcon(weaponType);
	partyWeaponIcon->zOrder  = 1;

	auto* partyWeaponFrame = static_cast<UI::Image*>(
		partyWeaponBadge->addChild(std::make_unique<UI::Image>())
	);
	partyWeaponFrame->name    = "partyWeaponFrame";
	partyWeaponFrame->anchor  = UI::Anchors::TopLeft;
	partyWeaponFrame->pivot   = UI::Pivots::TopLeft;
	partyWeaponFrame->width   = UI::DimValue::pct(100.f);
	partyWeaponFrame->height  = UI::DimValue::pct(100.f);
	partyWeaponFrame->texture = assetManager_.playerWeaponIconFrame();
	partyWeaponFrame->zOrder  = 2;

	auto* partyName = static_cast<UI::Label*>(
		partyRoot->addChild(std::make_unique<UI::Label>())
	);
	partyName->name    = "partyName";
	partyName->anchor  = UI::Anchors::TopLeft;
	partyName->pivot   = UI::Pivots::TopLeft;
	partyName->offsetX = UI::DimValue::px(kPartyWeaponBadgeSize + kPartyNameBadgeGap);
	partyName->offsetY = UI::DimValue::px(0.f);
	partyName->width   = UI::DimValue::px(
		kPartyHpBarWidth - kPartyWeaponBadgeBarOverlap - kPartyNameBadgeGap
	);
	partyName->height  = UI::DimValue::px(kPartyHpNameHeight);
	partyName->zOrder  = partyWeaponBadge->zOrder + 1;
	partyName->setTextHAlign(UI::TextHAlign::Leading);
	partyName->setTextVAlign(UI::TextVAlign::Center);
	partyName->setFontSize(16.0f);
	partyName->setTextColor(0.0f, 0.0f, 0.0f, 1.0f);
	partyName->setText(partyDisplayName(playerId));

	auto* partyBar = static_cast<UI::ProgressBar*>(
		partyRoot->addChild(std::make_unique<UI::ProgressBar>())
	);
	partyBar->name      = "partyHpBar";
	partyBar->anchor    = UI::Anchors::TopLeft;
	partyBar->pivot     = UI::Pivots::TopLeft;
	partyBar->offsetX   = UI::DimValue::px(kPartyWeaponBadgeSize - kPartyWeaponBadgeBarOverlap);
	partyBar->offsetY   = UI::DimValue::px(kPartyHpNameHeight);
	partyBar->width     = UI::DimValue::px(kPartyHpBarWidth);
	partyBar->height    = UI::DimValue::px(kPartyHpBarHeight);
	partyBar->backgroundTex = assetManager_.playerHpFrame();
	partyBar->fillTex       = assetManager_.playerHpLine();
	partyBar->drawBackgroundOnTop = true;
	partyBar->zOrder    = 1;
	partyBar->setProgress(1.f);

	otherPlayerHpBars_[playerId] = {
		player,
		worldBar,
		weaponType,
		partyRoot,
		partyWeaponBadge,
		partyWeaponIcon,
		partyName,
		partyBar
	};
	updatePartyHpHudLayout();
}

void Game::updatePartyHpHudLayout() {
	int visibleRow = 0;
	const uint16 localPlayerId = player_ ? static_cast<uint16>(player_->getId()) : 0;
	for (uint16 playerId : inGamePartyPlayerIds_) {
		if (playerId == localPlayerId) {
			continue;
		}

		auto it = otherPlayerHpBars_.find(playerId);
		if (it == otherPlayerHpBars_.end()) {
			continue;
		}

		auto& entry = it->second;
		if (!entry.partyRoot) {
			continue;
		}

		const float rowY = kPlayerHpUiY + kPartyHpStartYOffset + kPartyHpRowHeight * visibleRow;
		entry.partyRoot->offsetX = UI::DimValue::px(uiManager_.screenLeftInsetToLayoutX(kPlayerHpUiX));
		entry.partyRoot->offsetY = UI::DimValue::px(uiManager_.screenTopInsetToLayoutY(rowY));
		entry.partyRoot->visible = true;

		if (entry.partyNameLabel) {
			entry.partyNameLabel->setText(partyDisplayName(playerId));
		}

		++visibleRow;
	}

	for (auto& [id, entry] : otherPlayerHpBars_) {
		const bool listed = std::ranges::find(inGamePartyPlayerIds_, static_cast<uint16>(id)) != inGamePartyPlayerIds_.end()
			&& id != localPlayerId;
		if (!listed && entry.partyRoot) {
			entry.partyRoot->visible = false;
		}
	}
}

void Game::updatePartyHpHudValues() {
	for (auto& [id, entry] : otherPlayerHpBars_) {
		if (!entry.partyRoot || !entry.partyHpBar || !entry.player) {
			continue;
		}

		const int maxHp = std::max(1, entry.player->maxHp());
		const int hp = std::max(0, entry.player->hp());
		entry.partyHpBar->setProgress(static_cast<float>(hp) / static_cast<float>(maxHp));
		if (entry.partyWeaponIcon) {
			entry.partyWeaponIcon->texture = assetManager_.playerWeaponIcon(entry.weaponType);
		}
	}
}

void Game::importNode(std::ifstream& ifs) {
	readHeadTag(ifs, "Node");
	const auto type = readText(ifs, "Type");
	const auto name = readText(ifs, "Name");

	gSharedLog << "[Level Load] 레벨 노드 " << name << " 로드 완료\n";

	readHeadTag(ifs, "LocalTRS");
	const auto localT = readVec3(ifs, "Position");
	const auto localR = readVec4(ifs, "Rotation");
	const auto localS = readVec3(ifs, "Scale");
	readTailTag(ifs, "LocalTRS");

	readHeadTag(ifs, "WorldTRS");
	const auto worldT = readVec3(ifs, "Position");
	const auto worldR = readVec4(ifs, "Rotation");
	const auto worldS = readVec3(ifs, "Scale");
	readTailTag(ifs, "WorldTRS");

	Object object{};
	object.setPos(DirectX::XMLoadFloat3(&worldT));
	object.setOrient(DirectX::XMLoadFloat4(&worldR));
	object.setScale(DirectX::XMLoadFloat3(&worldS));

	if (type == "Terrain") {
		// Terrain is now streamed by TerrainChunkManager (chunks_index.bin).
		// Consume the legacy ManifestPath field to keep the scene stream aligned, then ignore.
		readText(ifs, "ManifestPath");
	}
	else if (type == "PlayerStart") {
		// 플레이어 스폰 위치(대기실 3D 카메라 포커스/캐릭터 배치에 사용).
		stageSpawnPositions_.push_back(mu::Vec3(worldT.x, worldT.y, worldT.z));
	}
	else {
		// no-op
	}

	const auto childCnt = readInteger(ifs, "ChildCnt");
	readHeadTag(ifs, "Children");
	for (int i = 0; i < childCnt; ++i) {
		importNode(ifs);
	}
	readTailTag(ifs, "Children");

	readTailTag(ifs, "Node");
}

void Game::setParticle()
{
	// ── Flame ────────────────────────────────────────────────────────────────
	{
		ps::ParticleSystemConfig cfg;
		cfg.main.lifetimeMin        = 0.5f;
		cfg.main.lifetimeMax        = 1.0f;
		cfg.main.speedMin           = 0.f;
		cfg.main.speedMax           = 0.3f;
		cfg.main.startColor         = { 1.f, 0.4f, 0.f, 1.f };
		cfg.main.startSizeMin       = 0.8f;
		cfg.main.startSizeMax       = 1.0f;
		cfg.main.startRotationMin   = 0.f;
		cfg.main.startRotationMax   = mu::pi * 2.f;
		cfg.main.gravityModifierMin = -0.3f;
		cfg.main.gravityModifierMax = 0.f;
		cfg.main.gravity            = { 0.f, -9.8f, 0.f };
		cfg.main.duration           = 0.f;

		cfg.emission.emitRate = 15.f;

		cfg.shape.type       = ps::ShapeModule::Type::Edge;
		cfg.shape.position   = { -6.f, 58.5f, -5.f };
		cfg.shape.direction  = { 0.f, 1.f, 0.f };
		cfg.shape.edgeLength = 1.5f;
		cfg.shape.edgeDir    = { 1.f, 0.f, 0.f };

		cfg.colorOverLifetime.enabled  = true;
		cfg.colorOverLifetime.gradient = ColorGradient::constant({ 1.f, 1.f, 1.f, 1.f });

		cfg.sizeOverLifetime.enabled   = true;
		cfg.sizeOverLifetime.sizeBegin = 1.0f;
		cfg.sizeOverLifetime.sizeEnd   = 1.0f;

		cfg.renderer.mode               = ps::RendererModule::Mode::Billboard;
		cfg.renderer.renderOrder        = 0;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.flameTex(), .blend = ps::BlendMode::Additive };

		cfg.textureSheetAnimation.enabled   = true;
		cfg.textureSheetAnimation.tilesX    = 3;
		cfg.textureSheetAnimation.tilesY    = 3;
		cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
		cfg.textureSheetAnimation.cycles    = 1.f;

		flameParticleSystem_.startContinuous(cfg);
	}

	// ── Smoke ────────────────────────────────────────────────────────────────
	{
		ps::ParticleSystemConfig cfg;
		cfg.main.lifetimeMin        = 0.5f;
		cfg.main.lifetimeMax        = 1.0f;
		cfg.main.speedMin           = 0.5f;
		cfg.main.speedMax           = 2.f;
		cfg.main.startColor         = { 0.5f, 0.5f, 0.5f, 1.f };
		cfg.main.startSizeMin       = 1.f;
		cfg.main.startSizeMax       = 1.f;
		cfg.main.startRotationMin   = 0.f;
		cfg.main.startRotationMax   = mu::pi * 2.f;
		cfg.main.gravityModifierMin = -0.5f;
		cfg.main.gravityModifierMax = -0.2f;
		cfg.main.gravity            = { 0.f, -1.f, 0.f };
		cfg.main.duration           = 0.f;

		cfg.emission.emitRate = 10.f;

		cfg.shape.type       = ps::ShapeModule::Type::Edge;
		cfg.shape.position   = { -6.f, 58.5f, -5.f };
		cfg.shape.direction  = { 0.f, 1.f, 0.f };
		cfg.shape.edgeLength = 1.5f;
		cfg.shape.edgeDir    = { 1.f, 0.f, 0.f };

		cfg.colorOverLifetime.enabled  = true;
		cfg.colorOverLifetime.gradient = ColorGradient{
			.keys = {
				{ 0.0f, { 1.f,  1.f,  1.f,  0.f } },
				{ 0.5f, { 0.5f, 0.5f, 0.5f, 1.f } },
				{ 1.0f, { 0.f,  0.f,  0.f,  0.f } },
			}
		};

		cfg.sizeOverLifetime.enabled   = true;
		cfg.sizeOverLifetime.sizeBegin = 1.f;
		cfg.sizeOverLifetime.sizeEnd   = 1.f;

		cfg.renderer.mode               = ps::RendererModule::Mode::Billboard;
		cfg.renderer.renderOrder        = 1;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.smokeTex() };

		cfg.textureSheetAnimation.enabled   = true;
		cfg.textureSheetAnimation.tilesX    = 3;
		cfg.textureSheetAnimation.tilesY    = 3;
		cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
		cfg.textureSheetAnimation.cycles    = 1.f;

		smokeParticleSystem_.startContinuous(cfg);
	}

	auto loadUnityParticleConfig = [this](const std::filesystem::path& jsonPath,
	                                      std::string_view relativePath) {
		// 백그라운드에서 미리 파싱해둔 캐시를 우선 사용한다 (디스크 재파싱 회피).
		const std::string key = jsonPath.filename().string() + "|" + std::string(relativePath);
		if (auto it = particleConfigCache_.find(key); it != particleConfigCache_.end()) {
			return it->second;
		}
		// 캐시 미스 시 폴백: 직접 파싱.
		ps::ParticleSystemConfig cfg;
		loadParticleSystemConfigFromUnityJson(jsonPath, relativePath, cfg);
		return cfg;
	};

	// ── Blood hit effect (칼/창/완드 공통 피격 혈흔, vfxId 0) ──────────────────
	// Alpha Blend(MatUnlit) + Plane 곡면 메시 + 3x3 스프라이트 시트 플립북.
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/blood_hit.json",
			"Particle System (4)"
		);
		cfg.renderer.pMesh = assetManager_.meshBloodPlane();
		cfg.renderer.pSubMesh = assetManager_.meshBloodPlane()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshBloodPlane()->subMeshes[0];
		cfg.renderer.mat = ps::MatUnlit{
			.mainTex = assetManager_.bloodTex(),
			.blend   = ps::BlendMode::Alpha,
		};
		// 피격 위치에 고정되도록 World 시뮬레이션으로 강제(Unity 원본은 Local).
		// 공유 인스턴스를 연속 타격에 재사용해도 기존 혈흔이 끌려오지 않게 한다.
		cfg.main.simulationSpace = ps::MainModule::SimulationSpace::World;
		cfg.renderer.renderOrder = 2;
		bloodEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── Sword Slash 1 effect ─────────────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Sword Slash 1_ParticleSystems.json",
			"Sword Slash 1"
		);
		cfg.renderer.pMesh = assetManager_.meshSlash3();
		cfg.renderer.pSubMesh = assetManager_.meshSlash3()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshSlash3()->subMeshes[0];
		cfg.renderer.mat = assetManager_.swordSlashMaterial();
		cfg.renderer.renderOrder = 2;
		swordSlash1Effect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Sword Slash 1_ParticleSystems.json",
			"Sword Slash 1/Smoke"
		);
		cfg.main.looping = false;
		cfg.renderer.mat = assetManager_.smokeBlendCGMaterial();
		swordSlash1Effect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}

	// ── Sword Slash Combo ─────────────────────────────────────────────────────
	{
		auto baseCfg = loadUnityParticleConfig(
			"../resources/effects/Sword Slash 1_ParticleSystems.json",
			"Sword Slash 1"
		);
		baseCfg.renderer.pMesh = assetManager_.meshSlash3();
		baseCfg.renderer.pSubMesh = assetManager_.meshSlash3()->subMeshes.empty()
		                           ? nullptr
		                           : &assetManager_.meshSlash3()->subMeshes[0];
		baseCfg.renderer.mat = assetManager_.swordSlashMaterial();
		baseCfg.renderer.renderOrder = 2;
		baseCfg.main.looping = false;
		baseCfg.main.startRotation3DEnabled = true;
		baseCfg.emission.enabled = true;
		baseCfg.emission.emitRate = 0.f;
		baseCfg.emission.rateOverDistance = 0.f;

		struct ComboLayer { float time; float rotZ; };
		const ComboLayer layers[] = {
			{ 0.10f, -0.62831855f },
			{ 0.45f,  0.31415927f },
			{ 0.60f, -0.20943952f },
			{ 1.15f,  0.59341192f },
			{ 1.15f,  0.f        },
		};
		for (const auto& layer : layers) {
			auto cfg = baseCfg;
			cfg.main.startRotation3DMin = { 0.f, -2.44346094f, layer.rotZ };
			cfg.main.startRotation3DMax = cfg.main.startRotation3DMin;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = layer.time,
					.countMin = 1,
					.countMax = 1,
					.cycleCount = 1,
					.repeatInterval = 0.01f,
					.probability = 1.f,
				}
			};
			swordSlashComboEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
		}
	}

	// ── Sword Slash 7 ─────────────────────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Slash_ParticleSystems.json",
			"Slash"
		);
		cfg.renderer.pMesh = assetManager_.meshSlash3();
		cfg.renderer.pSubMesh = assetManager_.meshSlash3()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshSlash3()->subMeshes[0];
		cfg.renderer.mat = assetManager_.swordSlash2Material();
		cfg.renderer.renderOrder = 2;
		swordSlash7Effect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Slashes_ParticleSystems.json",
			"Slashes"
		);
		cfg.renderer.pMesh = assetManager_.meshSlash3();
		cfg.renderer.pSubMesh = assetManager_.meshSlash3()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshSlash3()->subMeshes[0];
		cfg.renderer.mat = assetManager_.swordSlash2Material();
		cfg.renderer.renderOrder = 2;
		swordSlash7Effect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── Spikes Attack effect ───────────────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Spikes attack_ParticleSystems.json",
			"Spikes attack/Spikes"
		);
		cfg.renderer.pMesh = assetManager_.meshIceSpikes2();
		cfg.renderer.pSubMesh = assetManager_.meshIceSpikes2()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshIceSpikes2()->subMeshes[0];
		cfg.renderer.mat = assetManager_.spikesMaterial();
		cfg.colorOverLifetime.enabled = true;
		cfg.colorOverLifetime.gradient = ColorGradient{
			.keys = {
				{ 0.0f,  { 1.f, 1.f, 1.f, 1.f } },
				{ 0.78f, { 1.f, 1.f, 1.f, 1.f } },
				{ 1.0f,  { 1.f, 1.f, 1.f, 0.f } },
			}
		};
		spikesAttackEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── Piercing effect ────────────────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/PS_VFX_Piercing_ParticleSystems.json",
			"PS_VFX_Piercing"
		);
		cfg.renderer.pMesh = assetManager_.meshVfxProjectile02();
		cfg.renderer.pSubMesh = assetManager_.meshVfxProjectile02()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshVfxProjectile02()->subMeshes[0];
		cfg.renderer.mat = assetManager_.piercingMaterial();
		// Unity 로컬 +X 축으로 발사되는 VFX. 엔진 forward(+Z) 정렬을 위해 -90° yaw 보정.
		const mu::Mat4x4 piercingForwardFix = mu::rotateYH(mu::Degree(-90.f));
		cfg.shape.orientation    = piercingForwardFix;
		cfg.main.startRotation3D = piercingForwardFix;
		piercingEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── PiercingMulti effect: player forward에 수직인 전방 벽에 비처럼 분포해 2발씩 10웨이브 stab ──
	{
		const mu::Mat4x4 piercingForwardFix = mu::rotateYH(mu::Degree(-90.f));
		const mu::Vec2 offsets[kPiercingMultiWaveCount][kPiercingMultiBurstCount] = {
			{ { -2.24f,  1.04f }, {  0.88f, -0.62f } },
			{ { -0.62f, -1.08f }, {  2.18f,  0.38f } },
			{ { -1.78f,  0.24f }, {  1.46f,  1.12f } },
			{ { -2.38f, -0.72f }, {  0.34f,  0.82f } },
			{ { -0.96f,  1.18f }, {  2.34f, -0.24f } },
			{ { -1.42f, -0.38f }, {  0.72f, -1.16f } },
			{ { -2.08f,  0.62f }, {  1.88f,  0.92f } },
			{ { -0.28f, -0.84f }, {  2.48f, -1.02f } },
			{ { -2.46f,  0.06f }, {  0.18f,  1.24f } },
			{ { -1.12f, -1.22f }, {  1.24f,  0.14f } },
		};

		for (int wave = 0; wave < kPiercingMultiWaveCount; ++wave) {
			for (int lane = 0; lane < kPiercingMultiBurstCount; ++lane) {
				auto cfg = loadUnityParticleConfig(
					"../resources/effects/PS_VFX_Piercing_ParticleSystems.json",
					"PS_VFX_Piercing"
				);
				cfg.renderer.pMesh = assetManager_.meshVfxProjectile02();
				cfg.renderer.pSubMesh = assetManager_.meshVfxProjectile02()->subMeshes.empty()
				                       ? nullptr
				                       : &assetManager_.meshVfxProjectile02()->subMeshes[0];
				cfg.renderer.mat = assetManager_.piercingMaterial();

				cfg.main.startRotation3D = piercingForwardFix;
				cfg.main.speedMin        = 0.f;
				cfg.main.speedMax        = 0.f;
				cfg.main.lifetimeMin     = kPiercingMultiLifetime;
				cfg.main.lifetimeMax     = kPiercingMultiLifetime;
				cfg.main.looping         = false;
				cfg.main.duration        = kPiercingMultiInterval * static_cast<float>(kPiercingMultiWaveCount - 1) + 0.1f;
				cfg.main.maxParticles    = 4;

				cfg.velocityOverLifetime.enabled      = true;
				cfg.velocityOverLifetime.useCurves    = false;
				cfg.velocityOverLifetime.inWorldSpace = false;
				cfg.velocityOverLifetime.linear       = { 0.f, 0.f, 25.f };
				cfg.velocityOverLifetime.orbital      = { 0.f, 0.f, 0.f };
				cfg.velocityOverLifetime.radial       = 0.f;
				cfg.velocityOverLifetime.drag         = 0.f;

				cfg.shape.type        = ps::ShapeModule::Type::Box;
				cfg.shape.position    = {
					std::clamp(offsets[wave][lane].x(), -kPiercingMultiHalfWidth, kPiercingMultiHalfWidth),
					std::clamp(offsets[wave][lane].y(), -kPiercingMultiHalfHeight, kPiercingMultiHalfHeight),
					0.f
				};
				cfg.shape.rotation    = { 0.f, 0.f, 0.f };
				cfg.shape.orientation = mu::Mat4x4{};
				cfg.shape.direction   = { 0.f, 0.f, 1.f };
				cfg.shape.boxSize     = { 0.56f, 0.42f, 0.f };

				cfg.emission.enabled  = true;
				cfg.emission.emitRate = 0.f;
				cfg.emission.bursts   = {
					ps::EmissionModule::Burst{
						.time = kPiercingMultiInterval * static_cast<float>(wave),
						.countMin = 1, .countMax = 1, .cycleCount = 1
					}
				};

				piercingMultiEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous, 4);
			}
		}
	}

	// ── PiercingSlash effect (Vefects SH_VFX_Vefects_Slash_BIRP_New) ──────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/PS_VFX_Slash_ParticleSystems.json",
			"PS_VFX_Slash"
		);
		cfg.renderer.pMesh = assetManager_.meshVfxSlash01HD();
		cfg.renderer.pSubMesh = assetManager_.meshVfxSlash01HD()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshVfxSlash01HD()->subMeshes[0];
		cfg.renderer.mat = assetManager_.piercingSlashMaterial();
		cfg.main.simulationSpeed = 2.0f;
		piercingSlashEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── PiercingCircleSlash effect (same shader as PiercingSlash, Circle variant)
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/PS_VFX_PiercingCircleSlash_ParticleSystems.json",
			"PS_VFX_Slash"
		);
		cfg.renderer.pMesh = assetManager_.meshVfxSlash01HD();
		cfg.renderer.pSubMesh = assetManager_.meshVfxSlash01HD()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshVfxSlash01HD()->subMeshes[0];
		cfg.renderer.mat = assetManager_.piercingCircleSlashMaterial();
		cfg.main.startSizeMin *= 2.0f;
		cfg.main.startSizeMax *= 2.0f;
		cfg.rotationOverLifetime.enabled = true;
		cfg.rotationOverLifetime.useCurves = false;
		cfg.rotationOverLifetime.separateAxes = false;
		cfg.rotationOverLifetime.angularVelocityMin = 6.2831853f * 2.0f;  // 2 rev/sec (local Z)
		cfg.rotationOverLifetime.angularVelocityMax = 6.2831853f * 2.0f;
		piercingCircleSlashEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── Slash Wave effect ──────────────────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Slash wave_ParticleSystems.json",
			"Slash wave"
		);
		cfg.main.looping = false;
		cfg.renderer.pMesh = assetManager_.meshHalfTrail();
		cfg.renderer.pSubMesh = assetManager_.meshHalfTrail()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshHalfTrail()->subMeshes[0];
		cfg.renderer.mat = assetManager_.twoSidesMaterial();
		slashWaveEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Slash wave_ParticleSystems.json",
			"Slash wave/SlashPath"
		);
		cfg.main.looping = false;
		cfg.shape.direction = { 0.f, 0.f, 1.f };
		cfg.renderer.pMesh = assetManager_.meshSlashWave();
		cfg.renderer.pSubMesh = assetManager_.meshSlashWave()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshSlashWave()->subMeshes[0];
		cfg.renderer.mat = assetManager_.slashPathMaterial();
		slashWaveEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}

	// ── Crystals Front Attack effect ──────────────────────────────────────────
	{
		const std::filesystem::path crystalsJson =
			"../resources/effects/Crystals front attack_ParticleSystems.json";

		// parent: 7-burst trigger particles (invisible, just fires sub-emitter birth events)
		{
			auto cfg = loadUnityParticleConfig(crystalsJson, "Crystals front attack");
			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.mat  = ps::MatUnlit{ .mainTex = nullptr, .blend = ps::BlendMode::Additive };
			cfg.main.looping  = false;
			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = { {
				.event           = ps::SubEmittersModule::Event::Birth,
				.emitProbability = 1.f,
				.inheritSize     = true,
			} };
			crystalsFrontAttackEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);  // idx 0
			// Gameplay config for the hitbox-bound system 0 comes from the
			// skill lua (addVFX systems table) via bindVfxGameplayConfigs().
		}

		// child: crystal pillars (StretchedBillboard in Unity)
		{
			auto cfg = loadUnityParticleConfig(crystalsJson, "Crystals front attack/Crystals");
			cfg.renderer.mat  = ps::MatUnlit{
				.mainTex = assetManager_.crystalFree1Tex(),
				.blend = ps::BlendMode::Alpha,
				.color = { 1.15271747f, 1.1794312f, 1.41421354f, 1.f }
			};
			crystalsFrontAttackEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);  // idx 1
		}

		// parent[0] subEmitters[0] (Crystals entry) -> child[1]
		crystalsFrontAttackEffect_.bindSubEmitter(0, 0, 1, true);
	}

	// ── AoE Slash Green effect ────────────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/AoE slash green_ParticleSystems.json",
			"AoE slash green"
		);
		cfg.main.looping      = false;
		cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
		cfg.renderer.mode     = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat      = ps::MatUnlit{ .mainTex = assetManager_.circle2Tex(), .blend = ps::BlendMode::Additive };
		aoESlashGreenEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/AoE slash green_ParticleSystems.json",
			"AoE slash green/Slash"
		);
		cfg.main.looping        = false;
		cfg.main.startColor     = { 1.f, 1.f, 1.f, 0.5f };
		cfg.renderer.mode       = ps::RendererModule::Mode::StretchedBillboard;
		cfg.renderer.lengthScale  = 5.f;
		cfg.renderer.mat        = ps::MatUnlit{ .mainTex = assetManager_.slashTex(), .blend = ps::BlendMode::Additive };
		aoESlashGreenEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/AoE slash green_ParticleSystems.json",
			"AoE slash green/RotatePart"
		);
		cfg.main.looping    = false;
		cfg.renderer.mode   = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat    = ps::MatUnlit{ .mainTex = assetManager_.slashTex(), .blend = ps::BlendMode::Additive };
		aoESlashGreenEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}

	// ── foot bone indices (for dust VFX) ──────────────────────────────────────
	const auto& playerSkeleton = player_->model()->skeleton;
	if (playerSkeleton.bones) {
		for (const auto& bone : *playerSkeleton.bones) {
			if (bone.name == "foot_l")       footBoneIdxLeft_  = bone.boneIdx;
			else if (bone.name == "foot_r")  footBoneIdxRight_ = bone.boneIdx;
		}
	}
	if (footBoneIdxLeft_ < 0 || footBoneIdxRight_ < 0) {
		gSharedLog << "[Dust VFX] Warning: foot bones not found.\n";
	}

	// ── Dust (foot impact VFX) ────────────────────────────────────────────────
	{
		ps::ParticleSystemConfig cfg;
		cfg.main.lifetimeMin        = 0.3f;
		cfg.main.lifetimeMax        = 0.6f;
		cfg.main.speedMin           = 0.3f;
		cfg.main.speedMax           = 0.8f;
		cfg.main.startColor         = { 0.55f, 0.4f, 0.25f, 0.8f };
		cfg.main.startSizeMin       = 0.8f;
		cfg.main.startSizeMax       = 1.2f;
		cfg.main.startRotationMin   = 0.f;
		cfg.main.startRotationMax   = mu::pi * 2.f;
		cfg.main.gravityModifierMin = 0.f;
		cfg.main.gravityModifierMax = 0.05f;
		cfg.main.gravity            = { 0.f, -9.8f, 0.f };

		cfg.shape.type      = ps::ShapeModule::Type::Cone;
		cfg.shape.coneAngle = 1.2f;
		cfg.shape.direction = { 0.f, 1.f, 0.f };

		cfg.colorOverLifetime.enabled  = true;
		cfg.colorOverLifetime.gradient = ColorGradient{
			.keys = {
				{ 0.0f, { 1.f, 1.f, 1.f, 0.f } },
				{ 0.2f, { 1.f, 1.f, 1.f, 1.f } },
				{ 1.0f, { 0.7f, 0.7f, 0.7f, 0.f } },
			}
		};

		cfg.sizeOverLifetime.enabled   = true;
		cfg.sizeOverLifetime.sizeBegin = 0.3f;
		cfg.sizeOverLifetime.sizeEnd   = 0.8f;

		cfg.renderer.mode               = ps::RendererModule::Mode::Billboard;
		cfg.renderer.renderOrder        = 1;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.smokeTex() };

		cfg.textureSheetAnimation.enabled   = true;
		cfg.textureSheetAnimation.tilesX    = 3;
		cfg.textureSheetAnimation.tilesY    = 3;
		cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
		cfg.textureSheetAnimation.cycles    = 1.f;

		dustParticleSystem_.init(cfg);
	}

	// ── Red Energy Explosion effect ──────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Core"
		);
		cfg.main.looping         = false;
		cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat         = ps::MatUnlit{ .mainTex = assetManager_.stoneTex(), .blend = ps::BlendMode::Additive };
		redEnergyExplosionEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Trails"
		);
		cfg.main.looping         = false;
		cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat         = ps::MatUnlit{ .mainTex = nullptr, .blend = ps::BlendMode::Alpha };

		cfg.trail.enabled              = true;
		cfg.trail.material.mainTex     = assetManager_.trail67Tex();
		cfg.trail.material.blend       = ps::BlendMode::Additive;
		redEnergyExplosionEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Circle"
		);
		cfg.main.looping                = false;
		cfg.main.startColor             = {
			cfg.main.startColor.x() * 2.3773584f,
			cfg.main.startColor.y() * 2.3773584f,
			cfg.main.startColor.z() * 2.3773584f,
			cfg.main.startColor.w()
		};
		cfg.renderer.mode               = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat                = ps::MatUnlit{ .mainTex = assetManager_.circleTex(), .blend = ps::BlendMode::Alpha };
		redEnergyExplosionEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/ShockWaveIn"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.circleTex(), .blend = ps::BlendMode::Alpha };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/ShockWave"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.circleTex(), .blend = ps::BlendMode::Alpha };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Smoke"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.smoke26Tex(), .blend = ps::BlendMode::Alpha };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Flash"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::StretchedBillboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.flashTex(), .blend = ps::BlendMode::Alpha };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}

	// Crystal Cross Fade effect: parent trigger + Crystals sub-emitter only.
	{
		const std::filesystem::path crystalsCrossFadeJson =
			"../resources/effects/Crystals crossfade 2_ParticleSystems.json";

		{
			auto cfg = loadUnityParticleConfig(crystalsCrossFadeJson, "Crystals crossfade 2");
			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.mat  = ps::MatUnlit{ .mainTex = nullptr, .blend = ps::BlendMode::Additive };
			cfg.main.looping  = false;
			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = { {
				.event           = ps::SubEmittersModule::Event::Birth,
				.emitProbability = 1.f,
				.inheritSize     = true,
			} };
			crystalsCrossFadeEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);  // idx 0
		}

		{
			auto cfg = loadUnityParticleConfig(crystalsCrossFadeJson, "Crystals crossfade 2/Crystals");
			cfg.renderer.mode = ps::RendererModule::Mode::StretchedBillboard;
			cfg.renderer.mat  = ps::MatUnlit{
				.mainTex = assetManager_.crystalFree1Tex(),
				.blend = ps::BlendMode::Alpha,
				.color = { 1.15271747f, 1.1794312f, 1.41421354f, 1.f }
			};
			crystalsCrossFadeEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);  // idx 1
		}

		crystalsCrossFadeEffect_.bindSubEmitter(0, 0, 1);
	}

	// ── Earth Spike (Grandbaum ShieldWall 포격) ────────────────────────────────
	// 갈색을 얻으려면 **소재 선택이 먼저**다. tint는 곱셈이라 색을 뺄 수만 있고 더할 수 없는데,
	// crystal 아트(CrystalFree1.dds)는 평균 RGB (0.03, 0.36, 0.73)의 **완전 채도 파랑**이라
	// 어떤 배율을 곱해도 갈색이 안 나온다(R이 0.03이라 갈색 배율 = 거의 검정 = 첫 시도의 실패).
	// → 색이 전부 코드 구동인 소재로 교체: 기둥은 IceSpikes2 메시 + MatTwoSides(중립 노이즈 텍스처),
	//   예고는 magic_circle.dds(채도 0 그레이스케일).
	// MIRROR: client/standalone/game.cpp.
	{
		// 예고 마법진: 융기 전 대상 발밑에 깔리는 납작한 표식(= 회피 창).
		// Unity JSON이 아니라 코드로 구성한다 — 필요한 건 "한 장, 지면에 눕고, 0.85초 뒤 사라짐"뿐이라
		// 어떤 프리팹 시스템보다 단순하다.
		const mu::Vec4     kWarnColor{ 1.35f, 0.62f, 0.18f, 1.f };   // 앰버(흙) — 주 튜닝 노브
		constexpr float    kWarnSize     = 2.6f;    // 지름(m). 히트박스 폭 1.6m보다 크게 잡아 눈에 띄게
		constexpr float    kWarnLifetime = 0.85f;   // lua의 예고(300ms)~융기(1100ms) 창을 덮는다

		ps::ParticleSystemConfig cfg;
		cfg.main.looping      = false;
		cfg.main.duration     = 1.0f;
		cfg.main.lifetimeMin  = kWarnLifetime;
		cfg.main.lifetimeMax  = kWarnLifetime;
		cfg.main.speedMin     = 0.f;
		cfg.main.speedMax     = 0.f;
		cfg.main.startSizeMin = kWarnSize;
		cfg.main.startSizeMax = kWarnSize;
		cfg.main.gravityModifierMin = 0.f;
		cfg.main.gravityModifierMax = 0.f;
		// 지면에 눕히기: World 정렬이면 빌보드가 카메라를 향하지 않고 파티클 회전을 쓴다.
		// 그 회전은 **`billboardRotation3D`(= startRotation3D 오일러)에서만** 오고 이펙트 play
		// 방향(baseRotation)은 안 탄다 — 그래서 lua의 orient가 아니라 여기서 눕혀야 한다.
		// X축 -90°: 쿼드 로컬 +Z(법선)가 월드 +Y로 간다.
		cfg.main.startRotation3DEnabled = true;
		cfg.main.startRotation3DMin = { -3.14159265f * 0.5f, 0.f, 0.f };
		cfg.main.startRotation3DMax = { -3.14159265f * 0.5f, 0.f, 0.f };
		cfg.emission.enabled  = true;
		cfg.emission.emitRate = 0.f;    // PlayMode::Emit의 emit(1)로만 스폰
		cfg.shape.enabled     = true;
		cfg.shape.type        = ps::ShapeModule::Type::Point;
		cfg.renderer.mode      = ps::RendererModule::Mode::Billboard;
		cfg.renderer.alignment = ps::RendererModule::Alignment::World;
		// 밝은 주간 지형 위라 Additive는 묻힌다 → Alpha 데칼로 또렷하게.
		cfg.renderer.mat = ps::MatUnlit{
			.mainTex = assetManager_.magicCircleTex(),
			.blend   = ps::BlendMode::Alpha,
			.color   = kWarnColor
		};
		cfg.colorOverLifetime.enabled  = true;
		cfg.colorOverLifetime.gradient = ColorGradient{
			.keys = {
				{ 0.0f,  { 1.f, 1.f, 1.f, 0.f } },   // 페이드 인
				{ 0.15f, { 1.f, 1.f, 1.f, 1.f } },
				{ 0.80f, { 1.f, 1.f, 1.f, 1.f } },
				{ 1.0f,  { 1.f, 1.f, 1.f, 0.f } },   // 융기 직전 페이드 아웃
			}
		};
		earthSpikeWarnEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);  // idx 0
	}
	{
		// 흙 기둥: spikes(플레이어 스킬)와 같은 IceSpikes2 메시 + MatTwoSides. 이 소재는 색이
		// 전부 코드 값(frontFaces/backFaces/fresnel)이고 텍스처도 중립 노이즈라 갈색이 그대로 나온다.
		// 메시라서 지면 SnapAndAlign 정렬도 실제로 먹는다(빌보드는 안 먹는다).
		const mu::Vec4     kSpikeFrontColor  { 0.40f, 0.24f, 0.11f, 1.f };  // 젖은 흙
		const mu::Vec4     kSpikeBackColor   { 0.78f, 0.53f, 0.28f, 1.f };  // 속면은 밝게
		const mu::Vec4     kSpikeFresnelColor{ 1.00f, 0.68f, 0.32f, 1.f };  // 따뜻한 림
		constexpr float    kSpikeEmission = 2.0f;   // 얼음 원본은 7.0(형광). 흙은 낮춰야 자연스럽다
		constexpr float    kSpikeSize     = 1.4f;   // 주 튜닝 노브. 히트박스 폭 1.6m에 맞춘 값

		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Spikes attack_ParticleSystems.json",
			"Spikes attack/Spikes");
		cfg.renderer.mode     = ps::RendererModule::Mode::Mesh;
		cfg.renderer.pMesh    = assetManager_.meshIceSpikes2();
		cfg.renderer.pSubMesh = assetManager_.meshIceSpikes2()->subMeshes.empty()
		                      ? nullptr
		                      : &assetManager_.meshIceSpikes2()->subMeshes[0];

		ps::MatTwoSides mat   = assetManager_.spikesMaterial();   // 사본에 색만 덮는다
		mat.frontFacesColor   = kSpikeFrontColor;
		mat.backFacesColor    = kSpikeBackColor;
		mat.fresnelColor      = kSpikeFresnelColor;
		mat.backFresnelColor  = kSpikeFresnelColor;
		mat.emission          = kSpikeEmission;
		cfg.renderer.mat      = mat;

		cfg.main.looping      = false;
		// 원본 startSize는 TwoConstants(0 ~ 2.3)라 크기가 매번 달라진다. 히트박스가 고정 OBB이므로
		// 시각 크기도 고정해야 "보이는 것 = 맞는 것"이 된다.
		cfg.main.startSizeMin = kSpikeSize;
		cfg.main.startSizeMax = kSpikeSize;
		cfg.shape.coneRadius  = 0.f;   // 스폰 지터 제거 → 앵커(=히트박스 중심) 정중앙에 솟는다
		cfg.shape.randomPositionAmount = 0.f;
		cfg.colorOverLifetime.enabled  = true;
		cfg.colorOverLifetime.gradient = ColorGradient{
			.keys = {
				{ 0.0f,  { 1.f, 1.f, 1.f, 1.f } },
				{ 0.78f, { 1.f, 1.f, 1.f, 1.f } },
				{ 1.0f,  { 1.f, 1.f, 1.f, 0.f } },
			}
		};
		earthSpikeEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);  // idx 0
	}

	// ── Arrow Effect (Muzzle → mesh flight → Hit) ───────────────────────────
	{
		// System 0: Arrow mesh (parent) — flies in player's forward direction
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin = 40.f;
			cfg.main.speedMax = 40.f;
			cfg.main.startSizeMin = 0.3f;
			cfg.main.startSizeMax = 0.3f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.gravityModifierMin = 0.f;
			cfg.main.gravityModifierMax = 0.f;
			cfg.main.looping = false;
			cfg.main.duration = 0.f;

			cfg.emission.emitRate = 0.f;

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };
			cfg.shape.direction = { 0.f, 0.f, 1.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Mesh;
			cfg.renderer.renderOrder = 2;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowTex(), .blend = ps::BlendMode::Alpha };
			cfg.renderer.pMesh = assetManager_.meshArrow();
			cfg.renderer.pSubMesh = assetManager_.meshArrow()->subMeshes.empty()
				? nullptr : &assetManager_.meshArrow()->subMeshes[0];

			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = {
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Birth,
					.emitProbability = 1.f,
					.emitCount = 1,
					.inheritVelocity = false,
					.inheritColor = false,
					.inheritSize = false,
				},
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f,
					.emitCount = 1,
					.inheritVelocity = false,
					.inheritColor = false,
					.inheritSize = false,
				},
			};

			arrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 32 );
		}

		// System 1: ArrowMuzzle — spawned at arrow birth position
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin = 0.f;
			cfg.main.speedMax = 0.f;
			cfg.main.startSizeMin = 2.0f;
			cfg.main.startSizeMax = 2.0f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowMuzzleTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled = true;
			cfg.textureSheetAnimation.tilesX = 8;
			cfg.textureSheetAnimation.tilesY = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles = 1.f;

			arrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// System 2: ArrowHit — spawned at arrow death position
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin = 0.f;
			cfg.main.speedMax = 0.f;
			cfg.main.startSizeMin = 2.5f;
			cfg.main.startSizeMax = 2.5f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowHitTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled = true;
			cfg.textureSheetAnimation.tilesX = 8;
			cfg.textureSheetAnimation.tilesY = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles = 1.f;

			arrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// System 0 Birth → System 1 (muzzle), System 0 Death → System 2 (hit)
		arrowEffect_.bindSubEmitter( 0, 0, 1 );
		arrowEffect_.bindSubEmitter( 0, 1, 2 );
	}

	// Arrow Volley effect: one caster muzzle, then a simultaneous fan of Arrow meshes.
	{
		// Muzzle: reused once at the caster position.
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin = 0.f;
			cfg.main.speedMax = 0.f;
			cfg.main.startSizeMin = 2.2f;
			cfg.main.startSizeMax = 2.2f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowMuzzleTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled = true;
			cfg.textureSheetAnimation.tilesX = 8;
			cfg.textureSheetAnimation.tilesY = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles = 1.f;

			arrowVolleyMuzzleEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		const float angleStep = kArrowVolleySpreadDegrees
			/ static_cast<float>( kArrowVolleyCount - 1 );
		const float firstAngle = -kArrowVolleySpreadDegrees * 0.5f;
		for ( int i = 0; i < kArrowVolleyCount; ++i ) {
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.45f;
			cfg.main.lifetimeMax = 0.45f;
			cfg.main.speedMin = 38.f;
			cfg.main.speedMax = 38.f;
			cfg.main.startSizeMin = 0.3f;
			cfg.main.startSizeMax = 0.3f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.gravityModifierMin = 0.f;
			cfg.main.gravityModifierMax = 0.f;
			cfg.main.looping = false;
			cfg.main.duration = 0.f;

			const auto yawOffset = mu::rotateYH( mu::Degree( firstAngle + angleStep * static_cast<float>( i ) ) );
			cfg.main.startRotation3D = yawOffset;

			cfg.emission.emitRate = 0.f;

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };
			cfg.shape.direction = { 0.f, 0.f, 1.f };
			cfg.shape.orientation = yawOffset;

			cfg.renderer.mode = ps::RendererModule::Mode::Mesh;
			cfg.renderer.renderOrder = 2;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowTex(), .blend = ps::BlendMode::Alpha };
			cfg.renderer.pMesh = assetManager_.meshArrow();
			cfg.renderer.pSubMesh = assetManager_.meshArrow()->subMeshes.empty()
				? nullptr : &assetManager_.meshArrow()->subMeshes[0];

			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = {
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f,
					.emitCount = 1,
					.inheritVelocity = false,
					.inheritColor = false,
					.inheritSize = false,
				},
			};

			arrowVolleyEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 32 );
		}

		// Shared ArrowHit system for every arrow in the volley.
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin = 0.f;
			cfg.main.speedMax = 0.f;
			cfg.main.startSizeMin = 2.5f;
			cfg.main.startSizeMax = 2.5f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowHitTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled = true;
			cfg.textureSheetAnimation.tilesX = 8;
			cfg.textureSheetAnimation.tilesY = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles = 1.f;

			arrowVolleyEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 32 );
		}

		const int arrowVolleyHitIdx = kArrowVolleyCount;
		for ( int i = 0; i < kArrowVolleyCount; ++i ) {
			arrowVolleyEffect_.bindSubEmitter( i, 0, arrowVolleyHitIdx );
		}
	}

	// Arrow Rain effect: play Muzzle once at the caster, then drop Arrow meshes from above.
	{
		// Muzzle: reused once at the caster position.
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin = 0.f;
			cfg.main.speedMax = 0.f;
			cfg.main.startSizeMin = 2.0f;
			cfg.main.startSizeMax = 2.0f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowMuzzleTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled = true;
			cfg.textureSheetAnimation.tilesX = 8;
			cfg.textureSheetAnimation.tilesY = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles = 1.f;

			arrowRainMuzzleEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// System 0: Arrow mesh rain parent. Emits multiple arrows across a disc above the target.
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.34f;
			cfg.main.lifetimeMax = 0.34f;
			cfg.main.speedMin = 32.f;
			cfg.main.speedMax = 32.f;
			cfg.main.startSizeMin = 0.3f;
			cfg.main.startSizeMax = 0.3f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.gravityModifierMin = 0.f;
			cfg.main.gravityModifierMax = 0.f;
			cfg.main.looping = false;
			cfg.main.duration = 1.2f;
			cfg.main.startRotation3D = mu::rotateXH( mu::Degree( 90.f ) );

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 3, .countMax = 3, .cycleCount = 10, .repeatInterval = 0.12f
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Cone;
			cfg.shape.position = { 0.f, 11.f, 0.f };
			cfg.shape.orientation = mu::rotateXH( mu::Degree( 90.f ) );
			cfg.shape.coneRadius = kArrowRainRadius;
			cfg.shape.coneAngle = 0.f;
			cfg.shape.radiusThickness = 1.f;

			cfg.renderer.mode = ps::RendererModule::Mode::Mesh;
			cfg.renderer.renderOrder = 2;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowTex(), .blend = ps::BlendMode::Alpha };
			cfg.renderer.pMesh = assetManager_.meshArrow();
			cfg.renderer.pSubMesh = assetManager_.meshArrow()->subMeshes.empty()
				? nullptr : &assetManager_.meshArrow()->subMeshes[0];

			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = {
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f,
					.emitCount = 1,
					.inheritVelocity = false,
					.inheritColor = false,
					.inheritSize = false,
				},
			};

			arrowRainEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous, 64 );
		}

		// System 1: ArrowHit, reused where each arrow reaches the ground.
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin = 0.f;
			cfg.main.speedMax = 0.f;
			cfg.main.startSizeMin = 2.5f;
			cfg.main.startSizeMax = 2.5f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowHitTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled = true;
			cfg.textureSheetAnimation.tilesX = 8;
			cfg.textureSheetAnimation.tilesY = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles = 1.f;

			arrowRainEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 32 );
		}

		arrowRainEffect_.bindSubEmitter( 0, 0, 1 );
	}

	// ── EnergyExplosionArrow effect (Charge / Arrow / Hit / HitWhiteBG) ─────
	{
		// System 0: Charge — plays first, then spawns Arrow on death
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin  = 1.6f;
			cfg.main.lifetimeMax  = 1.6f;
			cfg.main.speedMin     = 0.f;
			cfg.main.speedMax     = 0.f;
			cfg.main.startSizeMin = 8.f;
			cfg.main.startSizeMax = 8.f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping      = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type     = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.EnergyExplosionChargeTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 6;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;

			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = {
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f, .emitCount = 1,
				},
			};

			energyExplosionArrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// System 1: Arrow — spawned on Charge Death, then spawns hit effects on death
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.6f;
			cfg.main.lifetimeMax = 0.6f;
			cfg.main.speedMin    = 40.f;
			cfg.main.speedMax    = 40.f;
			cfg.main.startSizeMin = 0.3f;
			cfg.main.startSizeMax = 0.3f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.gravityModifierMin = 0.f;
			cfg.main.gravityModifierMax = 0.f;
			cfg.main.looping = false;
			cfg.main.duration = 0.f;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };
			cfg.shape.direction = { 0.f, 0.f, 1.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Mesh;
			cfg.renderer.renderOrder = 2;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowTex(), .blend = ps::BlendMode::Alpha };
			cfg.renderer.pMesh = assetManager_.meshArrow();
			cfg.renderer.pSubMesh = assetManager_.meshArrow()->subMeshes.empty()
				? nullptr : &assetManager_.meshArrow()->subMeshes[0];
			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = {
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f,
					.emitCount = 1,
					.inheritVelocity = false,
					.inheritColor = false,
					.inheritSize = false,
				},
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f,
					.emitCount = 1,
					.inheritVelocity = false,
					.inheritColor = false,
					.inheritSize = false,
				},
			};

			energyExplosionArrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 32 );
		}

		// System 2: Hit — spawned on Arrow Death (8x6 sprite, Additive)
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin  = 1.6f;
			cfg.main.lifetimeMax  = 1.6f;
			cfg.main.speedMin     = 0.f;
			cfg.main.speedMax     = 0.f;
			cfg.main.startSizeMin = 10.0f;
			cfg.main.startSizeMax = 10.0f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping      = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type     = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.EnergyExplosionHitTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 6;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;

			energyExplosionArrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// System 3: HitWhiteBG — spawned on Arrow Death (8x6 sprite, Multiply)
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin  = 1.6f;
			cfg.main.lifetimeMax  = 1.6f;
			cfg.main.speedMin     = 0.f;
			cfg.main.speedMax     = 0.f;
			cfg.main.startSizeMin = 10.0f;
			cfg.main.startSizeMax = 10.0f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping      = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type     = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.EnergyExplosionHitWhiteBGTex(), .blend = ps::BlendMode::Multiply };

			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 6;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;

			energyExplosionArrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// Charge.Death → Arrow, Arrow.Death → Hit, Arrow.Death → HitWhiteBG
		energyExplosionArrowEffect_.bindSubEmitter( 0, 0, 1 );
		energyExplosionArrowEffect_.bindSubEmitter( 1, 0, 2 );
		energyExplosionArrowEffect_.bindSubEmitter( 1, 1, 3 );

		// Positional SFX on the real spawn: arrow launch (child 1) and explosion
		// (child 2). Fires at the actual impact/max-range point and time, so the
		// explosion sound stays in sync whether the arrow hits or flies its range.
		energyExplosionArrowEffect_.setChildSpawnCallback([](int child, const mu::Vec3& pos) {
			if (child == 1)      INet::ClientApp::sound().playSfx3D("charge_shoot", pos);
			else if (child == 2) INet::ClientApp::sound().playSfx3D("charge_explosion", pos);
		});
	}

	// TornadoShot: same visual as Tornado Continuous, moved forward each frame via setOrigin().
	{
		const std::filesystem::path tornadoJson =
			"../resources/effects/Par_TornadoContinous_ParticleSystems.json";

		const ps::MatWindRing ringMat{
			.mainTex          = assetManager_.texWindRing(),
			.edgeFadePower    = 2.f,
			.edgeFadeStrength = 1.f,
			.color            = { 0.933f, 0.933f, 0.933f, 0.765f },
		};

		{
			auto cfg = loadUnityParticleConfig( tornadoJson, "Par_TornadoContinous" );
			cfg.renderer.mode     = ps::RendererModule::Mode::Mesh;
			cfg.renderer.pMesh    = assetManager_.meshRing();
			cfg.renderer.pSubMesh = assetManager_.meshRing()->subMeshes.empty()
				? nullptr : &assetManager_.meshRing()->subMeshes[0];
			cfg.renderer.mat      = ringMat;
			tornadoShotEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
		}
		{
			auto cfg = loadUnityParticleConfig( tornadoJson, "Par_TornadoContinous/Bottom" );
			cfg.renderer.mode     = ps::RendererModule::Mode::Mesh;
			cfg.renderer.pMesh    = assetManager_.meshRing();
			cfg.renderer.pSubMesh = assetManager_.meshRing()->subMeshes.empty()
				? nullptr : &assetManager_.meshRing()->subMeshes[0];
			cfg.renderer.mat      = ringMat;
			tornadoShotEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
		}
		{
			auto cfg = loadUnityParticleConfig( tornadoJson, "Par_TornadoContinous/RingRise" );
			cfg.renderer.mode     = ps::RendererModule::Mode::Mesh;
			cfg.renderer.pMesh    = assetManager_.meshRing();
			cfg.renderer.pSubMesh = assetManager_.meshRing()->subMeshes.empty()
				? nullptr : &assetManager_.meshRing()->subMeshes[0];
			cfg.renderer.mat      = ringMat;
			tornadoShotEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
		}
		{
			auto cfg = loadUnityParticleConfig( tornadoJson, "Par_TornadoContinous/Par_BurstParticles" );
			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.mat  = ps::MatUnlit{
				.mainTex = assetManager_.texDotParticle(),
				.blend   = ps::BlendMode::Alpha
			};
			tornadoShotEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
		}

		// tornadoMuzzleEffect_: one-shot billboard at shot origin
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin  = 0.5f;
			cfg.main.lifetimeMax  = 0.5f;
			cfg.main.speedMin     = 0.f;
			cfg.main.speedMax     = 0.f;
			cfg.main.startSizeMin = 2.5f;
			cfg.main.startSizeMax = 2.5f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping      = false;
			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts   = {
				ps::EmissionModule::Burst{ .time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1 }
			};
			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat         = ps::MatUnlit{
				.mainTex = assetManager_.windBulletMuzzleTex(),
				.blend   = ps::BlendMode::Additive
			};
			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 6;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;
			tornadoMuzzleEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// tornadoHitEffect_: one-shot billboard at shot destination
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin  = 0.5f;
			cfg.main.lifetimeMax  = 0.5f;
			cfg.main.speedMin     = 0.f;
			cfg.main.speedMax     = 0.f;
			cfg.main.startSizeMin = 3.0f;
			cfg.main.startSizeMax = 3.0f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping      = false;
			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts   = {
				ps::EmissionModule::Burst{ .time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1 }
			};
			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat         = ps::MatUnlit{
				.mainTex = assetManager_.windBulletHitTex(),
				.blend   = ps::BlendMode::Additive
			};
			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 6;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;
			tornadoHitEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}
	}
}

void Game::prepareInGamePartyRoster(const PlayerInfo& myInfo, const std::vector<PlayerNameInfo>& roster) {
	inGamePartyPlayerIds_.clear();
	inGamePartyNameById_.clear();
	inGameMonsterKillsByPlayerId_.clear();
	inGameDamageByPlayerId_.clear();
	inGamePickedItemsByPlayerId_.clear();
	inGameBossLastHitPlayerId_ = -1;
	inGamePartyNameSeq_ = 0;
	if (killCountWidget_) killCountWidget_->reset();
	for (const PlayerNameInfo& info : roster) {
		registerInGamePartyPlayer(info.playerId, info.nickname);
	}
	registerInGamePartyPlayer(myInfo.playerId, myInfo.nickname);
}

void Game::setupPlayer(const PlayerInfo& playerInfo) {
	player_ = std::make_shared<Player>();

	player_->setId(playerInfo.playerId);
	player_->setPos(DirectX::XMLoadFloat3(&playerInfo.pos));
	player_->setOrient(DirectX::XMLoadFloat4(&playerInfo.orient));
	player_->setScale(DirectX::XMLoadFloat3(&playerInfo.scale));
	player_->setModel(assetManager_.modelPlayer());
	player_->setAnimBlender(animSystem_, assetManager_);
	equipPlayerWeapon(*player_, assetManager_, playerInfo.weaponType);
	player_->setHp(playerInfo.hp);
	player_->setMaxHp(playerInfo.maxHp);
	player_->enableBVRendering();
	if (playerWeaponIcon_) {
		playerWeaponIcon_->texture = assetManager_.playerWeaponIcon(playerInfo.weaponType);
	}

	player_->body().setMotionType(MotionType::Dynamic);
	player_->body().setMass(80.f);
	player_->body().setLinearDamping(kPlayerLinearDamping);
	player_->body().setAngularDamping(25.f);
	player_->body().setUprightStiffness(4000.f);

	// 플레이어 레이어로 등록 — 플레이어-플레이어 hard contact는 제외되고
	// resolvePlayerSeparation()의 soft separation이 대신 처리한다.
	physicsWorld_.registerBody(&player_->body(),
		[p = player_.get()]() { p->rebuildBodyBVH(); },
		kLayerPlayer, kPlayerCollisionMask);

	camera_.setTargetObject(player_);
	camera_.setOffsetFromTarget(mu::Vec3(0.f, 1.8f, -2.5f));
	camera_.setOffsetTargetPivot(mu::Vec3(0.f, 1.f, 0.f));
	camera_.setPerspective(mu::Degree(90.f),
		static_cast<float>(gClientRect.right - gClientRect.left) / (gClientRect.bottom - gClientRect.top),
		0.1f, 500.f
	);
	camera_.setPhysicsWorld( &physicsWorld_ );

	idPlayerMap_[playerInfo.playerId] = player_;
	registerInGamePartyPlayer(playerInfo.playerId, playerInfo.nickname);

	// Game은 로비 연결을 유지한 채 여러 경기를 재사용한다. ParticleEffect::addSystem은
	// 누적되므로 두 번째 S_Enter에서 다시 구성하면 VFX 시스템이 경기마다 배증한다.
	if (!particleEffectsReady_) {
		setParticle();
		particleEffectsReady_ = true;
	}

	// Compile skills and build dispatch context (online mode: prediction-only, no damage events).
	{
		SkillCompiler compiler;
		const Skeleton* pSkeleton = player_->model() ? &player_->model()->skeleton : nullptr;
		auto assets = compiler.compileAll("../resources/skills", pSkeleton);
		// VFXParticle 히트박스용 게임플레이 설정(effect JSON + lua 오버라이드) 빌드.
		buildVfxGameplayConfigs(assets, "../resources");
		skillSystem_.registerAssets(std::move(assets));

		// Build the bottom-right skill dial from this weapon's loadout metadata.
		setupSkillDial(playerInfo.weaponType);

		// 256은 상한이 아니라 초기 크기 힌트다(registerSkillObject가 필요한 만큼 늘린다).
		// 이전 세션 잔재를 지우는 역할도 겸하므로, 아래에서 이미 알고 있는 오브젝트를 복구한다.
		skillObjectById_.assign(256, nullptr);
		player_->setFaction(Faction::Players);
		registerSkillObject(player_->getId(), player_.get());
		// setupPlayer 이전에 도착한 오브젝트 복구(패킷 순서는 보장되지 않는다).
		// 몬스터는 S_NpcSpawnBatch가 S_Enter보다 먼저 올 수 있고, 원격 플레이어도 마찬가지다.
		for (auto& [id, monster] : idMonsterMap_) registerSkillObject(id, monster);
		for (auto& [id, other]   : idPlayerMap_)  registerSkillObject(id, other.get());

		// vfxId 0 is reserved for hit/blood VFX. vfxId 1..20 bind 1:1 to each
		// built ParticleEffect, mirroring StandAlone::Game::skillVfxById_ so the
		// same skill .lua PlayVFX events resolve identically in online mode.
		skillVfxById_.assign(21, nullptr);
		skillVfxById_[0]  = &bloodEffect_;                // Blood hit (칼/창/완드 피격)
		skillVfxById_[1]  = &swordSlash1Effect_;          // SwordSlash
		skillVfxById_[2]  = &slashWaveEffect_;            // SlashWave
		skillVfxById_[3]  = &swordSlashComboEffect_;      // SlashCombo
		skillVfxById_[4]  = &swordSlash7Effect_;          // Slash7
		skillVfxById_[5]  = &spikesAttackEffect_;         // Spikes
		skillVfxById_[6]  = &crystalsFrontAttackEffect_;  // CrystalsFrontAttack
		skillVfxById_[7]  = &aoESlashGreenEffect_;        // AoESlashGreen
		skillVfxById_[8]  = &redEnergyExplosionEffect_;   // RedEnergyExplosion
		skillVfxById_[9]  = &crystalsCrossFadeEffect_;    // CrystalsCrossFade
		skillVfxById_[10] = &arrowEffect_;                // Arrow
		skillVfxById_[11] = &arrowVolleyEffect_;          // ArrowVolley
		skillVfxById_[12] = &arrowRainEffect_;            // ArrowRain
		skillVfxById_[13] = &energyExplosionArrowEffect_; // EnergyExplosionArrow
		skillVfxById_[14] = &tornadoShotEffect_;          // TornadoShot
		skillVfxById_[15] = &piercingEffect_;             // Piercing
		skillVfxById_[16] = &piercingSlashEffect_;        // PiercingSlash
		skillVfxById_[17] = &piercingCircleSlashEffect_;  // PiercingCircleSlash
		skillVfxById_[18] = &piercingMultiEffect_;        // PiercingMulti
		skillVfxById_[19] = &earthSpikeWarnEffect_;       // EarthSpike telegraph (Grandbaum)
		skillVfxById_[20] = &earthSpikeEffect_;           // EarthSpike pillar   (Grandbaum)

		skillCtx_.objectById          = skillObjectById_.data();
		skillCtx_.objectByIdSize      = static_cast<int>(skillObjectById_.size());
		skillCtx_.vfxById             = skillVfxById_.data();
		skillCtx_.vfxByIdSize         = static_cast<int>(skillVfxById_.size());
		skillCtx_.camera              = &camera_;
		skillCtx_.clientPredictionOnly = true;
		// Wire PlaySound timeline events to the 3D SFX backend (cosmetic; caster position).
		skillCtx_.playSound = [](const char* name, mu::Vec3 pos, float maxMs, float fadeMs, float vol) {
			INet::ClientApp::sound().playSfx3D(name, pos, vol, maxMs, fadeMs);
		};

		// Terrain query for ground-snapped placement (PlayVFX ground flags,
		// AttachType::Ground hitboxes, particle ground-conform/collision).
		groundSampler_.heightAt = [this](float x, float z) {
			return chunkManager_.empty() ? 0.f : chunkManager_.heightAtWorld(x, z);
		};
		groundSampler_.normalAt = [this](float x, float z) {
			return chunkManager_.empty() ? mu::Vec3{ 0.f, 1.f, 0.f } : chunkManager_.normalAtWorld(x, z);
		};
		skillCtx_.ground = &groundSampler_;

		// 이펙트는 이 시점에 이미 전부 구성되어 있다: 스킬 lua의 addVFX systems
		// 구성으로 빌드된 게임플레이 설정을 각 시스템에 주입 (결정론 모드 활성).
		skillSystem_.bindVfxGameplayConfigs(skillVfxById_.data(),
		                                    static_cast<int>(skillVfxById_.size()));
	}

	// Local player has finished spawning in-game (server S_Enter): show the
	// authored intro/monologue once. No-op if dialogues.json lacks the event.
	if (dialogueSystem_.show("sample_intro")) {
		applyCursorPolicy();
	}
}

void Game::setupGround(const ObjectInfo& groundInfo) {
	// ground_ = std::make_shared<Cube>();

	// ground_->setId(groundInfo.objectId);
	// ground_->setMaterialSetIdx(groundInfo.materialSetIdx);
	// ground_->setPos(DirectX::XMLoadFloat3(&groundInfo.pos));
	// ground_->setOrient(DirectX::XMLoadFloat4(&groundInfo.orient));
	// ground_->setScale(DirectX::XMLoadFloat3(&groundInfo.scale));
	// ground_->setModel(assetManager_.modelCube());
	// ground_->enableBVRendering();
}

void Game::createOtherPlayer(const ObjectInfo& otherPlayerInfo) {
	auto otherPlayer = std::make_shared<Player>();

	otherPlayer->setId(otherPlayerInfo.objectId);
	otherPlayer->setPos(DirectX::XMLoadFloat3(&otherPlayerInfo.pos));
	otherPlayer->setOrient(DirectX::XMLoadFloat4(&otherPlayerInfo.orient));
	otherPlayer->setScale(DirectX::XMLoadFloat3(&otherPlayerInfo.scale));
	otherPlayer->setModel(assetManager_.modelPlayer());
	otherPlayer->setAnimBlender(animSystem_, assetManager_);
	equipPlayerWeapon(*otherPlayer, assetManager_, otherPlayerInfo.weaponType);
	otherPlayer->setHp(otherPlayerInfo.hp);
	otherPlayer->setMaxHp(otherPlayerInfo.maxHp);
	otherPlayer->setFaction(Faction::Players);
	otherPlayer->enableBVRendering();

	otherPlayer->body().setMotionType(MotionType::Kinematic);
	otherPlayer->body().setMass(80.f);
	// 원격 플레이어는 velocity가 네트워크 패킷으로만 결정된다.
	// 물리 감속(linearDamping)을 적용하면 패킷 간격 사이에 velocity가 소멸해
	// 애니메이션 블렌딩 비율이 진동하므로, 감속을 0으로 설정한다.
	otherPlayer->body().setLinearDamping(0.f);
	otherPlayer->body().setAngularDamping(100.f);

	// 서버 주도 객체: 패킷으로 pos/vel이 설정되면 PhysicsWorld가 Kinematic 적분으로
	// 패킷 간격 사이를 dead-reckoning 보간한다.
	// 플레이어 레이어로 등록 — 로컬 플레이어와의 hard contact는 제외되고
	// resolvePlayerSeparation()이 soft separation으로 처리한다.
	physicsWorld_.registerBody(&otherPlayer->body(),
		[p = otherPlayer.get()]() { p->rebuildBodyBVH(); },
		kLayerPlayer, kPlayerCollisionMask);

	registerInGamePartyPlayer(otherPlayerInfo.objectId);
	createOtherPlayerHud(otherPlayerInfo.objectId, otherPlayer.get(), otherPlayerInfo.weaponType);

	otherPlayer->setRenderObjectId(nextRenderObjId_++);

	registerSkillObject(otherPlayerInfo.objectId, otherPlayer.get());

	otherPlayers_.push_back(otherPlayer);
	idPlayerMap_[otherPlayerInfo.objectId] = otherPlayer;
}

void Game::createOtherPlayer(const PlayerInfo& otherPlayerInfo) {
	auto otherPlayer = std::make_shared<Player>();

	otherPlayer->setId(otherPlayerInfo.playerId);
	otherPlayer->setPos(DirectX::XMLoadFloat3(&otherPlayerInfo.pos));
	otherPlayer->setOrient(DirectX::XMLoadFloat4(&otherPlayerInfo.orient));
	otherPlayer->setScale(DirectX::XMLoadFloat3(&otherPlayerInfo.scale));
	otherPlayer->setModel(assetManager_.modelPlayer());
	otherPlayer->setAnimBlender(animSystem_, assetManager_);
	equipPlayerWeapon(*otherPlayer, assetManager_, otherPlayerInfo.weaponType);
	otherPlayer->setHp(otherPlayerInfo.hp);
	otherPlayer->setMaxHp(otherPlayerInfo.maxHp);
	otherPlayer->setFaction(Faction::Players);
	otherPlayer->enableBVRendering();

	otherPlayer->body().setMotionType(MotionType::Kinematic);
	otherPlayer->body().setMass(80.f);
	// 원격 플레이어는 velocity가 네트워크 패킷으로만 결정된다.
	// 물리 감속(linearDamping)을 적용하면 패킷 간격 사이에 velocity가 소멸해
	// 애니메이션 블렌딩 비율이 진동하므로, 감속을 0으로 설정한다.
	otherPlayer->body().setLinearDamping(0.f);
	otherPlayer->body().setAngularDamping(100.f);

	// 서버 주도 객체: 패킷으로 pos/vel이 설정되면 PhysicsWorld가 Kinematic 적분으로
	// 패킷 간격 사이를 dead-reckoning 보간한다.
	// 플레이어 레이어로 등록 — 로컬 플레이어와의 hard contact는 제외되고
	// resolvePlayerSeparation()이 soft separation으로 처리한다.
	physicsWorld_.registerBody(&otherPlayer->body(),
		[p = otherPlayer.get()]() { p->rebuildBodyBVH(); },
		kLayerPlayer, kPlayerCollisionMask);

	// S_Enter_Other(뒤늦게 합류한 플레이어)는 PlayerInfo에 닉네임이 실려 온다.
	registerInGamePartyPlayer(otherPlayerInfo.playerId, otherPlayerInfo.nickname);
	createOtherPlayerHud(otherPlayerInfo.playerId, otherPlayer.get(), otherPlayerInfo.weaponType);

	otherPlayer->setRenderObjectId(nextRenderObjId_++);

	registerSkillObject(otherPlayerInfo.playerId, otherPlayer.get());

	otherPlayers_.push_back(otherPlayer);
	idPlayerMap_[otherPlayerInfo.playerId] = otherPlayer;
}

void Game::createGoblin(const ObjectInfo& goblinInfo) {
	// Idempotent against duplicate spawn packets. S_Enter and S_NpcSpawnBatch can both list
	// the same npc (e.g. the player streams in/out of an area whose monsters already exist).
	// Re-creating would orphan the previous object in goblins_ (still rendered) while
	// idMonsterMap_ points to the new one — a "ghost" that gets neither moves nor hits, so it
	// freezes/sinks and ignores damage. Skip if this id is already live (corpses are absent
	// from idMonsterMap_, so a genuine respawn of a dead npc still creates a fresh object).
	if (idMonsterMap_.count(goblinInfo.objectId)) return;

	auto goblin = std::make_shared<Goblin>();

	goblin->setId(goblinInfo.objectId);
	goblin->setPos(DirectX::XMLoadFloat3(&goblinInfo.pos));
	goblin->setOrient(DirectX::XMLoadFloat4(&goblinInfo.orient));
	goblin->setScale(DirectX::XMLoadFloat3(&goblinInfo.scale));
	goblin->setModel(assetManager_.modelGoblin());
	goblin->setAnimBlender(animSystem_, assetManager_);

	if (goblin->model() && goblin->model()->ragdollDef) {
		goblin->ragdoll()->build(
			goblin->model()->skeleton,
			*goblin->model()->ragdollDef,
			physicsWorld_,
			goblin->body().scale()
		);
	}

	goblin->setHp(goblinInfo.hp);
	goblin->setMaxHp(goblinInfo.maxHp);
	goblin->setFaction(Faction::Monsters);
	goblin->enableBVRendering();

	goblin->body().setMotionType(MotionType::Kinematic);
	goblin->body().setMass(40.f);
	//goblin->body().setLinearDamping(20.f);
	goblin->body().setLinearDamping(0.f);
	goblin->body().setAngularDamping(100.f);

	// physicsWorld_.registerBody(&goblin->body(),
		// [p = goblin.get()]() { p->rebuildBodyBVH(); });

	{
		auto* bar = static_cast<UI::ProgressBar*>(
			uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
		);
		bar->anchor    = UI::Anchors::TopLeft;
		bar->pivot     = UI::Pivots::TopLeft;
		bar->width     = UI::DimValue::px(80.f);
		bar->height    = UI::DimValue::px(8.f);
		bar->fillColor = { 0.9f, 0.15f, 0.1f, 1.f };
		bar->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
		bar->visible   = false;
		goblinHpBars_[goblinInfo.objectId] = { goblin.get(), bar, 2.5f };
	}

	goblin->setRenderObjectId(nextRenderObjId_++);

	registerSkillObject(goblinInfo.objectId, goblin.get());

	goblins_.push_back(goblin);
	idGoblinMap_[goblinInfo.objectId]    = goblin;
	idMonsterMap_[goblinInfo.objectId]   = goblin.get();
	respawnKind_[goblinInfo.objectId]      = MonsterKind::Goblin;
	monsterSpawnInfo_[goblinInfo.objectId] = goblinInfo;
}

// 전술 전투 중간보스 전용. Goblin과 동일한 셋업이나 모델만 modelHobgoblin()을 쓴다
// (같은 리그를 공유하므로 goblinAnimations()/goblinHpBars_/idGoblinMap_ 등은 그대로 재사용).
void Game::createHobgoblin(const ObjectInfo& hobgoblinInfo) {
	if (idMonsterMap_.count(hobgoblinInfo.objectId)) return;   // skip duplicate spawn (see createGoblin)
	auto hobgoblin = std::make_shared<Hobgoblin>();

	hobgoblin->setId(hobgoblinInfo.objectId);
	hobgoblin->setPos(DirectX::XMLoadFloat3(&hobgoblinInfo.pos));
	hobgoblin->setOrient(DirectX::XMLoadFloat4(&hobgoblinInfo.orient));
	hobgoblin->setScale(DirectX::XMLoadFloat3(&hobgoblinInfo.scale));
	hobgoblin->setModel(assetManager_.modelHobgoblin());
	hobgoblin->setAnimBlender(animSystem_, assetManager_);

	if (hobgoblin->model() && hobgoblin->model()->ragdollDef) {
		hobgoblin->ragdoll()->build(
			hobgoblin->model()->skeleton,
			*hobgoblin->model()->ragdollDef,
			physicsWorld_,
			hobgoblin->body().scale()
		);
	}

	hobgoblin->setHp(hobgoblinInfo.hp);
	hobgoblin->setMaxHp(hobgoblinInfo.maxHp);
	hobgoblin->setFaction(Faction::Monsters);
	hobgoblin->enableBVRendering();

	hobgoblin->body().setMotionType(MotionType::Kinematic);
	hobgoblin->body().setMass(40.f);
	hobgoblin->body().setLinearDamping(0.f);
	hobgoblin->body().setAngularDamping(100.f);

	{
		auto* bar = static_cast<UI::ProgressBar*>(
			uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
		);
		bar->anchor    = UI::Anchors::TopLeft;
		bar->pivot     = UI::Pivots::TopLeft;
		bar->width     = UI::DimValue::px(80.f);
		bar->height    = UI::DimValue::px(8.f);
		bar->fillColor = kNamedMonsterHpColor;
		bar->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
		bar->visible   = false;
		goblinHpBars_[hobgoblinInfo.objectId] = { hobgoblin.get(), bar, 2.5f };
	}

	hobgoblin->setRenderObjectId(nextRenderObjId_++);

	registerSkillObject(hobgoblinInfo.objectId, hobgoblin.get());

	// Heat distortion: ancient-tree mid-boss — sickly emerald haze, tall plume.
	bossHeatProfiles_[hobgoblinInfo.objectId] = BossHeatState{
		.tint = mu::Vec3(0.22f, 0.85f, 0.28f), .intensity = 0.56f,
		.worldRadius = 1.8f, .heightBias = 1.f, .aspectY = 1.6f,
		.warpAmp = 0.01f, .shimmerSpeed = 2.4f, .lastPos = hobgoblin->pos()
	};

	goblins_.push_back(hobgoblin);
	idGoblinMap_[hobgoblinInfo.objectId]  = hobgoblin;
	idMonsterMap_[hobgoblinInfo.objectId] = hobgoblin.get();
	respawnKind_[hobgoblinInfo.objectId]      = MonsterKind::Goblin;
	monsterSpawnInfo_[hobgoblinInfo.objectId] = hobgoblinInfo;
}

void Game::createSnake(const ObjectInfo& info) {
	if (idMonsterMap_.count(info.objectId)) return;   // skip duplicate spawn (see createGoblin)
	auto snake = std::make_shared<Snake>();

	snake->setId(info.objectId);
	snake->setPos(DirectX::XMLoadFloat3(&info.pos));
	snake->setOrient(DirectX::XMLoadFloat4(&info.orient));
	snake->setScale(DirectX::XMLoadFloat3(&info.scale));
	snake->setModel(assetManager_.modelSnake());
	snake->setAnimBlender(animSystem_, assetManager_);

	if (snake->model() && snake->model()->ragdollDef) {
		snake->ragdoll()->build(
			snake->model()->skeleton,
			*snake->model()->ragdollDef,
			physicsWorld_,
			snake->body().scale()
		);
	}

	snake->setHp(info.hp);
	snake->setMaxHp(info.maxHp);
	snake->setFaction(Faction::Monsters);
	snake->enableBVRendering();

	snake->body().setMotionType(MotionType::Kinematic);
	snake->body().setMass(40.f);
	snake->body().setLinearDamping(0.f);
	snake->body().setAngularDamping(100.f);

	{
		auto* bar = static_cast<UI::ProgressBar*>(
			uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
		);
		bar->anchor    = UI::Anchors::TopLeft;
		bar->pivot     = UI::Pivots::TopLeft;
		bar->width     = UI::DimValue::px(80.f);
		bar->height    = UI::DimValue::px(8.f);
		bar->fillColor = { 0.9f, 0.15f, 0.1f, 1.f };
		bar->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
		bar->visible   = false;
		snakeHpBars_[info.objectId] = { snake.get(), bar, 2.5f };
	}

	snake->setRenderObjectId(nextRenderObjId_++);

	registerSkillObject(info.objectId, snake.get());

	snakes_.push_back(snake);
	idSnakeMap_[info.objectId]    = snake;
	idMonsterMap_[info.objectId]  = snake.get();
	respawnKind_[info.objectId]      = MonsterKind::Snake;
	monsterSpawnInfo_[info.objectId] = info;
}

void Game::createMushroom(const ObjectInfo& info) {
	if (idMonsterMap_.count(info.objectId)) return;   // skip duplicate spawn (see createGoblin)
	auto mushroom = std::make_shared<Mushroom>();

	mushroom->setId(info.objectId);
	mushroom->setPos(DirectX::XMLoadFloat3(&info.pos));
	mushroom->setOrient(DirectX::XMLoadFloat4(&info.orient));
	mushroom->setScale(DirectX::XMLoadFloat3(&info.scale));
	mushroom->setModel(assetManager_.modelMushroom());
	mushroom->setAnimBlender(animSystem_, assetManager_);

	if (mushroom->model() && mushroom->model()->ragdollDef) {
		mushroom->ragdoll()->build(
			mushroom->model()->skeleton,
			*mushroom->model()->ragdollDef,
			physicsWorld_,
			mushroom->body().scale()
		);
	}

	mushroom->setHp(info.hp);
	mushroom->setMaxHp(info.maxHp);
	mushroom->setFaction(Faction::Monsters);
	mushroom->enableBVRendering();

	mushroom->body().setMotionType(MotionType::Kinematic);
	mushroom->body().setMass(50.f);
	mushroom->body().setLinearDamping(0.f);
	mushroom->body().setAngularDamping(100.f);

	{
		auto* bar = static_cast<UI::ProgressBar*>(
			uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
		);
		bar->anchor    = UI::Anchors::TopLeft;
		bar->pivot     = UI::Pivots::TopLeft;
		bar->width     = UI::DimValue::px(80.f);
		bar->height    = UI::DimValue::px(8.f);
		bar->fillColor = { 0.9f, 0.15f, 0.1f, 1.f };
		bar->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
		bar->visible   = false;
		mushroomHpBars_[info.objectId] = { mushroom.get(), bar, 2.5f };
	}

	mushroom->setRenderObjectId(nextRenderObjId_++);

	registerSkillObject(info.objectId, mushroom.get());

	mushrooms_.push_back(mushroom);
	idMushroomMap_[info.objectId]   = mushroom;
	idMonsterMap_[info.objectId]    = mushroom.get();
	respawnKind_[info.objectId]      = MonsterKind::Mushroom;
	monsterSpawnInfo_[info.objectId] = info;
}

// Shared spawn wiring for the newer monster types (Bomber/Birdy/Slime/Treant). Fills the
// common state (model/blender/ragdoll/body/HP bar + id maps) and inserts the HP bar into the
// caller's per-type map; the caller pushes the typed shared_ptr into its own vector. Mirrors
// the body of createSnake without the type-specific container/id-map lines.
void Game::configureNetMonster(const std::shared_ptr<Object>& obj, const ObjectInfo& info,
                               const Model* model, MonsterKind kind, float mass,
                               std::unordered_map<uint16, MonsterHpEntry>& hpBars,
                               bool isNamed) {
	obj->setId(info.objectId);
	obj->setPos(DirectX::XMLoadFloat3(&info.pos));
	obj->setOrient(DirectX::XMLoadFloat4(&info.orient));
	obj->setScale(DirectX::XMLoadFloat3(&info.scale));
	obj->setModel(model);
	obj->setAnimBlender(animSystem_, assetManager_);   // virtual -> concrete monster blender

	if (obj->model() && obj->model()->ragdollDef) {
		obj->ragdoll()->build(
			obj->model()->skeleton,
			*obj->model()->ragdollDef,
			physicsWorld_,
			obj->body().scale()
		);
	}

	obj->setHp(info.hp);
	obj->setMaxHp(info.maxHp);
	obj->setFaction(Faction::Monsters);
	obj->enableBVRendering();

	obj->body().setMotionType(MotionType::Kinematic);
	obj->body().setMass(mass);
	obj->body().setLinearDamping(0.f);
	obj->body().setAngularDamping(100.f);

	// Monsters get their own interpolation window: S_NpcMoveBatch is sent on EVERY room tick
	// (60Hz), not at the 20Hz the Object default was tuned for (remote players' S_Move). With
	// the 50ms default, each packet resets netInterpAcc_ before tNet can pass 0.33, so only a
	// third of every server step was ever interpolated and the remaining two thirds arrived as
	// a per-packet jump. The mesh translated at the right *average* speed but in 60Hz stutters,
	// which reads as sliding against a smoothly-playing walk cycle (the anim rate is driven by
	// the true server velocity). Matching the window to the send rate makes tNet reach exactly
	// 1.0 as the next packet lands. Documented in docs/gameArchitecture.md step 8.
	obj->netInterpDuration_ = kNpcMoveInterval;

	{
		auto* bar = static_cast<UI::ProgressBar*>(
			uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
		);
		bar->anchor    = UI::Anchors::TopLeft;
		bar->pivot     = UI::Pivots::TopLeft;
		bar->width     = UI::DimValue::px(80.f);
		bar->height    = UI::DimValue::px(8.f);
		bar->fillColor = isNamed
			? kNamedMonsterHpColor
			: DirectX::XMFLOAT4{ 0.9f, 0.15f, 0.1f, 1.f };
		bar->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
		bar->visible   = false;
		hpBars[info.objectId] = { obj.get(), bar, 2.5f };
	}

	obj->setRenderObjectId(nextRenderObjId_++);

	registerSkillObject(info.objectId, obj.get());

	idMonsterMap_[info.objectId]     = obj.get();
	respawnKind_[info.objectId]      = kind;
	monsterSpawnInfo_[info.objectId] = info;
	// (caller pushes obj into its own typed vector)
}

void Game::createBomber(const ObjectInfo& info) {
	if (idMonsterMap_.count(info.objectId)) return;   // skip duplicate spawn (see createGoblin)
	auto bomber = std::make_shared<Bomber>();
	configureNetMonster(bomber, info, assetManager_.modelBomber(), MonsterKind::Bomber, 70.f, bomberHpBars_);
	bombers_.push_back(bomber);
}

void Game::createBirdy(const ObjectInfo& info) {
	if (idMonsterMap_.count(info.objectId)) return;
	auto birdy = std::make_shared<Birdy>();
	configureNetMonster(birdy, info, assetManager_.modelBirdy(), MonsterKind::Birdy, 40.f, birdyHpBars_);
	birdys_.push_back(birdy);
}

void Game::createSlime(const ObjectInfo& info) {
	if (idMonsterMap_.count(info.objectId)) return;
	auto slime = std::make_shared<Slime>();
	configureNetMonster(slime, info, assetManager_.modelSlime(), MonsterKind::Slime, 90.f, slimeHpBars_);
	slimes_.push_back(slime);
}

void Game::createTreant(const ObjectInfo& info) {
	if (idMonsterMap_.count(info.objectId)) return;
	auto treant = std::make_shared<Treant>();
	configureNetMonster(treant, info, assetManager_.modelTreant(), MonsterKind::Treant, 120.f, treantHpBars_);
	treants_.push_back(treant);
}

// Grandbaum/Isys 미드보스: 전용 모델로 스폰하되 corpse/래그돌/에너지오브 정합을 위해 변종 베이스의
// MonsterKind(Treant/Birdy)로 라우팅한다. 컨테이너도 변종 베이스(treants_/birdys_)에 저장한다.
void Game::createGrandbaum(const ObjectInfo& info) {
	if (idMonsterMap_.count(info.objectId)) return;
	auto grandbaum = std::make_shared<Grandbaum>();
	configureNetMonster(grandbaum, info, assetManager_.modelGrandbaum(), MonsterKind::Treant, 120.f, treantHpBars_, true);
	treants_.push_back(std::static_pointer_cast<Treant>(grandbaum));
	bossNpcIds_.insert(info.objectId);   // 미니맵 주황 아이콘 판별용
	// Heat distortion: ancient-tree mid-boss — sickly emerald haze, tall plume.
	bossHeatProfiles_[info.objectId] = BossHeatState{
		.tint = mu::Vec3(0.36f, 0.16f, 0.08f), .intensity = 0.56f,
		.worldRadius = 4.f, .heightBias = 2.4f, .aspectY = 1.8f,
		.warpAmp = 0.01f, .shimmerSpeed = 2.4f, .lastPos = mu::Vec3(DirectX::XMLoadFloat3(&info.pos))
	};
}

void Game::createIsys(const ObjectInfo& info) {
	if (idMonsterMap_.count(info.objectId)) return;
	auto isys = std::make_shared<Isys>();
	configureNetMonster(isys, info, assetManager_.modelIsys(), MonsterKind::Birdy, 60.f, birdyHpBars_, true);
	birdys_.push_back(std::static_pointer_cast<Birdy>(isys));
	bossNpcIds_.insert(info.objectId);   // 미니맵 주황 아이콘 판별용
	// Heat distortion: ancient-tree mid-boss — sickly emerald haze, tall plume.
	bossHeatProfiles_[info.objectId] = BossHeatState{
		.tint = mu::Vec3(0.95f, 0.14f, 0.5f), .intensity = 0.56f,
		.worldRadius = 2.1f, .heightBias = 1.2f, .aspectY = 1.5f,
		.warpAmp = 0.01f, .shimmerSpeed = 2.4f, .lastPos = mu::Vec3(DirectX::XMLoadFloat3(&info.pos))
	};
}

// Final boss: own 14-clip rig, dedicated MonsterKind::Boss container/corpse routing.
void Game::createBoss(const ObjectInfo& info) {
	if (idMonsterMap_.count(info.objectId)) return;
	auto boss = std::make_shared<Boss>();
	configureNetMonster(boss, info, assetManager_.modelBoss(), MonsterKind::Boss, 150.f, bossHpBars_, true);
	bosses_.push_back(boss);
	bossNpcIds_.insert(info.objectId);   // 미니맵 주황 아이콘 판별용
	// Heat distortion: ancient-tree mid-boss — sickly emerald haze, tall plume.
	bossHeatProfiles_[info.objectId] = BossHeatState{
		.tint = mu::Vec3(0.4f, 0.2f, 0.6f), .intensity = 0.72f,
		.worldRadius = 5.f, .heightBias = 4.2f, .aspectY = 2.f,
		.warpAmp = 0.006f, .shimmerSpeed = 4.f, .lastPos = mu::Vec3(DirectX::XMLoadFloat3(&info.pos))
	};
}

// === Client-authored corpse pipeline =======================================
// A dead monster is detached from server-synced containers into corpses_ (with a
// fresh RenderObjectId, carrying its HP bar). The corpse holds its ragdoll for a
// few seconds, then dissolves into energy orbs, and is removed only after every
// orb is absorbed. Respawns borrow a fresh object from a per-kind pool so the
// corpse animation is never cut short by a respawn packet.

u32t Game::migrateToCorpse(const std::shared_ptr<Object>& obj, MonsterKind kind, uint16 npcId) {
	// Keep the object's renderObjectId (it is stable per object and the monotonic counter
	// already guarantees no two live objects share one — no need to burn a fresh id here,
	// which would let the counter overflow over many death/respawn cycles).
	// Reassign the network/game id to the fixed detached-corpse sentinel so the corpse can
	// never collide with a server-synced id (incl. a respawn that reuses npcId). Corpses are
	// not looked up by id, so a shared sentinel is safe and needs no per-corpse allocation.
	obj->setId(kDetachedCorpseId);
	detachedNpcIds_.insert(npcId);
	setNpcStatusFlags(npcId, npcStatusMask(NpcStatusFlag::None));

	// Freeze the detached body. It no longer receives server moves (advanceState), so the
	// stale prev != curr left by the death-frame move (e.g. knockback launching curr up
	// while prev is on the ground) makes interpolatePos = lerp(prev, curr, t) oscillate
	// renderState_.world every frame as t cycles 0->1 — the corpse's BV/cull bounds (and
	// orb spawn anchor) sink and pop back up repeatedly. snapToCurrent() pins prev = curr.
	obj->body().setLinearVel(mu::Vec3{});
	obj->body().snapToCurrent();

	Corpse c;
	c.obj      = obj;
	c.kind     = kind;
	c.origId   = npcId;
	// Reuse the (stable, unique-per-object) renderObjectId as the orb-association id: a
	// corpse is exactly one object, so it is unique among concurrently-active corpses, and
	// it is freed for reuse only after all orbs are absorbed. No separate counter to overflow.
	c.corpseId = obj->renderObjectId();
	c.phase    = Corpse::Phase::Ragdoll;
	c.age      = 0.f;

	auto grabBar = [&](auto& barMap) {
		if (auto it = barMap.find(npcId); it != barMap.end()) {
			c.hpBar = it->second.hpBar;
			if (c.hpBar) c.hpBar->visible = false;
			barMap.erase(it);
		}
	};

	switch (kind) {
	case MonsterKind::Goblin:
		grabBar(goblinHpBars_);
		idGoblinMap_.erase(npcId);
		std::erase(goblins_, std::static_pointer_cast<Goblin>(obj));
		break;
	case MonsterKind::Snake:
		grabBar(snakeHpBars_);
		idSnakeMap_.erase(npcId);
		std::erase(snakes_, std::static_pointer_cast<Snake>(obj));
		break;
	case MonsterKind::Mushroom:
		grabBar(mushroomHpBars_);
		idMushroomMap_.erase(npcId);
		std::erase(mushrooms_, std::static_pointer_cast<Mushroom>(obj));
		break;
	case MonsterKind::Bomber:
		grabBar(bomberHpBars_);
		std::erase(bombers_, std::static_pointer_cast<Bomber>(obj));
		break;
	case MonsterKind::Birdy:
		grabBar(birdyHpBars_);
		std::erase(birdys_, std::static_pointer_cast<Birdy>(obj));
		break;
	case MonsterKind::Slime:
		grabBar(slimeHpBars_);
		std::erase(slimes_, std::static_pointer_cast<Slime>(obj));
		break;
	case MonsterKind::Treant:
		grabBar(treantHpBars_);
		std::erase(treants_, std::static_pointer_cast<Treant>(obj));
		break;
	case MonsterKind::Boss:
		grabBar(bossHpBars_);
		std::erase(bosses_, std::static_pointer_cast<Boss>(obj));
		break;
	}
	idMonsterMap_.erase(npcId);
	unregisterSkillObject(npcId);

	// A corpse is not a barrier: drop any stale barrier registration so the raw pointer in
	// barrierObjects_ can't outlive this object's current role (and so a pooled reuse does
	// not inherit barrierActive_). setNpcBarrier re-adds it if the server re-enables one.
	if (obj->isBarrierActive()) {
		obj->setBarrierActive(false);
		barrierObjects_.erase(
			std::remove(barrierObjects_.begin(), barrierObjects_.end(), obj.get()),
			barrierObjects_.end());
	}
	obj->setHitImpulseImmune(false);

	const u32t cid = c.corpseId;
	corpses_.push_back(std::move(c));
	return cid;
}

void Game::returnMonsterToPool(Corpse& corpse) {
	PooledMonster pm{ corpse.obj, corpse.hpBar };
	switch (corpse.kind) {
	case MonsterKind::Goblin:   goblinPool_.push_back(std::move(pm));   break;
	case MonsterKind::Snake:    snakePool_.push_back(std::move(pm));    break;
	case MonsterKind::Mushroom: mushroomPool_.push_back(std::move(pm)); break;
	case MonsterKind::Bomber:   bomberPool_.push_back(std::move(pm));   break;
	case MonsterKind::Birdy:    birdyPool_.push_back(std::move(pm));    break;
	case MonsterKind::Slime:    slimePool_.push_back(std::move(pm));    break;
	case MonsterKind::Treant:   treantPool_.push_back(std::move(pm));   break;
	case MonsterKind::Boss:     bossPool_.push_back(std::move(pm));     break;
	}
}

bool Game::reinitFromPool(MonsterKind kind, uint16 npcId, const mu::Vec3& pos, int32 hp) {
	// [EXPERIMENT] Force fresh-spawn for Bomber: never reuse a pooled bomber so respawns go
	// through createBomber instead of reinitFromPool. If the "vanish on death" bug disappears,
	// the cause is pooled-bomber reactivation. REVERT after the test.
	if (kind == MonsterKind::Bomber) return false;

	std::vector<PooledMonster>* pool = nullptr;
	switch (kind) {
	case MonsterKind::Goblin:   pool = &goblinPool_;   break;
	case MonsterKind::Snake:    pool = &snakePool_;    break;
	case MonsterKind::Mushroom: pool = &mushroomPool_; break;
	case MonsterKind::Bomber:   pool = &bomberPool_;   break;
	case MonsterKind::Birdy:    pool = &birdyPool_;    break;
	case MonsterKind::Slime:    pool = &slimePool_;    break;
	case MonsterKind::Treant:   pool = &treantPool_;   break;
	case MonsterKind::Boss:     pool = &bossPool_;     break;
	}
	if (!pool || pool->empty()) return false;

	PooledMonster pm = std::move(pool->back());
	pool->pop_back();
	const std::shared_ptr<Object> obj = pm.obj;

	obj->setId(npcId);
	obj->setPos(pos);                       // snaps prev = curr (stable interpolation base)
	obj->setHp(hp);
	obj->setMaxHp(hp);
	obj->setHidden(false);
	obj->setHiddenByOrb(false);
	obj->setDead(false);                    // reused corpse: clear death state up front
	obj->setHitImpulseImmune(false);        // 전술 면역 상태를 새 네트워크 객체가 물려받지 않게 한다.
	obj->body().setLinearVel(mu::Vec3{});   // drop any stale death-frame velocity
	obj->netInterpAcc_ = 0s;                // start network interpolation fresh
	// Keep the pooled object's renderObjectId (stable per object) — do NOT allocate a fresh
	// one, or the counter would climb every respawn and eventually exceed maxRenderObjectId.
	if (obj->ragdoll() && obj->ragdoll()->isActive())
		obj->ragdoll()->deactivate(physicsWorld_);
	obj->body().setMotionType(MotionType::Kinematic);

	UI::ProgressBar* bar = pm.hpBar;
	if (bar) bar->visible = false;

	switch (kind) {
	case MonsterKind::Goblin: {
		auto g = std::static_pointer_cast<Goblin>(obj);
		goblins_.push_back(g);
		idGoblinMap_[npcId] = g;
		if (bar) goblinHpBars_[npcId] = { g.get(), bar, 2.5f };
		break;
	}
	case MonsterKind::Snake: {
		auto s = std::static_pointer_cast<Snake>(obj);
		snakes_.push_back(s);
		idSnakeMap_[npcId] = s;
		if (bar) snakeHpBars_[npcId] = { s.get(), bar, 2.5f };
		break;
	}
	case MonsterKind::Mushroom: {
		auto m = std::static_pointer_cast<Mushroom>(obj);
		mushrooms_.push_back(m);
		idMushroomMap_[npcId] = m;
		if (bar) mushroomHpBars_[npcId] = { m.get(), bar, 2.5f };
		break;
	}
	case MonsterKind::Bomber: {
		auto b = std::static_pointer_cast<Bomber>(obj);
		bombers_.push_back(b);
		if (bar) bomberHpBars_[npcId] = { b.get(), bar, 2.5f };
		break;
	}
	case MonsterKind::Birdy: {
		auto b = std::static_pointer_cast<Birdy>(obj);
		birdys_.push_back(b);
		if (bar) birdyHpBars_[npcId] = { b.get(), bar, 2.5f };
		break;
	}
	case MonsterKind::Slime: {
		auto s = std::static_pointer_cast<Slime>(obj);
		slimes_.push_back(s);
		if (bar) slimeHpBars_[npcId] = { s.get(), bar, 2.5f };
		break;
	}
	case MonsterKind::Treant: {
		auto t = std::static_pointer_cast<Treant>(obj);
		treants_.push_back(t);
		if (bar) treantHpBars_[npcId] = { t.get(), bar, 2.5f };
		break;
	}
	case MonsterKind::Boss: {
		auto b = std::static_pointer_cast<Boss>(obj);
		bosses_.push_back(b);
		if (bar) bossHpBars_[npcId] = { b.get(), bar, 2.5f };
		break;
	}
	}
	idMonsterMap_[npcId] = obj.get();
	registerSkillObject(npcId, obj.get());
	respawnKind_[npcId] = kind;
	holdEvent(eventList_, EvRespawn(npcId));
	return true;
}

void Game::updateCorpses(Milliseconds deltaTime, float tPhysicInterp) {
	const float dtSec = std::chrono::duration<float>(deltaTime).count();
	// 래그돌 물리가 이 프로젝트의 시연 포인트라 유지 구간을 길게 잡는다. 늘린 만큼
	// 오브 구간(energyOrbSystem.cpp의 kFormingTime·추적 속도)을 줄여 총 흡수 시간을
	// 보존한다 — 시간 예산 표는 docs/gameArchitecture.md "연출 시간 예산".
	constexpr float kRagdollSeconds = 2.0f;   // hold the ragdoll before dissolving
	constexpr float kChargeWindow   = 0.5f;   // how long a charge credit waits for its corpse

	// Credit queued charges to the most-recent uncharged ragdoll corpse.
	for (auto cit = pendingOrbCharges_.begin(); cit != pendingOrbCharges_.end(); ) {
		cit->age += dtSec;
		Corpse* best = nullptr;
		for (auto& c : corpses_) {
			if (c.phase != Corpse::Phase::Ragdoll || c.orbsSpawned || c.totalCharge > 0.f) continue;
			if (!best || c.age < best->age) best = &c;
		}
		if (best) {
			best->totalCharge += cit->delta;
			best->slot = cit->slot;
			cit = pendingOrbCharges_.erase(cit);
		} else if (cit->age > kChargeWindow) {
			skillDial_.syncDisplayToTarget(cit->slot);  // no corpse arrived: fill HUD immediately
			cit = pendingOrbCharges_.erase(cit);
		} else {
			++cit;
		}
	}

	for (auto it = corpses_.begin(); it != corpses_.end(); ) {
		Corpse& c = *it;
		c.age += dtSec;
		Object& o = *c.obj;
		if (c.phase == Corpse::Phase::Ragdoll) {
			// Drive finalXforms from the ragdoll bodies BEFORE Object::update, so that
			// Object::update rebuilds renderState_.worldBVs (the debug BV boxes) from the
			// ragdoll pose — not the stale animation pose left by animSystem_.update().
			// (Otherwise the BV tracks the death animation while the mesh/physics flop.)
			if (o.ragdoll() && o.ragdoll()->isActive() && o.animBlender() && o.model())
				o.ragdoll()->syncToFinalXforms(
					o.animBlender()->finalXformData(), o.model()->skeleton, o.renderState().world,
					tPhysicInterp);
			o.update(deltaTime, tPhysicInterp);
			if (o.ragdoll() && o.ragdoll()->isActive())
				o.rebuildBodyBVH();
			if (c.age >= kRagdollSeconds && !c.orbsSpawned && o.animBlender() && o.model()) {
				const auto& fx = o.animBlender()->finalXformData();
				orbSystem_.spawnFromMonster(*o.model(),
					std::span<const mu::Mat4x4>(fx.data(), fx.size()),
					o.renderState().world, c.totalCharge, c.slot, c.corpseId);
				c.orbsSpawned = true;
				if (finalBossRewardCorpseTracked_
					&& c.corpseId == finalBossRewardCorpseId_) {
					finalBossRewardOrbsSpawned_ = true;
				}
				c.phase = Corpse::Phase::Orb;
				if (o.ragdoll() && o.ragdoll()->isActive())
					o.ragdoll()->deactivate(physicsWorld_);
			}
			++it;
		} else {  // Orb phase: keep the corpse alive until all its orbs are absorbed.
			if (!orbSystem_.hasActiveOrbs(c.corpseId)) {
				returnMonsterToPool(c);
				it = corpses_.erase(it);
			} else {
				++it;
			}
		}
	}
}

void Game::createStronghold(const ObjectInfo& info) {
	auto sh = std::make_shared<Stronghold>();   // placeholder mesh (cube); structure model TBD

	sh->setId(info.objectId);
	sh->setPos(DirectX::XMLoadFloat3(&info.pos));
	sh->setOrient(DirectX::XMLoadFloat4(&info.orient));
	sh->setScale(DirectX::XMLoadFloat3(&info.scale));
	sh->setModel(assetManager_.modelStronghold());
	sh->setFaction(Faction::Monsters);
	// HP/maxHp arrive in the enter packet (server-authoritative).
	sh->setHp(info.hp);
	sh->setMaxHp(info.maxHp);
	sh->setRenderObjectId(nextRenderObjId_++);
	sh->enableBVRendering();

	// Static collidable obstacle: register to the client physics world so the
	// locally-predicted player collides with it (server pos/scale already place
	// the box on the ground; cube model provides the collision BVH).
	sh->body().setMotionType(MotionType::Static);
	sh->body().snapToCurrent();
	physicsWorld_.registerBody(&sh->body(), [p = sh.get()]() { p->rebuildBodyBVH(); });

	auto* bar = static_cast<UI::ProgressBar*>(
		uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
	);
	bar->anchor    = UI::Anchors::TopLeft;
	bar->pivot     = UI::Pivots::TopLeft;
	bar->width     = UI::DimValue::px(120.f);
	bar->height    = UI::DimValue::px(10.f);
	bar->fillColor = { 0.9f, 0.7f, 0.1f, 1.f };
	bar->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
	bar->visible   = false;
	strongholdHpBars_[info.objectId] = { sh.get(), bar, 4.0f, 0.f };

	strongholds_.push_back(sh);
}

// ── 월드 드롭 보석 ───────────────────────────────────────────────────────────
// 권위 경계: 서버가 착지점을, 클라가 낙하 연출을 소유한다. 튀어 오른 보석은 로컬
// Dynamic 물리로 구르다가 권위 착지점으로 수렴한 뒤 물리에서 빠진다(비용 상한).
// 근거: RoomServer/docs/itemDropSystem.md
namespace {
constexpr float kGemTossSec        = 0.75f;   // 던지기 탄도 비행 시간(초기 속도 역산에 사용)
constexpr float kGemFlyMaxSec      = 2.0f;    // 이 시간이 지나면 무조건 수렴 단계로
constexpr float kGemSettleSec      = 0.3f;    // 권위 착지점으로 블렌딩하는 시간
constexpr float kGemMass           = 0.6f;
constexpr float kGemHalfExtent     = 0.12f;   // 관성 텐서용 근사 반치수
constexpr float kGemIdleSpinRadSec = 0.9f;    // Idle 회전 속도
constexpr float kGemBobAmplitude   = 0.05f;
constexpr float kGemBobRadSec      = 2.2f;
constexpr float kGemIdleLift       = 0.15f;   // 지면에 박혀 보이지 않도록 살짝 띄운다

// 카탈로그의 아이템 이름은 JSON에서 온 UTF-8이라 위젯에 넣기 전에 변환해야 한다.
// pch.hpp가 NONLS를 정의해 MultiByteToWideChar/CP_UTF8을 쓸 수 없으므로,
// lobbyUI.cpp / DialogueSystem.cpp와 같은 자체 디코더를 쓴다.
std::wstring gemUtf8ToWide(const std::string& bytes) {
	std::wstring out;
	out.reserve(bytes.size());
	const std::size_t n = bytes.size();
	std::size_t i = 0;
	while (i < n) {
		const unsigned char c = static_cast<unsigned char>(bytes[i]);
		char32_t cp = 0;
		int extra = 0;
		if (c < 0x80)              { cp = c;        extra = 0; }
		else if ((c >> 5) == 0x06) { cp = c & 0x1F; extra = 1; }
		else if ((c >> 4) == 0x0E) { cp = c & 0x0F; extra = 2; }
		else if ((c >> 3) == 0x1E) { cp = c & 0x07; extra = 3; }
		else                       { ++i; continue; }   // stray continuation/invalid lead

		if (i + static_cast<std::size_t>(extra) >= n) break;
		bool ok = true;
		for (int k = 1; k <= extra; ++k) {
			const unsigned char cc = static_cast<unsigned char>(bytes[i + k]);
			if ((cc >> 6) != 0x02) { ok = false; break; }
			cp = (cp << 6) | (cc & 0x3F);
		}
		if (!ok) { ++i; continue; }
		i += static_cast<std::size_t>(extra) + 1;

		if (cp <= 0xFFFF) {
			out.push_back(static_cast<wchar_t>(cp));
		} else {
			cp -= 0x10000;
			out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
			out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
		}
	}
	return out;
}
}

void Game::createItemDrop(const ItemDropInfo& info) {
	if (info.dropId == 0) return;
	// 중복 수신/재접속 대비: 같은 dropId가 이미 있으면 무시한다.
	if (gemDrops_.contains(info.dropId)) return;

	const Model* model = assetManager_.gemModel(info.itemId, info.visualVariant);
	if (!model) return;

	const mu::Vec3 landing = mu::Vec3(info.pos.x, info.pos.y, info.pos.z);

	// 던지기 시작점: 시체가 아직 살아 있으면 그 위치, 아니면 착지점 위.
	mu::Vec3 origin = landing + mu::Vec3(0.f, 1.5f, 0.f);
	if (const size_t sid = static_cast<size_t>(info.sourceObjId);
		sid != 0 && sid < skillObjectById_.size() && skillObjectById_[sid]) {
		origin = skillObjectById_[sid]->pos() + mu::Vec3(0.f, 1.0f, 0.f);
	}

	auto obj = std::make_shared<Object>();
	obj->setPos(origin);
	obj->setModel(model);
	obj->setRenderObjectId(nextRenderObjId_++);

	RigidBody& body = obj->body();
	body.setMotionType(MotionType::Dynamic);
	body.setMass(kGemMass);
	body.setInertia(computeBoxInertia(
		kGemMass, mu::Vec3(kGemHalfExtent, kGemHalfExtent, kGemHalfExtent)));
	body.setLinearDamping(0.25f);
	body.setAngularDamping(0.5f);
	body.setRestitution(0.35f);
	body.setFriction(0.8f);

	// 탄도해가 착지점에서 끝나도록 초기 속도를 역산한다. 접촉/감쇠 때문에 정확히
	// 떨어지지는 않지만, 뒤이은 Settling 단계가 권위 착지점으로 수렴시킨다.
	const mu::Vec3 g = physicsWorld_.gravity();
	const mu::Vec3 v0 = (landing - origin) * (1.f / kGemTossSec) - g * (0.5f * kGemTossSec);
	body.setLinearVel(v0);
	body.setOmega(mu::Vec3(
		rand(-6.f, 6.f), rand(-6.f, 6.f), rand(-6.f, 6.f)));
	body.snapToCurrent();

	Object* raw = obj.get();
	physicsWorld_.registerBody(&body, [raw]() { raw->rebuildBodyBVH(); });

	GemDrop drop;
	drop.obj               = std::move(obj);
	drop.itemId            = info.itemId;
	drop.phase             = GemDrop::Phase::Flying;
	drop.inPhysics         = true;
	drop.authoritativePos  = landing;
	gemDrops_.emplace(info.dropId, std::move(drop));
}

void Game::destroyItemDrop(uint16 dropId) {
	const auto it = gemDrops_.find(dropId);
	if (it == gemDrops_.end()) return;   // 중복 제거 통지는 무해

	if (it->second.inPhysics && it->second.obj)
		physicsWorld_.unregisterBody(&it->second.obj->body());
	gemDrops_.erase(it);

	if (aimedDropId_ == dropId)     aimedDropId_ = 0;
	if (pickupPendingId_ == dropId) pickupPendingId_ = 0;
}

void Game::onItemDropRemoved(uint16 dropId, uint16 /*pickerObjId*/, ItemPickupResult result) {
	if (result == ItemPickupResult::PickedUp || result == ItemPickupResult::Expired) {
		destroyItemDrop(dropId);
		return;
	}

	// 습득 거절: 드롭은 월드에 남는다. 대기 상태만 풀고 사유를 안내한다.
	if (pickupPendingId_ == dropId) pickupPendingId_ = 0;
	switch (result) {
	case ItemPickupResult::InventoryFull:
		pickupNotice_ = L"인벤토리가 가득 찼습니다"; break;
	case ItemPickupResult::TooFar:
		pickupNotice_ = L"너무 멀리 있습니다"; break;
	case ItemPickupResult::NotFound:
		pickupNotice_ = L"이미 다른 사람이 주웠습니다"; break;
	default:
		pickupNotice_ = L"지금은 주울 수 없습니다"; break;
	}
	pickupNoticeSec_ = 2.0f;
}

void Game::updateItemDrops(Milliseconds deltaTime, float tPhysicInterpolation) {
	const float dt = std::chrono::duration_cast<Seconds>(deltaTime).count();

	if (pickupNoticeSec_ > 0.f) pickupNoticeSec_ = std::max(0.f, pickupNoticeSec_ - dt);
	// 응답이 유실돼도 영구히 잠기지 않도록 대기 상태를 만료시킨다.
	if (pickupPendingId_ != 0) {
		pickupPendingSec_ += dt;
		if (pickupPendingSec_ > 1.0f) { pickupPendingId_ = 0; pickupPendingSec_ = 0.f; }
	}

	for (auto& [dropId, drop] : gemDrops_) {
		if (!drop.obj) continue;
		drop.phaseSec += dt;

		switch (drop.phase) {
		case GemDrop::Phase::Flying: {
			// 지형 청크가 아직 로드되지 않았거나 보석 메시에 BVH가 없으면 접촉이 생기지
			// 않아 무한 낙하한다. 권위 착지점 아래로는 내려가지 못하게 잘라둔다.
			if (const mu::Vec3 p = drop.obj->pos(); p.y() < drop.authoritativePos.y()) {
				drop.obj->setCurrPos(mu::Vec3(p.x(), drop.authoritativePos.y(), p.z()));
				const mu::Vec3 v = drop.obj->body().linearVel();
				if (v.y() < 0.f)
					drop.obj->body().setLinearVel(mu::Vec3(v.x(), 0.f, v.z()));
			}

			// 멈췄거나(접지 후 정지) 시간이 다 되면 수렴 단계로 넘어간다.
			const mu::Vec3 v = drop.obj->body().linearVel();
			const bool settled = (v.x() * v.x() + v.y() * v.y() + v.z() * v.z()) < 0.04f
				&& drop.phaseSec > 0.5f;
			if (!settled && drop.phaseSec < kGemFlyMaxSec)
				break;

			if (drop.inPhysics) {
				physicsWorld_.unregisterBody(&drop.obj->body());
				drop.inPhysics = false;
			}
			drop.settleFrom = drop.obj->pos();
			drop.phase      = GemDrop::Phase::Settling;
			drop.phaseSec   = 0.f;
			break;
		}
		case GemDrop::Phase::Settling: {
			const float t = std::min(1.f, drop.phaseSec / kGemSettleSec);
			const float s = t * t * (3.f - 2.f * t);   // smoothstep
			const mu::Vec3 target = drop.authoritativePos + mu::Vec3(0.f, kGemIdleLift, 0.f);
			drop.obj->setPos(drop.settleFrom + (target - drop.settleFrom) * s);
			if (t >= 1.f) {
				drop.phase    = GemDrop::Phase::Idle;
				drop.phaseSec = 0.f;
			}
			break;
		}
		case GemDrop::Phase::Idle: {
			drop.idleSpinRad += kGemIdleSpinRadSec * dt;
			drop.bobPhase    += kGemBobRadSec * dt;
			drop.obj->setOrient(mu::NQuat(mu::Vec3(0.f, 1.f, 0.f), drop.idleSpinRad));
			drop.obj->setPos(drop.authoritativePos + mu::Vec3(
				0.f, kGemIdleLift + kGemBobAmplitude * std::sin(drop.bobPhase), 0.f));
			break;
		}
		}

		drop.obj->update(deltaTime, tPhysicInterpolation);
	}

	updateItemDropAim();
}

void Game::updateItemDropAim() {
	aimedDropId_ = 0;
	if (gemDrops_.empty() || !player_) return;

	// 서버 Room::pickupItem의 kPickupRadius와 반드시 같아야 한다. 다르면 프롬프트가
	// 뜨는데 서버는 TooFar로 거절하는(혹은 그 반대의) 거짓말 UX가 된다.
	constexpr float kPickupRadius   = 2.5f;
	constexpr float kPickupVertical = 3.0f;
	constexpr float kScreenFallbackPx = 60.f;

	const float sw = uiManager_.screenWidth();
	const float sh = uiManager_.screenHeight();
	const mu::Mat4x4 view = camera_.view();
	const mu::Mat4x4 proj = camera_.proj();

	Ray ray{};
	screenToRay(sw * 0.5f, sh * 0.5f, sw, sh, view, proj, ray.origin, ray.dir);

	const mu::Vec3 playerPos = player_->pos();
	float bestRayT   = std::numeric_limits<float>::max();
	uint16 bestRayId = 0;
	float bestPx     = kScreenFallbackPx;
	uint16 bestPxId  = 0;

	for (const auto& [dropId, drop] : gemDrops_) {
		if (!drop.obj) continue;

		// 습득 가능 거리 밖이면 조준 대상에서 제외 — 프롬프트가 서버 판정과 일치한다.
		const mu::Vec3 d = playerPos - drop.obj->pos();
		if (std::abs(d.y()) > kPickupVertical) continue;
		if ((d.x() * d.x() + d.z() * d.z()) > kPickupRadius * kPickupRadius) continue;

		const RayHit hit = RaycastBVH(drop.obj->worldBVH(), ray);
		if (hit.hit && hit.t < bestRayT) {
			bestRayT  = hit.t;
			bestRayId = dropId;
			continue;
		}

		// 폴백: 보석 메시가 작아 화면 중앙 레이를 자주 놓친다. 에디터 피킹
		// (editorController::pickHitbox)과 같은 화면 거리 근접 판정을 함께 쓴다.
		float sx = 0.f, sy = 0.f;
		if (!worldToScreen(drop.obj->pos(), view, proj, sw, sh, sx, sy)) continue;
		const float dx = sx - sw * 0.5f;
		const float dy = sy - sh * 0.5f;
		const float px = std::sqrt(dx * dx + dy * dy);
		if (px < bestPx) { bestPx = px; bestPxId = dropId; }
	}

	aimedDropId_ = bestRayId != 0 ? bestRayId : bestPxId;
}

void Game::onStrongholdState( uint16 strongholdId, int32 hp, uint8 state ) {
	auto it = strongholdHpBars_.find( strongholdId );
	if ( it == strongholdHpBars_.end() || !it->second.obj ) return;

	// S_StrongholdState는 파괴/재건 "상태 전이"에서만 온다(전투 중 HP 감소는 S_SkillHit/S_Hit
	// 가 담당). 파괴/재건 전이는 EventBus 경로(EvDeath/EvRespawn)로 보내 isDead_를 단일화한다.
	Stronghold* sh = it->second.obj;
	if ( hp > sh->maxHp() ) sh->setMaxHp( hp );   // safety clamp (maxHp now arrives at enter)

	if ( state == 1 ) {
		// 파괴: HP→0은 EvDeath 핸들러가 소유한다. 여기서 setHp(0)을 하면 막타 데미지 넘버가
		// 같은 프레임 디스패치 전에 prevHp를 잃어 0으로 계산되므로 setHp를 호출하지 않는다.
		holdEvent( eventList_, EvDeath( strongholdId ) );    // idempotent via handler guard
	} else {
		// 재건/생존: 서버가 복구된 full HP를 함께 보낸다.
		sh->setHp( hp );
		holdEvent( eventList_, EvRespawn( strongholdId ) );
	}
}

// 연출 존 핸들러 바인딩. Unity에서 오서링한 태그를 키로 로컬 동작(BGM/카메라/포스트FX 등)을
// 연결한다. 게임플레이 태그(예: "boss_arena_1")는 여기서 미바인딩 → 로컬 판정 시 no-op.
void Game::bindZoneHandlers() {
	struct ArenaPresentation {
		std::string_view tag;
		std::string_view wallPrefix;
		std::string_view bgm;
	};
	static constexpr ArenaPresentation kArenas[] = {
		{ "Arena_Hobgoblin", "WallHobgoblin", "hobgoblin_arena" },
		{ "Arena_Grandbaum",  "WallGrandbaum",  "grandbaum_arena" },
		{ "Arena_Isys",       "WallIsys",       "isys_arena" },
		{ "Arena_Boss",       "WallBoss",       "boss_arena" },
	};

	for (const ArenaPresentation& arena : kArenas) {
		clientZoneSystem_.on(std::string(arena.tag), ZoneEvent::Enter,
			[this, wallPrefix = arena.wallPrefix, bgm = arena.bgm](Zone& zone) {
				// ZoneSystem is updated only with player_->pos(), so this callback
				// belongs exclusively to the player controlled by this client.
				if (completedArenaZoneIds_.contains(zone.id())) {
					return;
				}
				if (!localPresentedArenaZoneIds_.insert(zone.id()).second) {
					return;
				}
				localArenaPresentationZoneId_ = zone.id();
				tacticalZoneIntro_.trigger(wallPrefix);
				if (wallPrefix == "WallBoss") {
					showBossHpHud();
				}

				constexpr float kArenaBgmFadeOutMs = 1100.f;
				constexpr float kArenaBgmFadeInMs = 3400.f;
				constexpr float kArenaBgmFadeInDelayMs = 800.f;
				INet::ClientApp::sound().playBgm(bgm,
					kArenaBgmFadeOutMs,
					kArenaBgmFadeInMs,
					kArenaBgmFadeInDelayMs);
			});
	}
}

// 모든 "Arena_*" zone을 독립적으로 순회해 각자의 Wall 마커로 마법진 quad를 재구성한다. 색(tint)은
// 여기서 정하지 않는다 — 로컬 플레이어 위치에 따라 매 프레임 renderBarrierMagicCircleQuads에서 결정한다.
// 한 플레이어가 진입(state==1)해도 아직 밖에 있는 플레이어는 일방향 벽으로 입장 가능하므로 파란색을 봐야
// 하기 때문. 여기서는 그 판정에 필요한 벽 평면(center/outward, makeOneWayWall과 동일)만 캐시한다.
void Game::rebuildBarrierMagicCircleQuads() {
	barrierMagicCircleQuads_.clear();

	for (const auto& z : chunkManager_.zones()) {
		const std::string prefix = arenaWallPrefix(z);
		if (prefix.empty()) continue;

		// interior 기준점 = 이 아레나 Wall 마커들의 중점(arenaWalls_ 빌드와 동일 규칙). outward 부호가
		// 이 기준점으로 정해지므로, 마법진 색 판정도 충돌 벽과 정확히 같은 안/바깥을 쓴다.
		mu::Vec3 wallSum{};
		int wallCount = 0;
		for (const auto& m : chunkManager_.markers()) {
			if (m.type != "Wall" || m.name.rfind(prefix, 0) != 0) continue;
			wallSum += m.pos;
			++wallCount;
		}
		if (wallCount == 0) continue;
		const mu::Vec3 mid = wallSum / static_cast<float>(wallCount);

		for (const auto& m : chunkManager_.markers()) {
			if (m.type != "Wall" || m.name.rfind(prefix, 0) != 0) continue;

			const float diameter = barrierMagicDiameter(m);
			const BarrierMagicAdjust adj = barrierMagicAdjust(m);
			const mu::Vec3 circlePos = m.pos + m.orient.rotate(adj.offset);
			const OneWayWall w = makeOneWayWall(m, mid);

			BarrierMagicCircleQuad quad{};
			quad.world = mu::scaleH(mu::Vec3{ diameter, diameter, 1.f })
			           * mu::translate(circlePos);
			// 추가 회전은 planeFix와 markerOrient 사이에 끼워 마커 로컬축 기준으로 적용(rotateDeg=0이면 항등).
			const mu::Mat4x4 extraRot = mu::rotateXH(mu::Degree(adj.rotateDeg.x()))
			                          * mu::rotateYH(mu::Degree(adj.rotateDeg.y()))
			                          * mu::rotateZH(mu::Degree(adj.rotateDeg.z()));
			quad.rotation = kBarrierMagicQuadPlaneFix * extraRot * mu::Mat4x4(m.orient);
			quad.sortPos = circlePos;
			quad.zoneId = static_cast<uint16>(z.id);
			quad.wallCenter = w.center;
			quad.wallOutward = w.outward;
			barrierMagicCircleQuads_.push_back(quad);
		}
	}
}

void Game::renderBarrierMagicCircleQuads() {
	const Texture* tex = assetManager_.magicCircleTex();
	if (!tex) return;

	// 색은 (1) 서버 권위 zone 상태와 (2) 로컬 플레이어가 그 벽을 통과할 수 있는지로 결정한다.
	// state==1(아레나 활성)이라도 로컬 플레이어가 벽 바깥(아직 입장 가능)이면 파란색 — 한 명이
	// 진입해도 다른 플레이어 화면에선 통과 가능 색으로 보인다(일방향 벽이 안쪽으로는 통과 허용).
	const bool havePlayer = (player_ != nullptr);
	const mu::Vec3 playerPos = havePlayer ? player_->pos() : mu::Vec3{};

	for (const auto& quad : barrierMagicCircleQuads_) {
		uint8 state = 0;   // 미진입 zone은 기본 통과 가능(파란색)
		if (auto it = zoneStates_.find(quad.zoneId); it != zoneStates_.end())
			state = it->second;

		bool blocked = false;
		if (state == 1 && havePlayer) {
			// outward 기준 부호거리. 안쪽(<= eps)이면 이 벽을 넘어 나갈 수 없음 → 빨강.
			const float side = (playerPos.x() - quad.wallCenter.x()) * quad.wallOutward.x()
			                 + (playerPos.z() - quad.wallCenter.z()) * quad.wallOutward.z();
			blocked = (side <= kBarrierMagicBlockedSideEps);
		}

		gfx_.addDrawEvent(BillboardPipeline::DrawEvent{
			.world = quad.world,
			.pTex = tex,
			.tint = blocked ? kBarrierMagicBlockedColor : kBarrierMagicPassableColor,
			.blend = ps::BlendMode::Alpha,
			.rotation3D = quad.rotation,
			.alignment = ps::RendererModule::Alignment::Local,
			.renderOrder = kBarrierMagicRenderOrder,
			.sortMode = ps::RendererModule::SortMode::Distance,
			.sortingFudge = -0.1f,
			.sortPos = quad.sortPos,
		});
	}
}

// 서버 권위 존 상태 동기화(S_ZoneState). state==1이면 아레나 진입으로 간주해 양끝 후방 Wall
// 일방향 슬랩을 빌드한다(zone tag로 Wall prefix 도출, 두 벽 중점이 interior 기준점). state==0이면
// 해제. 물리 벽은 만들지 않는다 — 후퇴 차단은 resolveArenaWallLeash의 위치 클램프가 담당한다.
void Game::onZoneState( uint16 zoneId, uint8 state ) {
	std::string prefix;
	for ( const auto& z : chunkManager_.zones() ) {
		if ( z.id != static_cast<int>( zoneId ) ) continue;
		prefix = arenaWallPrefix( z );
		break;
	}

	const auto previousIt = zoneStates_.find(zoneId);
	const uint8 previousState = previousIt != zoneStates_.end() ? previousIt->second : 0;
	zoneStates_[zoneId] = state;
	std::cout << "[Zone] onZoneState zoneId=" << zoneId << " state=" << (int)state << '\n';

	// S_ZoneState is shared gameplay state and must not start client presentation.
	// A completed arena stays presentation-locked. Only a later genuine 0 -> 1
	// transition rearms it for a new encounter; merely revisiting its trigger
	// after state==0 must not replay the intro or arena BGM.
	const int localZoneId = static_cast<int>(zoneId);
	if (previousState == 0 && state == 1
		&& completedArenaZoneIds_.erase(localZoneId) > 0) {
		localPresentedArenaZoneIds_.erase(localZoneId);
	}

	if (previousState == 1 && state == 0) {
		const bool firstClear = completedArenaZoneIds_.insert(localZoneId).second;
		if (prefix == "WallBoss" && bossHpHudActive_) {
			hideBossHpHud();
		}

		if (localArenaPresentationZoneId_ == localZoneId) {
			constexpr float kArenaBgmFadeOutMs = 1100.f;
			constexpr float kArenaBgmFadeInMs = 3400.f;
			constexpr float kArenaBgmFadeInDelayMs = 800.f;
			INet::ClientApp::sound().playBgm("ingame",
				kArenaBgmFadeOutMs,
				kArenaBgmFadeInMs,
				kArenaBgmFadeInDelayMs);
			localArenaPresentationZoneId_ = -1;
		}

		// First clear of the Hobgoblin tactical zone (server-authoritative wall
		// removal, 1 -> 0): show the follow-up monologue once. firstClear comes
		// from completedArenaZoneIds_, kept in sync on every client via
		// S_ZoneState, so each client shows it exactly once per clear cycle.
		if (firstClear && prefix == "WallHobgoblin") {
			if (dialogueSystem_.show("sample_context")) {
				applyCursorPolicy();
			}
		}
	}

	rebuildBarrierMagicCircleQuads();   // 장식용 마법진(전 아레나, 마커 기반, 충돌과 무관)

	arenaWalls_.clear();
	if ( state == 1 ) {
		// zone id로 tag를 찾아 Wall prefix("Wall"+<Arena_ 뒷부분>)를 도출 → 해당 Wall 마커들로
		// 양끝 일방향 슬랩을 빌드. interior 기준점 = 그 마커들의 중점(두 벽 사이).
		if ( !prefix.empty() ) {
			std::vector<const MarkerDef*> wallMarkers;
			mu::Vec3 wallSum{};
			for ( const auto& m : chunkManager_.markers() ) {
				if ( m.type != "Wall" || m.name.rfind( prefix, 0 ) != 0 ) continue;
				wallMarkers.push_back( &m );
				wallSum += m.pos;
			}
			if ( !wallMarkers.empty() ) {
				const mu::Vec3 mid = wallSum / static_cast<float>( wallMarkers.size() );
				for ( const MarkerDef* wm : wallMarkers ) {
					OneWayWall w = makeOneWayWall( *wm, mid );
					arenaWalls_.push_back( w );
					std::cout << "[Zone] local one-way wall '" << wm->name << "' outward=("
					          << w.outward.x() << ", " << w.outward.z() << ") halfWidth=" << w.halfWidth << '\n';
				}
			}
		}
		arenaLeashActive_ = !arenaWalls_.empty();
		if ( player_ ) arenaPrevPlayerPos_ = player_->pos();
		if ( !arenaLeashActive_ )
			std::cout << "[Zone] WARN: zone " << zoneId << " produced no arena walls\n";
	}
	else {
		arenaLeashActive_ = false;
	}
}

void Game::onTacticalDialogue(uint16 zoneId, TacticalDialogueId dialogueId) {
	// Recipient selection is server-authoritative. Do not reject a valid targeted
	// event because this client's shared zone-state cache is a frame behind.
	(void)zoneId;
	tacticalDialogueOverlay_.trigger(dialogueId);
}

// 아레나 후방 Wall 일방향 벽: 직전 프레임 위치 대비, 양끝 Wall을 바깥으로 통과하려는 로컬
// 플레이어만 평면으로 되돌린다(XZ만, Y 보존). 안쪽 입장·측면 이동은 통과 → 후발 파티원이 벽에
// 막히지 않는다. setCurrPos만 보정 → 임펄스 튕김 없음, 결과는 C_Move로 전파. 프레임당 1회.
void Game::resolveArenaWallLeash() {
	if ( !arenaLeashActive_ || !player_ || playerDead_ ) {
		if ( player_ ) arenaPrevPlayerPos_ = player_->pos();   // 비활성 중에도 prev는 추적해 둔다
		return;
	}

	const mu::Vec3 cur = player_->pos();
	mu::Vec3 np = cur;
	for ( const OneWayWall& w : arenaWalls_ )
		np = clampOneWayWall( arenaPrevPlayerPos_, np, w, kPlayerSeparationRadius );

	if ( np.x() != cur.x() || np.z() != cur.z() ) {
		player_->setCurrPos( np );   // XZ만 보정(clampOneWayWall이 Y 보존)
		moveChange_ = true;
	}
	arenaPrevPlayerPos_ = player_->pos();
}

void Game::removePlayer( i32t playerId ) {
	auto itPlayer = std::ranges::find_if(
		otherPlayers_, [ playerId ]( const std::shared_ptr<Player>& obj ) {
			return obj->getId( ) == playerId;
		}
	);

	DISPLAY_ERROR_STR( itPlayer != otherPlayers_.end(),
		"[Game Error] Game::removePlayer: 제거하려는 플레이어가 존재하지 않습니다.\n",
		false
	);

	if (itPlayer == otherPlayers_.end()) {
		return;
	}

	// 아래 해체는 게임 도중 GPU 참조 객체를 즉시 파괴한다 — 특히 파티 HUD의
	// Label은 전용 TextImage(ID3D12 텍스처)를 소유하는데, D3D12는 Release 즉시
	// 메모리를 회수하므로 in-flight 프레임이 그 텍스처를 참조하고 있으면 디바이스
	// 폴트 → TDR로 같은 GPU의 모든 클라이언트가 멈춘다(DEVICE_HUNG). 퇴장은 드문
	// 이벤트이므로 파괴 전에 GPU를 드레인한다(수 프레임 대기).
	gfx_.drainGpu();

	animSystem_.untrackAnimBlender(itPlayer->get()->renderState().animBlender.get());
	physicsWorld_.unregisterBody(&(*itPlayer)->body());

	// Stop any skills this player owns and drop the skill-system reference before
	// the Object is destroyed, so checkHitboxCollisions never dereferences a
	// dangling pointer through skillObjectById_.
	// This runs inside the frame-start APC where skillCtx_ still holds last
	// frame's pointers; an earlier packet in the same batch may have reallocated
	// skillObjectById_, so re-sync before dispatching into the skill system.
	refreshSkillCtx();
	skillSystem_.interruptAll(static_cast<i32t>(playerId), skillCtx_);
	unregisterSkillObject(playerId);

	if (auto it = otherPlayerHpBars_.find(playerId); it != otherPlayerHpBars_.end()) {
		uiManager_.root()->removeChild(it->second.hpBar);
		if (it->second.partyRoot) {
			uiManager_.root()->removeChild(it->second.partyRoot);
		}
		otherPlayerHpBars_.erase(it);
	}

	unregisterInGamePartyPlayer(static_cast<uint16>(playerId));
	otherPlayers_.erase(itPlayer);
	idPlayerMap_.erase( playerId );

	// Re-pack the remaining party HUD rows so the removed row leaves no gap.
	updatePartyHpHudLayout();
}

// 플레이어 간 reciprocal soft separation (클라 예측).
//
// 각 클라는 자기 플레이어만 소유한다(위치 권위). 두 플레이어가 겹치면 이 클라는
// 로컬 플레이어를 침투량의 "절반"만큼만 밀어낸다. 상대의 절반은 상대 클라가 동일
// 규칙으로 처리하고, 그 결과는 기존 C_Move/S_Move 동기화로 전파된다.
//   - 절반인 이유: 양쪽이 전부 밀면 합이 2×침투 → 과분리·진동. 각자 절반이면 합이
//     정확히 침투량이라 매끄럽게 0으로 수렴한다(RVO/ORCA reciprocity).
//   - 상대(B)가 밀려나야 함을 아는 방법: B의 클라도 매 step 이 함수를 돌려, 아직 남은
//     겹침을 직접 감지해 자기 절반을 처리한다(입력 없어도 실행). 명령 수신이 아님.
// 보고 정책: 분리 변위는 위치(setCurrPos)로만 운반하고 velocity에는 주입하지 않는다
// (원격 dead-reckoning·애니메이션 블렌딩 안정성 유지).
void Game::resolvePlayerSeparation(Seconds dt) {
	if (!player_ || player_->faction() != Faction::Players) return;
	if (playerDead_) return;

	const float maxStep = kMaxSeparationSpeed * dt.count();
	const float sumR    = 2.f * kPlayerSeparationRadius;
	const float sumR2   = sumR * sumR;

	const mu::Vec3 myPos = player_->pos();
	mu::Vec3 accumXZ{ 0.f, 0.f, 0.f };  // 이번 step의 누적 분리 보정 (XZ, Y는 항상 0)

	for (const auto& p : otherPlayers_) {
		if (!p || p->faction() != Faction::Players) continue;
		if (p->hp() <= 0) continue;  // 사망/래그돌 상태는 분리 대상에서 제외

		const mu::Vec3 oPos = p->pos();
		const float dx = myPos.x() - oPos.x();
		const float dz = myPos.z() - oPos.z();
		const float dist2 = dx * dx + dz * dz;
		if (dist2 >= sumR2) continue;  // 수평으로 겹치지 않음

		float dist = std::sqrt(dist2);
		float dirX, dirZ;
		if (dist > 1e-4f) {
			dirX = dx / dist;
			dirZ = dz / dist;
		} else {
			// 완전 겹침: 전역 고유 id로 결정론적 방향 부여 → 양쪽 클라가 반대 방향으로
			// 분리되어 결과가 일관된다.
			const float s = (player_->getId() < p->getId()) ? 1.f : -1.f;
			dirX = s; dirZ = 0.f;
			dist = 0.f;
		}

		const float penetration = sumR - dist;
		// 절반 책임 + stiffness, 분리 속도 상한으로 클램프(soft·순간이동 방지).
		const float corr = std::min(0.5f * penetration * kSeparationStiffness, maxStep);
		accumXZ = accumXZ + mu::Vec3(dirX * corr, 0.f, dirZ * corr);
	}

	if (accumXZ.len2() > 1e-10f) {
		player_->setCurrPos(myPos + accumXZ);  // Y 불변 (accumXZ.y == 0)
		// 정지 상태(velocity≈0)에서 밀려나도 분리된 위치가 다른 클라에 전파되도록
		// move 패킷 전송 플래그를 켠다. (그렇지 않으면 moveChange_가 false로 남는다)
		moveChange_ = true;
	}
}

// 서버가 S_NpcBarrier로 차단벽 토글 → 대상 NPC의 barrier 플래그 갱신 + 활성 목록 관리.
void Game::setNpcBarrier(bool active, const std::vector<uint16>& npcIds, uint16 impulseOnlyNpcId) {
	for (uint16 id : npcIds) {
		// 전 몬스터(슬라임 포함) id→Object* 맵에서 조회. idGoblinMap_는 goblin/hobgoblin 전용이라
		// 슬라임이 빠져 그랜드밤 ShieldWall barrier가 아예 등록 안 되던 버그를 수정한다(barrier엔 Object*면 충분).
		auto it = idMonsterMap_.find(id);
		if (it == idMonsterMap_.end() || !it->second) continue;

		Object* obj = it->second;
		obj->setHitImpulseImmune(active);
		if (obj->isBarrierActive() == active) continue;  // 이미 같은 상태면 스킵

		obj->setBarrierActive(active);
		if (active) {
			barrierObjects_.push_back(obj);
		} else {
			barrierObjects_.erase(
				std::remove(barrierObjects_.begin(), barrierObjects_.end(), obj),
				barrierObjects_.end());
		}
	}

	// 보스는 실제 barrier 목록에는 넣지 않고 로컬 스킬 impulse만 차단한다.
	if (impulseOnlyNpcId != SNpcBarrierPacket::INVALID_NPC_ID) {
		auto it = idMonsterMap_.find(impulseOnlyNpcId);
		if (it != idMonsterMap_.end() && it->second) {
			it->second->setHitImpulseImmune(active);
		}
	}
}

void Game::hideNpcs(const std::vector<uint16>& npcIds) {
	for (uint16 id : npcIds) {
		auto it = idGoblinMap_.find(id);
		if (it == idGoblinMap_.end() || !it->second) continue;

		auto& goblin = it->second;
		goblin->setHidden(true);
		if (goblin->ragdoll()->isActive())
			goblin->ragdoll()->deactivate(physicsWorld_);
	}
}

// 점 p에서 선분 [a,b]까지의 XZ 평면 최근접점(Y=0). 표준 사영 + [0,1] 클램프.
static mu::Vec3 closestPointOnSegmentXZ(mu::Vec3 p, mu::Vec3 a, mu::Vec3 b) {
	const float abx = b.x() - a.x();
	const float abz = b.z() - a.z();
	const float ab2 = abx * abx + abz * abz;
	float t = 0.f;
	if (ab2 > 1e-8f) {
		t = ((p.x() - a.x()) * abx + (p.z() - a.z()) * abz) / ab2;
		t = std::clamp(t, 0.f, 1.f);
	}
	return mu::Vec3(a.x() + abx * t, 0.f, a.z() + abz * t);
}

// 차단벽 분리: barrier NPC들을 인접끼리 선분(캡슐)으로 이어 "연속 벽"으로 처리하고, 그 안에 들어온
// 로컬 플레이어를 전체 침투량만큼 밖으로 민다. 점(원) 방식과 달리 NPC 간격이 얼마든 틈이 봉합된다.
//  - barrier는 움직이지 않는 서버 권위 객체 → 절반이 아닌 "전체" 침투를 플레이어가 해소(hard wall).
//  - 위치(setCurrPos)만 보정 → 임펄스/속도 주입 없음 → 튕김 없음.
//  - 죽은 벽 NPC(hp≤0)는 수집에서 제외 → 연결이 끊겨 그 구간에 구멍이 난다(탈출 기믹 유지).
void Game::resolveBarrierSeparation(Seconds dt) {
	if (!player_ || playerDead_) return;
	if (barrierObjects_.empty()) return;

	// 살아있는 barrier의 XZ 위치 수집.
	std::vector<mu::Vec3> alive;
	alive.reserve(barrierObjects_.size());
	for (Object* b : barrierObjects_) {
		if (b && b->hp() > 0) alive.push_back(b->pos());
	}
	if (alive.empty()) return;

	const float sumR      = kPlayerSeparationRadius + kBarrierRadius;
	const float sumR2     = sumR * sumR;
	const float linkDist2 = kBarrierLinkDist * kBarrierLinkDist;

	const mu::Vec3 myPos = player_->pos();
	mu::Vec3 accumXZ{ 0.f, 0.f, 0.f };

	const int n = static_cast<int>(alive.size());

	// 플레이어를 (캡슐/원) 표면 밖으로 미는 보정을 누적하는 람다.
	// cp = 벽 요소(선분/점) 위 최근접점, fallbackDir = 거의-겹침 시 밀 방향.
	auto pushOut = [&](mu::Vec3 cp, float fbDirX, float fbDirZ) {
		const float dx = myPos.x() - cp.x();
		const float dz = myPos.z() - cp.z();
		const float dist2 = dx * dx + dz * dz;
		if (dist2 >= sumR2) return;  // 밖

		float dist = std::sqrt(dist2);
		float dirX, dirZ;
		if (dist > 1e-4f) { dirX = dx / dist; dirZ = dz / dist; }
		else              { dirX = fbDirX;    dirZ = fbDirZ; dist = 0.f; }

		const float penetration = sumR - dist;
		accumXZ = accumXZ + mu::Vec3(dirX * penetration, 0.f, dirZ * penetration);
	};

	for (int i = 0; i < n; ++i) {
		const mu::Vec3& pi = alive[i];

		// linkDist 내 모든 이웃과 선분 캡슐을 형성(최근접만 잇던 종전 방식은 가장 넓은 틈을 건너뛰었음).
		// j>i로 각 쌍을 한 번만. 한 쌍이라도 이으면 isolated 아님.
		bool linked = false;
		for (int j = i + 1; j < n; ++j) {
			const float ex = alive[j].x() - pi.x();
			const float ez = alive[j].z() - pi.z();
			const float d2 = ex * ex + ez * ez;
			if (d2 >= linkDist2) continue;
			linked = true;

			// 거의-겹침 폴백: 선분 수직 방향.
			float fbX = 1.f, fbZ = 0.f;
			const float sl = std::sqrt(d2);
			if (sl > 1e-4f) { fbX = -ez / sl; fbZ = ex / sl; }
			pushOut(closestPointOnSegmentXZ(myPos, pi, alive[j]), fbX, fbZ);
		}

		// linkDist 내 이웃이 (양방향 모두) 없으면 고립 barrier → 원으로 단독 차단.
		if (!linked) {
			bool hasNeighbor = false;
			for (int j = 0; j < n && !hasNeighbor; ++j) {
				if (j == i) continue;
				const float ex = alive[j].x() - pi.x();
				const float ez = alive[j].z() - pi.z();
				if (ex * ex + ez * ez < linkDist2) hasNeighbor = true;
			}
			if (!hasNeighbor) {
				pushOut(mu::Vec3(pi.x(), 0.f, pi.z()), 1.f, 0.f);
			}
		}
	}

	// 누적 보정 크기를 상한으로 클램프(인접쌍 중복 합산·다중 선분으로 인한 과보정/순간이동 방지).
	const float mag2 = accumXZ.len2();
	if (mag2 > 1e-10f) {
		const float mag = std::sqrt(mag2);
		if (mag > kMaxBarrierPushPerStep) {
			accumXZ = accumXZ * (kMaxBarrierPushPerStep / mag);
		}
		player_->setCurrPos(myPos + accumXZ);  // Y 불변
		moveChange_ = true;
	}
}

// 최종 보스 분리: 거대한 나무인 보스는 플레이어에게 밀리지 않아야 한다.
//
// 종전엔 서버 물리가 이 역할을 대신하고 있었다 — 플레이어가 보스에 파고들면 접촉 해소가 보스를
// 밀어냈고, 그 반작용이 "단단함"으로 보였다. 문제는 서버 플레이어 바디가 Kinematic(무한 질량)인 데다
// C_Move(20Hz)마다 setPos로 텔레포트해서, 한 패킷에 최대 50cm를 파고든 뒤 보스가 그만큼 튕겨
// 나갔다는 것이다(한 발자국씩 순간이동). 질량을 올려도 소용없다 — 무한 질량 상대와의 접촉에서
// 보스의 속도 변화량은 질량과 무관하게 depenetration bias와 같다.
// → 서버는 플레이어↔보스 접촉을 필터링하고(Room::setupFinalBoss), 차단은 barrier와 동일한
//   위치 보정 방식으로 이쪽에서 한다(임펄스 없음 → 튕김 없음).
void Game::resolveBossSeparation(Seconds dt) {
	if (!player_ || playerDead_) return;
	if (bosses_.empty()) return;

	const float sumR  = kPlayerSeparationRadius + kBossSeparationRadius;
	const float sumR2 = sumR * sumR;

	const mu::Vec3 myPos = player_->pos();
	mu::Vec3 accumXZ{ 0.f, 0.f, 0.f };

	for (const auto& boss : bosses_) {
		// 시체/사망 보스는 통과시킨다(래그돌 연출 중 플레이어가 갇히지 않게).
		if (!boss || boss->hp() <= 0 || boss->isDead()) continue;

		const mu::Vec3 bp = boss->pos();
		const float dx = myPos.x() - bp.x();
		const float dz = myPos.z() - bp.z();
		const float d2 = dx * dx + dz * dz;
		if (d2 >= sumR2) continue;

		// 중심이 완전히 겹친 축퇴 상황: 방향이 없으므로 보스의 전방 반대로 밀어낸다.
		if (d2 < 1e-8f) {
			const mu::Vec3 f = boss->forward();
			accumXZ += mu::Vec3(-f.x(), 0.f, -f.z()) * sumR;
			continue;
		}

		const float d    = std::sqrt(d2);
		const float push = sumR - d;                  // 침투량 전체
		accumXZ += mu::Vec3(dx / d, 0.f, dz / d) * push;
	}

	const float mag2 = accumXZ.len2();
	if (mag2 > 1e-10f) {
		const float mag = std::sqrt(mag2);
		if (mag > kMaxBossPushPerStep)
			accumXZ = accumXZ * (kMaxBossPushPerStep / mag);
		player_->setCurrPos(myPos + accumXZ);  // Y 불변
		moveChange_ = true;
	}
}

void Game::movePlayer(uint16 playerId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 velocity) {
	// find(): operator[] would insert a null entry for an unknown id, and code
	// that find()s the map and dereferences without a null check would crash.
	auto playerIt = idPlayerMap_.find(playerId);
	auto player = playerIt != idPlayerMap_.end() ? playerIt->second : nullptr;

	DISPLAY_ERROR_STR(player != nullptr,
		"[Game Error] Game::movePlayer: 이동하려는 플레이어가 존재하지 않습니다.\n",
		false
	);

	if (player == nullptr) {
		return;
	}

	if (player->isDead()) {
		return;
	}

	// 서버가 보낸 velocity를 그대로 사용한다.
	// 위치 차이로 역산하던 이전 방식은 패킷 지터에 민감해 애니메이션 진동을 유발했다.
	player->body().advanceState();
	player->setCurrPos( DirectX::XMLoadFloat3( &pos ) );
	player->setVelocity( DirectX::XMLoadFloat3( &velocity ) );
	player->netInterpAcc_ = 0s;
}

void Game::onPlayerKnockback( uint16 playerId, float dirX, float dirZ, float speed, uint16 knockMs, uint16 postLockMs ) {
	// 로컬 플레이어만 처리한다(다른 플레이어의 넉백은 그쪽 클라가 실행해 S_Move로 위치가 동기화됨).
	// 서버는 RoomServer의 session->id()를 대상 ID로 보내므로, 인게임 정식 ID인
	// player_->getId()(== S_Enter의 playerId)와 비교해야 한다. myLobbyId_(로비 sessionId)는
	// RoomServer ID와 체계가 달라 여기서 쓰면 일부 플레이어가 자기 넉백을 놓친다.
	if ( player_ == nullptr || playerId != static_cast<uint16>( player_->getId() ) ) {
		return;
	}
	knockbackDir_           = mu::Vec3( dirX, 0.f, dirZ );
	knockbackSpeed_         = speed;
	knockbackTimer_         = static_cast<float>( knockMs ) / 1000.f;
	postKnockbackLockTimer_ = static_cast<float>( postLockMs ) / 1000.f;
}

void Game::rotatePlayer(uint16 playerId, float yawRad, float pitchRad) {
	auto playerIt = idPlayerMap_.find(playerId);
	auto player = playerIt != idPlayerMap_.end() ? playerIt->second : nullptr;

	DISPLAY_ERROR_STR(player != nullptr,
		"[Game Error] Game::rotatePlayer: 회전하려는 플레이어가 존재하지 않습니다.\n",
		false
	);

	if (player == nullptr) {
		return;
	}

	if (player->isDead()) {
		return;
	}

	const mu::NQuat yaw = mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(yawRad));
	player->setOrient(yaw);
	// 원격 플레이어 조준 pitch(상체 굽힘 시각용). body orient에는 넣지 않는다.
	player->setAimPitch(pitchRad);
}

void Game::moveGoblin(uint16 npcId, uint8 statusFlags, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 orient, DirectX::XMFLOAT3 velocity) {
	auto it = idMonsterMap_.find(npcId);
	if (it == idMonsterMap_.end()) {
		// Detached as a corpse (dead, awaiting respawn): in-flight server moves for it are
		// expected — ignore silently. Pre-corpse this was a dead monster left in the map.
		if (detachedNpcIds_.count(npcId)) return;
		DISPLAY_ERROR_STR(false, "[Game Error] Game::moveGoblin: NPC not found.\n", false);
		return;
	}
	Object* monster = it->second;

	// 서버가 이 npc의 이동을 마지막으로 보낸 시각. 감사(F12)에서 오래 갱신되지 않은
	// 개체를 STALE로 표시한다 — 서버가 hp<=0으로 판단해 move 배치에서 제외했다는 신호.
	lastNpcMoveAt_[npcId] = diagElapsed_;

	if (monster->isDead()) return;

	monster->body().advanceState();
	monster->setCurrPos(DirectX::XMLoadFloat3(&pos));
	monster->setOrient(DirectX::XMLoadFloat4(&orient));
	monster->setVelocity(DirectX::XMLoadFloat3(&velocity));
	// Restart network interpolation for this move (mirrors movePlayer). Without this the
	// monster would interpolate on the physics clock and oscillate between prev/curr.
	monster->netInterpAcc_ = 0s;
	setNpcStatusFlags(npcId, statusFlags);
}

void Game::setNpcStatusFlags(uint16 npcId, uint8 statusFlags) {
	auto entryIt = npcStatusIcons_.find(npcId);
	const bool confused = hasNpcStatusFlag(statusFlags, NpcStatusFlag::Confused);

	if (!confused) {
		if (entryIt != npcStatusIcons_.end()) {
			entryIt->second.statusFlags = statusFlags;
			if (entryIt->second.icon) entryIt->second.icon->visible = false;
		}
		return;
	}

	if (entryIt == npcStatusIcons_.end()) {
		auto* icon = static_cast<UI::Image*>(
			uiManager_.root()->addChild(std::make_unique<UI::Image>())
		);
		icon->name    = "npcConfusionStatusIcon";
		icon->anchor  = UI::Anchors::TopLeft;
		icon->pivot   = UI::Pivots::Center;
		icon->width   = UI::DimValue::px(kConfusionIconBaseSize);
		icon->height  = UI::DimValue::px(kConfusionIconBaseSize);
		icon->texture = assetManager_.confusionStatusIcon();
		icon->zOrder  = 50;
		icon->visible = false;

		const float phaseOffset =
			(static_cast<float>(npcId % 31u) / 31.f) * kTwoPi;
		entryIt = npcStatusIcons_.emplace(
			npcId,
			NpcStatusIconEntry{
				.icon = icon,
				.statusFlags = statusFlags,
				.phaseOffset = phaseOffset
			}
		).first;
	}
	else {
		entryIt->second.statusFlags = statusFlags;
	}
}

void Game::updateNpcStatusIcons(float deltaTimeSec) {
	npcStatusIconElapsed_ += deltaTimeSec;
	if (npcStatusIconElapsed_ > 3600.f) {
		npcStatusIconElapsed_ = std::fmod(npcStatusIconElapsed_, 3600.f);
	}

	auto resolveWorldYOffset = [&](uint16 npcId) {
		if (auto it = goblinHpBars_.find(npcId); it != goblinHpBars_.end())
			return it->second.worldYOffset;

		auto fromMonsterBars = [npcId](const auto& bars, float& out) {
			if (auto it = bars.find(npcId); it != bars.end()) {
				out = it->second.worldYOffset;
				return true;
			}
			return false;
		};

		float result = 2.5f;
		if (fromMonsterBars(snakeHpBars_, result))    return result;
		if (fromMonsterBars(mushroomHpBars_, result)) return result;
		if (fromMonsterBars(bomberHpBars_, result))   return result;
		if (fromMonsterBars(birdyHpBars_, result))    return result;
		if (fromMonsterBars(slimeHpBars_, result))    return result;
		if (fromMonsterBars(treantHpBars_, result))   return result;
		if (fromMonsterBars(bossHpBars_, result))     return result;
		return result;
	};

	const Texture* confusionTexture = assetManager_.confusionStatusIcon();
	for (auto& [npcId, entry] : npcStatusIcons_) {
		if (!entry.icon) continue;
		entry.icon->visible = false;

		if (!hasNpcStatusFlag(entry.statusFlags, NpcStatusFlag::Confused) ||
			!confusionTexture || !confusionTexture->res) {
			continue;
		}

		auto monsterIt = idMonsterMap_.find(npcId);
		if (monsterIt == idMonsterMap_.end()) continue;
		Object* monster = monsterIt->second;
		if (!monster || monster->hidden() || monster->isHiddenByOrb() ||
			monster->isDead() || monster->hp() <= 0) {
			continue;
		}

		const mu::Vec3 iconWorldPos = monster->renderState().pos
			+ mu::Vec3{ 0.f, resolveWorldYOffset(npcId) + kConfusionIconWorldGap, 0.f };
		float screenX{}, screenY{};
		if (!worldToScreen(
				iconWorldPos,
				camera_.view(), camera_.proj(),
				uiManager_.screenWidth(), uiManager_.screenHeight(),
				screenX, screenY)) {
			continue;
		}

		const float phase = entry.phaseOffset;
		const float pulse = 1.f + kConfusionIconPulseAmount * std::sinf(
			npcStatusIconElapsed_ * kTwoPi * kConfusionIconPulseHz + phase);
		const float bob = kConfusionIconBobPixels * std::sinf(
			npcStatusIconElapsed_ * kTwoPi * kConfusionIconBobHz + phase);
		const float size = kConfusionIconBaseSize * pulse;

		entry.icon->width   = UI::DimValue::px(size);
		entry.icon->height  = UI::DimValue::px(size);
		entry.icon->offsetX = UI::DimValue::px(uiManager_.screenToLayoutX(screenX));
		entry.icon->offsetY = UI::DimValue::px(uiManager_.screenToLayoutY(screenY) - bob);
		entry.icon->visible = true;
	}
}

void Game::onNpcAttack( uint16 npcId ) {
	if (idMonsterMap_.count(npcId) == 0) {
		// Detached as a corpse (dead, awaiting respawn): ignore stray attack packets silently.
		if (detachedNpcIds_.count(npcId)) return;
		DISPLAY_ERROR_STR(false, "[Game Error] Game::onNpcAttack: NPC not found.\n", false);
		return;
	}
	holdEvent( eventList_, EvAttack( npcId ) );
}

void Game::onPlayerAttack( uint16 attackerId ) {
	holdEvent( eventList_, EvAttack( attackerId ) );
}

void Game::applyHit( uint16 targetId, int32 newHp, int32 attackerId, uint8 hitAnimIndex ) {
	// HP바 가시성은 EventBus가 다루지 않는 시각 상태이므로 여기서 갱신한다(모든 몬스터·거점 공통).
	if ( auto barIt = goblinHpBars_.find( targetId ); barIt != goblinHpBars_.end() )
		barIt->second.hpBarVisibleSeconds = 5.f;
	if ( auto barIt = snakeHpBars_.find( targetId ); barIt != snakeHpBars_.end() )
		barIt->second.hpBarVisibleSeconds = 5.f;
	if ( auto barIt = mushroomHpBars_.find( targetId ); barIt != mushroomHpBars_.end() )
		barIt->second.hpBarVisibleSeconds = 5.f;
	if ( auto barIt = bomberHpBars_.find( targetId ); barIt != bomberHpBars_.end() )
		barIt->second.hpBarVisibleSeconds = 5.f;
	if ( auto barIt = birdyHpBars_.find( targetId ); barIt != birdyHpBars_.end() )
		barIt->second.hpBarVisibleSeconds = 5.f;
	if ( auto barIt = slimeHpBars_.find( targetId ); barIt != slimeHpBars_.end() )
		barIt->second.hpBarVisibleSeconds = 5.f;
	if ( auto barIt = treantHpBars_.find( targetId ); barIt != treantHpBars_.end() )
		barIt->second.hpBarVisibleSeconds = 5.f;
	if ( auto it = strongholdHpBars_.find( targetId ); it != strongholdHpBars_.end() )
		it->second.hpBarVisibleSeconds = 5.f;

	// HP / isDead / 래그돌 / 피격·사망 애니메이션은 EvHit/EvDeath 핸들러(Object::EventBus)가 소유한다.
	// 디스패치 루프(InGameScene)에서 대상 객체의 eventBus로 분배되며, 데미지 넘버도 그곳에서 생성된다.
	// 거점도 이제 Stronghold::EventBus를 가지므로 고블린/플레이어와 동일 경로를 탄다.
	if ( newHp <= 0 )
		holdEvent( eventList_, EvDeath( targetId, attackerId ) );
	else
		holdEvent( eventList_, EvHit( targetId, newHp, attackerId, hitAnimIndex ) );
}

void Game::onNpcRespawn( uint16 npcId, int32 newHp, DirectX::XMFLOAT3 spawnPos ) {
	// This npc id is live again — clear any "detached as corpse" mark (covers all three
	// respawn sub-paths below: revive-in-place / pool reuse / fresh create).
	detachedNpcIds_.erase(npcId);
	setNpcStatusFlags(npcId, npcStatusMask(NpcStatusFlag::None));

	// Still active (e.g. temporarily hidden via S_NpcHide, never killed): revive in place.
	if (auto it = idMonsterMap_.find(npcId); it != idMonsterMap_.end()) {
		Object* npc = it->second;
		npc->setHp( newHp );
		npc->setHitImpulseImmune( false );
		npc->setHidden( false );   // 숨김(S_NpcHide)으로 퇴장했던 NPC 복귀 시 재표시
		npc->setHiddenByOrb( false );
		// isDead_ 리셋 및 사망/부활 애니메이션은 EvRespawn 핸들러(EventBus)가 소유한다.
		holdEvent( eventList_, EvRespawn( npcId ) );
		// Clear a pending death-ragdoll: if a lethal hit arrived this frame (EvDeath set
		// ragdollPendingActivation) and the respawn revives the npc in place before the
		// justDied loop runs, the loop would otherwise still activate the ragdoll and
		// migrate this now-alive monster into a corpse — removing it from idMonsterMap_ so
		// it receives neither moves nor hits. Clearing the pending flag prevents that.
		npc->setRagdollPendingActivation( false );
		if (npc->ragdoll() && npc->ragdoll()->isActive())
			npc->ragdoll()->deactivate(physicsWorld_);
		npc->body().setLinearVel( mu::Vec3{} );   // drop any stale death-frame velocity
		npc->netInterpAcc_ = 0s;                   // restart network interpolation cleanly
		npc->setPos( DirectX::XMLoadFloat3( &spawnPos ) );
		return;
	}

	// Detached into a client-authored corpse on death: spawn a fresh instance so the
	// corpse animation keeps running. Reuse a pooled object if available, else create.
	const MonsterKind kind = respawnKind_.count(npcId) ? respawnKind_.at(npcId) : MonsterKind::Goblin;
	const mu::Vec3    pos  = DirectX::XMLoadFloat3( &spawnPos );
	if (reinitFromPool(kind, npcId, pos, newHp)) return;

	// Pool empty (corpse still animating): create a new monster from stored spawn info.
	ObjectInfo info{};
	if (auto si = monsterSpawnInfo_.find(npcId); si != monsterSpawnInfo_.end()) info = si->second;
	info.objectId = npcId;
	info.pos      = spawnPos;
	info.hp       = newHp;
	info.maxHp    = (info.maxHp > newHp) ? info.maxHp : newHp;
	switch (kind) {
	case MonsterKind::Goblin:   createGoblin(info);   break;
	case MonsterKind::Snake:    createSnake(info);    break;
	case MonsterKind::Mushroom: createMushroom(info); break;
	case MonsterKind::Bomber:   createBomber(info);   break;
	case MonsterKind::Birdy:    createBirdy(info);    break;
	case MonsterKind::Slime:    createSlime(info);    break;
	case MonsterKind::Treant:   createTreant(info);   break;
	case MonsterKind::Boss:     createBoss(info);     break;
	}
	holdEvent( eventList_, EvRespawn( npcId ) );
}

void Game::onSkillStart( uint16 ownerId, uint32 skillAssetId, uint16 elapsedMs, uint32 skillSeed, float aimPitchRad,
                         bool castAnchorValid, float castAnchorX, float castAnchorZ ) {
	// Trigger attack animation on the remote player that cast the skill.
	holdEvent( eventList_, EvAttack( ownerId ) );

	// Cast-time aim pitch snapshot: set on the remote caster BEFORE startSkill so
	// t=0 timeline events reproduce the caster's pitched trajectory (NPC casts are 0).
	if (auto it = idPlayerMap_.find(ownerId); it != idPlayerMap_.end()) {
		it->second->setAimPitch(aimPitchRad);
	}

	// Start skill visuals for the remote owner (clientPredictionOnly — no damage).
	// skillSeed is the caster-generated seed relayed by the server, so this
	// client renders the identical particle layout the server judges hits on.
	// APC-time call: re-sync skillCtx_ in case skillObjectById_ was resized by an
	// earlier packet in this batch.
	refreshSkillCtx();
	debugLogSkillOwnerResolution("remote", skillAssetId, static_cast<i32t>(ownerId));
	// Targeted ground skills relay their cast anchor (world XZ) so this client plants the
	// effect + its Ground-attach hitbox exactly where the server judged it. Y is re-sampled
	// from the terrain, and yaw still comes from the caster.
	const mu::Vec3 anchorPos{ castAnchorX, 0.f, castAnchorZ };
	skillSystem_.startSkill(skillAssetId, static_cast<i32t>(ownerId), skillCtx_,
	                        Milliseconds{ static_cast<float>(elapsedMs) }, skillSeed,
	                        castAnchorValid ? &anchorPos : nullptr);
}

void Game::onSkillHit( uint16 attackerId, uint16 targetId, int32 newHp, uint32 skillAssetId, DirectX::XMFLOAT3 targetVelocity, uint8 hitAnimIndex ) {
	// Store hit velocity on any monster before applyHit so ragdoll activation can use it.
	if (newHp <= 0) {
		if (auto it = idMonsterMap_.find(targetId); it != idMonsterMap_.end()) {
			it->second->setRagdollInitVelocity(DirectX::XMLoadFloat3(&targetVelocity));
		}
	}
	applyHit(targetId, newHp, attackerId, hitAnimIndex);

	// 피격 VFX(blood 등)는 클라 로컬 hit 검출(SkillSystem::processHitResults)이
	// narrow phase 충돌점에 직접 재생한다. 여기서 target->pos()(발밑)에 재생하면
	// 위치가 부정확하고 예측 경로와 중복되므로 재생하지 않는다.
}

void Game::onDebugHitboxes( SDebugHitboxPacket* pkt ) {
	auto list = pkt->getOBBList();
	for (uint16 i = 0; i < list.count(); ++i) {
		const OBBInfo& info = list[i];
		debugBVView_.push(
			OBB{
				mu::Vec3(DirectX::XMLoadFloat3(&info.center)),
				mu::Vec3(DirectX::XMLoadFloat3(&info.halfExtents)),
				mu::NQuat(DirectX::XMLoadFloat4(&info.orient), mu::NQuat::NoNormalize_t{})
			},
			Milliseconds{ 100.f },
			BVPipeline::BVModel::Box,
			mu::Vec4{ 1.f, 0.2f, 0.2f, 1.f }   // 빨강 = 서버 권위 히트박스 (클라 예측은 초록)
		);
	}
}

// 게임의 업데이트는 다음 순서대로 이루어진다.
// 네트워크 패킷 처리
// 입력 처리
// 이벤트 처리
// 물리 업데이트 루틴
// 객체별 업데이트 루틴
// 애니메이션 업데이트
void Game::update(Milliseconds deltaTime) {
	// 설정창에서 바뀐 디스플레이 설정(해상도/전체화면)을 씬 갱신/렌더 이전(프레임 안전 지점)에 적용한다.
	applyPendingDisplaySettings();
	lobbyReturnFade_.update(
		std::chrono::duration<float>(deltaTime).count());

	switch (scene_) {
	case Scene::Lobby:  LobbyScene(deltaTime);  break;
	case Scene::InGame: InGameScene(deltaTime); break;
	}
}

void Game::render() {
	gfx_.setVsync(settings_.vsync);

	// 설정 볼륨(0~100%)을 오디오 버스에 매 프레임 적용(setVsync와 동일한 폴링 방식).
	// 창 비활성 시 마스터를 0으로 내려 음소거한다. UI 버스는 효과음 볼륨을 따른다.
	extern bool gWindowActive;
	auto& snd = INet::ClientApp::sound();
	snd.setMasterVolume(gWindowActive ? settings_.masterVolume * 0.01f : 0.f);
	snd.setBusVolume(SoundManager::Bus::Bgm, settings_.bgmVolume * 0.01f);
	snd.setBusVolume(SoundManager::Bus::Sfx, settings_.sfxVolume * 0.01f);
	snd.setBusVolume(SoundManager::Bus::Ui,  settings_.sfxVolume * 0.01f);

	switch (scene_) {
	case Scene::Lobby:  renderLobby();  break;
	case Scene::InGame: renderInGame(); break;
	}
}

// Re-syncs the per-frame skill dispatch pointers. Called every frame from
// InGameScene, and again by packet handlers that reach the skill system from
// the frame-start APC (where the last refresh may predate a container resize).
void Game::refreshSkillCtx() {
	skillCtx_.evList         = &eventList_;
	skillCtx_.pTimer         = pTimer_;
	skillCtx_.objectById     = skillObjectById_.data();
	skillCtx_.objectByIdSize = static_cast<int>(skillObjectById_.size());
}

void Game::InGameScene(Milliseconds deltaTime) {
	SleepEx(1, true);

	updateServerTimeSync();

	if (player_ == nullptr) {
		return;
	}

	// F12 감사의 STALE 판정 기준이 되는 단조 시계.
	diagElapsed_ += deltaTime;

	// Skill dispatch context: refresh per-frame pointers.
	refreshSkillCtx();

	// 이전 프레임 속도 저장
	prevVelocity_ = currVelocity_;

	// 입력 처리 (속도는 프레임 간 유지 - processInputGame이 감속/가속 관리)
	processInput(deltaTime);

	// 보스 처치 연출은 카메라/UI/네트워크 시계를 늦추지 않고 로컬 시뮬레이션만
	// 감속한다. 실시간 카메라 시계 덕분에 연출은 항상 정해진 시간 안에 복귀한다.
	const float simulationScale = finalScoreboard_.isVisible()
		? 0.f : camera_.focusCinematicTimeScale();
	const Milliseconds simulationDeltaTime = deltaTime * simulationScale;

	// 현재 속도 저장
	currVelocity_ = player_->velocity();
	
	// 속도가 바뀌거나 현재 이동 중이면 move 패킷 전송 플래그를 켠다.
	// 정속 이동 중에도 패킷을 전송해야 원격 클라이언트의 100ms 타임아웃이 발동하지 않는다.
	constexpr float kMoveThreshold = 0.05f;
	if (prevVelocity_ != currVelocity_ || currVelocity_.len() > kMoveThreshold) {
		moveChange_ = true;
	}

	// 물리 업데이트 루틴
	//
	// 물리량 갱신은 게임 갱신과 다르게 고정 주기로 수행한다.
	// 이를 통해 너무 유동적인 delta time으로 인한 시뮬레이션의 불안정성과
	// 물리 업데이트의 성능적 비용 문제를 해결한다.
	// 물리 업데이트 주기는 physicUpdateInterval_ 변수에 저장된다.
	//
	// update 함수에서 physicUpdateAcc_ 변수를 통해
	// 물리량 갱신의 주기가 돌아왔는지 판단하고
	// 주기가 되었다면 물리량 갱신을 수행한다.
	const Seconds clampedDt = std::min(Seconds(simulationDeltaTime), kMaxPhysicsDeltaTime);
	physicUpdateAcc_ += clampedDt;

	// move 패킷 전송 주기 판단
	moveStateSendAcc_ += deltaTime;

	const Seconds effectiveInterval = physicUpdateInterval * static_cast<float>(physicUpdateScaleK_);

	// Death-ragdoll corpses need far more PGS iterations to converge than the default
	// budget (tuned for shallow/branching constraint sets like player-terrain contacts).
	// A long unbranched joint chain (e.g. Snake: 16 bodies, 15 joints deep, vs ~3-6 hops
	// for every other ragdoll) cannot converge within the default 4 velocity passes --
	// the residual position error compounds every step until the chain destabilizes and
	// flies apart within about a second. Mirrors the debug ragdoll-test-object iteration
	// boost in standalone/game.cpp (spawnTestObject), which documents the same mechanism
	// for deep ConeTwist chains; this wires it into the real corpse path. Cheap: contacts
	// are unaffected, only joints get the extra passes (setJointSolverExtraIterations).
	const bool anyRagdollActive = std::any_of(corpses_.begin(), corpses_.end(),
		[](const Corpse& c) { return c.phase == Corpse::Phase::Ragdoll; });
	physicsWorld_.setJointSolverExtraIterations(anyRagdollActive ? 48 : 0);

	int physicsStepsDone = 0;
	while (physicUpdateAcc_ >= effectiveInterval
		   && physicsStepsDone < kMaxPhysicsStepsPerFrame) {
		physicsWorld_.step(effectiveInterval);
		// 접지 중력 게이팅: 이번 step의 terrain 접촉을 보고 로컬 플레이어 접지 판정 +
		// 중력 게이트 설정(다음 step에 반영) + ground-snap. 물리 step 루프 안에서
		// (렌더 프레임이 아니라) 호출해야 고정 timestep 의미가 유지된다.
		if (player_) player_->updateGroundedGravityGate(physicsWorld_, effectiveInterval);
		// 물리 적분 직후, 로컬 플레이어를 다른 플레이어와 reciprocal soft separation.
		// (setCurrPos로 curr만 갱신 → 렌더 보간의 prev는 보존된다)
		resolvePlayerSeparation(effectiveInterval);
		// 전술 차단벽: barrier 활성 NPC 밖으로 로컬 플레이어를 전체 침투량만큼 밀어낸다.
		resolveBarrierSeparation(effectiveInterval);
		// 최종 보스: 서버가 플레이어↔보스 접촉을 필터링하므로 차단은 여기서만 이뤄진다.
		resolveBossSeparation(effectiveInterval);
		physicUpdateAcc_ -= effectiveInterval;
		++physicsStepsDone;
	}

	// 아레나 후방 Wall 일방향 벽: 직전 프레임 대비 양끝 Wall을 바깥으로 통과하려는 로컬 플레이어를
	// 평면으로 되돌린다(정적 벽이라 sub-step 불필요, 프레임당 1회). 입장·전진·측면은 통과.
	resolveArenaWallLeash();

	if (physicsStepsDone >= kMaxPhysicsStepsPerFrame) {
		++consecutiveLagFrames_;
		consecutiveNonLagFrames_ = 0;
		if (consecutiveLagFrames_ >= kLagScaleUpFrames && physicUpdateScaleK_ < kMaxPhysicsScaleK) {
			++physicUpdateScaleK_;
			consecutiveLagFrames_ = 0;
		}
	} else {
		consecutiveLagFrames_ = 0;
		if (physicUpdateScaleK_ > 1) {
			if (++consecutiveNonLagFrames_ >= kLagScaleDownFrames) {
				--physicUpdateScaleK_;
				consecutiveNonLagFrames_ = 0;
			}
		} else {
			consecutiveNonLagFrames_ = 0;
		}
	}
	skipNextRender_ = (consecutiveLagFrames_ >= kRenderSkipLagFrames);

	for (auto& p : otherPlayers_) {
		p->rebuildBodyBVH();
	}
	for (auto& g : goblins_)   g->rebuildBodyBVH();
	for (auto& s : snakes_)    s->rebuildBodyBVH();
	for (auto& m : mushrooms_) m->rebuildBodyBVH();
	for (auto& b : bombers_)   b->rebuildBodyBVH();
	for (auto& b : birdys_)    b->rebuildBodyBVH();
	for (auto& s : slimes_)    s->rebuildBodyBVH();
	for (auto& t : treants_)   t->rebuildBodyBVH();
	for (auto& b : bosses_)    b->rebuildBodyBVH();

	if (!playerDead_)
		skillSystem_.update(simulationDeltaTime, skillCtx_);

	// 클라 예측 히트박스(초록)를 서버 권위 히트박스(빨강, onDebugHitboxes)와 나란히 렌더해
	// 포즈/타이밍 오프셋을 육안 비교한다. 서버의 kBroadcastDebugHitboxes(Room.cpp)와 짝으로 켠다.
	// 상세: RoomServer/docs/roomTickCadence.md §8.2
	static constexpr bool kDebugSkillHitboxOverlay = false;
	if constexpr (kDebugSkillHitboxOverlay)
		skillSystem_.renderDebugHitboxes(debugBVView_);

	// 이벤트 디스패치
	//
	// 애니메이션 트리거(피격/공격/사망/부활)는 trigger* 직접 호출 대신
	// EventBus로 일원화한다. 패킷 핸들러(프레임 시작 SleepEx)와 스킬 시스템(update)이
	// eventList_에 post한 이벤트를, 대상 객체의 eventBus로 분배한다.
	// (서버 권위 게임이므로 데미지 적용은 applyHit이 이미 끝냈고, 여기서는 HP/연출/애니메이션만 다룬다.)
	// 객체 갱신(아래) 이전에 처리해 같은 프레임에 애니메이션 상태가 반영되도록 한다.
	{
		const Seconds evDt = std::chrono::duration_cast<Seconds>(simulationDeltaTime);

		auto resolveObject = [&](i32t id) -> Object* {
			if (player_ && player_->getId() == id) return player_.get();
			if (auto it = idPlayerMap_.find(static_cast<uint16>(id)); it != idPlayerMap_.end())
				return it->second.get();
			if (auto it = idMonsterMap_.find(static_cast<uint16>(id)); it != idMonsterMap_.end())
				return it->second;
			if (auto it = strongholdHpBars_.find(static_cast<uint16>(id)); it != strongholdHpBars_.end())
				return it->second.obj;
			return nullptr;
		};

		for (auto pEvRaw : eventList_) {
			auto pEv = reinterpret_cast<BasicEvent*>(pEvRaw);
			i32t routeId = -1;
			switch (pEv->type) {
			case EventType::Hit:     routeId = static_cast<const EvHit*>(pEv)->targetId;       break;
			case EventType::Attack:  routeId = static_cast<const EvAttack*>(pEv)->attackerId;  break;
			case EventType::Death:   routeId = static_cast<const EvDeath*>(pEv)->victimId;     break;
			case EventType::Respawn: routeId = static_cast<const EvRespawn*>(pEv)->targetId;   break;
			default: break;
			}
			if (routeId < 0) continue;

			Object* obj = resolveObject(routeId);
			if (!obj) continue;

			// 보스/중간보스의 최초 Death만 포커스한다. BossHeatState는 스폰 타입
			// 기준으로 등록되므로 RTTI 없이 보스 계열을 정확히 구분할 수 있다.
			if (pEv->type == EventType::Death && !obj->isDead()) {
				auto heatIt = bossHeatProfiles_.find(static_cast<uint16>(routeId));
				if (heatIt != bossHeatProfiles_.end()) {
					auto findOwner = [routeId](const auto& monsters) -> std::shared_ptr<Object> {
						auto it = std::ranges::find_if(monsters, [routeId](const auto& monster) {
							return monster && monster->getId() == routeId;
						});
						return it != monsters.end()
							? std::static_pointer_cast<Object>(*it)
							: std::shared_ptr<Object>{};
					};

					std::shared_ptr<Object> bossTarget;
					if (auto it = idGoblinMap_.find(static_cast<uint16>(routeId));
						it != idGoblinMap_.end()) {
						bossTarget = std::static_pointer_cast<Object>(it->second);
					}
					if (!bossTarget) bossTarget = findOwner(birdys_);
					if (!bossTarget) bossTarget = findOwner(treants_);
					if (!bossTarget) bossTarget = findOwner(bosses_);

					if (bossTarget) {
						const BossHeatState& presentation = heatIt->second;
						const bool isFinalBoss = std::ranges::any_of(
							bosses_, [routeId](const std::shared_ptr<Boss>& boss) {
								return boss && boss->getId() == routeId;
							});
						Camera::FocusCinematicConfig config{};
						config.duration = Milliseconds{ isFinalBoss ? 2300.f : 1950.f };
						config.blendIn = Milliseconds{ 350.f };
						config.blendOut = Milliseconds{ 550.f };
						config.slowMotionScale = isFinalBoss ? 0.14f : 0.18f;
						config.focusHeight = std::max(0.9f, presentation.heightBias);
						config.shotDistance = std::max(4.5f, presentation.worldRadius * 1.65f);
						config.shotHeight = std::max(0.5f, presentation.worldRadius * 0.2f);
						config.zoomFovy = mu::Degree{ isFinalBoss ? 46.f : 52.f };
						camera_.playFocusCinematic(bossTarget, config);
						if (isFinalBoss) {
							finalScoreboardPending_ = true;
							finalBossRewardCorpseId_ = 0u;
							finalBossRewardCorpseTracked_ = false;
							finalBossRewardOrbsSpawned_ = false;
							const i32t killerId = static_cast<const EvDeath*>(pEv)->killerId;
							if (killerId >= 0
								&& std::ranges::find(
									inGamePartyPlayerIds_, static_cast<uint16>(killerId))
									!= inGamePartyPlayerIds_.end()) {
								inGameBossLastHitPlayerId_ = killerId;
							}
						}
					}
				}
			}

			// Combat feedback (game thread, no locking): compute damage from the
			// pre-receive HP since the EvHit/EvDeath handler below mutates hp/dead.
			if (pEv->type == EventType::Hit || pEv->type == EventType::Death) {
				const int prevHp = obj->hp();
				const int dmg = (pEv->type == EventType::Hit)
					? prevHp - static_cast<const EvHit*>(pEv)->hp
					: prevHp;   // Death: remaining HP is the killing-blow damage
				// A confirmed non-lethal hit may legitimately deal zero damage (for
				// example, against a tactically invulnerable NPC). Negative deltas are
				// stale/out-of-order HP updates and remain hidden.
				const bool shouldShowDamage = (pEv->type == EventType::Hit)
					? dmg >= 0
					: dmg > 0;
				if (shouldShowDamage) {
					const uint16 targetId = static_cast<uint16>(routeId);
					DamageKind kind = DamageKind::EnemyHit;
					if (idPlayerMap_.find(targetId) != idPlayerMap_.end())
						kind = DamageKind::PlayerHit;
					else if (strongholdHpBars_.find(targetId) != strongholdHpBars_.end())
						kind = DamageKind::StrongholdHit;
					const mu::Vec3 anchor = obj->renderState().pos
						+ mu::Vec3{ 0.f, damageNumberSystem_.tuning().worldHeadOffsetY, 0.f };
					damageNumberSystem_.spawn(anchor, dmg, kind, targetId);
				}
				// Attribute only server-confirmed damage actually applied to monsters.
				// EvHit keeps the attacker id alongside the authoritative post-hit HP;
				// lethal hits use EvDeath::killerId. Every client therefore derives the
				// same party totals without counting prediction-only contacts or overkill.
				i32t damageDealerId = -1;
				if (pEv->type == EventType::Hit) {
					const auto* hit = static_cast<const EvHit*>(pEv);
					if (hit->attackerId != EvHit::kInvalidAttackerId) {
						damageDealerId = static_cast<i32t>(hit->attackerId);
					}
				}
				else {
					damageDealerId = static_cast<const EvDeath*>(pEv)->killerId;
				}
				const bool isMonsterTarget =
					idMonsterMap_.find(static_cast<uint16>(routeId)) != idMonsterMap_.end();
				const bool isPartyDamageDealer = damageDealerId >= 0
					&& std::ranges::find(
						inGamePartyPlayerIds_, static_cast<uint16>(damageDealerId))
						!= inGamePartyPlayerIds_.end();
				if (dmg > 0 && isMonsterTarget && isPartyDamageDealer) {
					inGameDamageByPlayerId_[static_cast<uint16>(damageDealerId)] += dmg;
				}
				// Every client receives the same server-confirmed lethal hit and killer id.
				// Count every monster type so the final ranking is identical for the party.
				if (pEv->type == EventType::Death && !obj->isDead()
					&& idMonsterMap_.find(static_cast<uint16>(routeId)) != idMonsterMap_.end()) {
					const i32t killerId = static_cast<const EvDeath*>(pEv)->killerId;
					const bool isPartyPlayer = killerId >= 0
						&& std::ranges::find(inGamePartyPlayerIds_, static_cast<uint16>(killerId))
							!= inGamePartyPlayerIds_.end();
					if (isPartyPlayer) {
						++inGameMonsterKillsByPlayerId_[static_cast<uint16>(killerId)];
						if (killCountWidget_ && player_ && killerId == player_->getId()) {
							killCountWidget_->addKill();
						}
					}
				}
			}

			obj->eventBus()->receive(pEv, evDt, eventList_, *pTimer_, obj);

			// 로컬 플레이어 사망 시 게임 레벨 플래그를 세운다. (standalone game.cpp의 playerDead_ 처리와 대응)
			if (pEv->type == EventType::Death && player_ && player_->getId() == routeId)
				playerDead_ = true;
		}
	}

	// Advance floating damage numbers (game thread; KillCountWidget ticks via uiManager_.update).
	damageNumberSystem_.update(std::chrono::duration<float>(deltaTime).count());

	if (moveStateSendAcc_ >= moveStateSendInterval_) {
		moveStateSendAcc_ = 0s;

		if (moveChange_) {
			sendMovePacket();
		}
	}
	moveChange_ = false;

	// 객체별 업데이트 루틴
	// 
	// 물리량 갱신 주기에 대해,
	// 마지막 물리량 갱신으로부터 얼마나 지났는지의 비율로
	// RenderState 갱신을 위한 PhysicState 보간 계수를 설정한다.
	// 게임 객체의 update 함수에 전달된다.
	const auto tPhysicInterpolation = physicUpdateAcc_ / effectiveInterval;

	// 게임 객체들 갱신
	// ground_->update(deltaTime, tPhysicInterpolation);
	player_->update(simulationDeltaTime, tPhysicInterpolation );

	for ( auto& obj : otherPlayers_ ) {
		obj->netInterpAcc_ += simulationDeltaTime;
		const float tNet = std::min(obj->netInterpAcc_ / obj->netInterpDuration_, 1.f);

		// 패킷 2개 간격(100ms) 이상 새 패킷이 없으면 멈춘 것으로 확정.
		// 1개 간격(50ms)이면 정상 패킷 도착 타이밍과 겹쳐 oscillation이 발생하므로 2배로 여유를 준다.
		if ( obj->netInterpAcc_ >= obj->netInterpDuration_ * 2.f ) {
			obj->setVelocity(mu::Vec3{});
		}

		obj->update( simulationDeltaTime, tNet );
	}

	// Monsters are server-position-driven (S_Move) just like remote players, so they use
	// the same NETWORK interpolation, not the physics-step clock. tNet ramps 0->1 over one
	// move interval and clamps at 1, holding at curr when moves stop — this is what prevents
	// the prev<->curr oscillation (sink/reappear) the physics clock caused for idle/sparse
	// monsters. (Same shape as the remote-player loop above.)
	auto updateMonstersNet = [&](auto& container) {
		for (auto& m : container) {
			m->netInterpAcc_ += simulationDeltaTime;
			const float tNet = std::min(m->netInterpAcc_ / m->netInterpDuration_, 1.f);
			if (m->netInterpAcc_ >= m->netInterpDuration_ * 2.f)
				m->setVelocity(mu::Vec3{});   // no new packet for 2 intervals -> stop drift
			m->update(simulationDeltaTime, tNet);
		}
	};
	updateMonstersNet(goblins_);
	updateMonstersNet(snakes_);
	updateMonstersNet(mushrooms_);
	updateMonstersNet(bombers_);
	updateMonstersNet(birdys_);
	updateMonstersNet(slimes_);
	updateMonstersNet(treants_);
	updateMonstersNet(bosses_);

	for (auto& sh : strongholds_) {
		sh->update(simulationDeltaTime, tPhysicInterpolation);
	}

	updateItemDrops(deltaTime, tPhysicInterpolation);

	animSystem_.updatePriorities(
		std::chrono::duration_cast<Seconds>(deltaTime),
		player_->pos()
	);

	// 지형 청크 스트리밍 틱 (카메라 갱신 전에 호출).
	chunkManager_.update(player_->pos(), deltaTime);

	// 연출 존 판정 (로컬 예측 플레이어 위치 기준, 패킷 없음).
	clientZoneSystem_.update(player_->pos());

	// Bind the absorb -> HUD-charge callback once.
	if (!orbSystem_.onAbsorb) {
		orbSystem_.onAbsorb = [this](const EnergyOrbSystem::Orb& orb) {
			skillDial_.addDisplayCharge(orb.slot, orb.chargePerOrb);
			// M5: emissive ripple across the local player's body at the contact point.
			// The orb color is HDR (bright); GB2 is UNORM so it would clamp to a vivid,
			// busy full-saturation ring. Soften it: normalize the hue to peak 1, mix
			// toward white (pastel), and feed a modest intensity -> a subtle wash.
			if (player_) {
				const auto cf   = orb.colorHDR.getXmf();
				const float peak = std::max({ cf.x, cf.y, cf.z, 1e-4f });
				mu::Vec3 soft = orb.colorHDR * (1.f / peak);                  // hue, peak = 1
				soft = mu::lerp(soft, mu::Vec3{ 1.f, 1.f, 1.f }, 0.3f);       // desaturate
				player_->addBodyRipple(orb.contactPoint, soft, 0.25f);        // gentle intensity
			}
		};
	}

	// Energy orb death FX: advance orbs (tracking + absorption) toward the live player.
	// Charge credits are matched to corpses in updateCorpses(); see below.
	orbSystem_.update(
		std::chrono::duration<float>(simulationDeltaTime).count(),
		player_->pos());

	// DEBUG fallback: if no "PathPt" markers were authored, synthesize a winding sample
	// path at the local player so the effect is visible online right away. Auto-disables
	// as soon as a real path exists (build() from markers makes hasPaths() true).
	if (!pathGuide_.hasPaths())
		pathGuide_.buildSamplePath(player_->pos(), player_->forward());

	// Path guidance: advance the ribbon window + guiding wisp (re-conforms to ground).
	// Suppressed entirely during tactical-combat arenas (player is locked in anyway).
	pathGuide_.setSuppressed(localArenaPresentationZoneId_ >= 0);
	pathGuide_.update(std::chrono::duration<float>(deltaTime).count(), player_->pos(), chunkManager_);

	camera_.update(deltaTime);
	camera_.updateFocusCinematic(deltaTime);
	// 3D 오디오 리스너를 카메라에 맞춘다(공간 SFX 감쇠/패닝 기준).
	{
		const mu::Vec3 camEye = camera_.eye();
		INet::ClientApp::sound().setListener(camEye, camera_.at() - camEye, mu::Vec3(0.f, 1.f, 0.f));
	}
	dirLight_.update(deltaTime);
	dirLight_.updateCSMCascades(camera_.view(), camera_.proj(), assetConfigs_.cascade, assetConfigs_.shadowMap);

	// 애니메이션 업데이트
	animSystem_.update(
		Seconds{ 0.01f * camera_.focusCinematicTimeScale() });

	// Death migration: activate the ragdoll for monsters that died this frame, then
	// detach them into client-authored corpses so a server respawn can't cut the death
	// animation short. Collected first to avoid mutating the active vectors mid-iteration.
	// (finalXformData is valid here, right after animSystem_.update().)
	{
		std::vector<std::pair<std::shared_ptr<Object>, MonsterKind>> justDied;
		auto activateAndCollect = [&](const std::shared_ptr<Object>& objPtr, MonsterKind kind) {
			Object& g = *objPtr;
			if (!g.ragdollPendingActivation()) return;
			g.setRagdollPendingActivation(false);
			Ragdoll& rd = *g.ragdoll();
			if (!rd.isBuilt() || !g.animBlender() || !g.model()) return;
			rd.seedFromFinalXforms(
				g.animBlender()->finalXformData(), g.model()->skeleton, g.renderState().world);
			rd.buildPassengers(g.model()->skeleton, g.animBlender()->finalXformData());
			rd.activate(physicsWorld_);

			// Momentum hand-off + toppling kick + per-bone noise (see Ragdoll::applyDeathKick).
			rd.applyDeathKick(g.ragdollInitVelocity());
			g.setRagdollInitVelocity(mu::Vec3{});

			justDied.emplace_back(objPtr, kind);
		};

		for (auto& goblin   : goblins_)   activateAndCollect(goblin,   MonsterKind::Goblin);
		for (auto& snake    : snakes_)    activateAndCollect(snake,    MonsterKind::Snake);
		for (auto& mushroom : mushrooms_) activateAndCollect(mushroom, MonsterKind::Mushroom);
		for (auto& bomber   : bombers_)   activateAndCollect(bomber,   MonsterKind::Bomber);
		for (auto& birdy    : birdys_)    activateAndCollect(birdy,    MonsterKind::Birdy);
		for (auto& slime    : slimes_)    activateAndCollect(slime,    MonsterKind::Slime);
		for (auto& treant   : treants_)   activateAndCollect(treant,   MonsterKind::Treant);
		for (auto& boss     : bosses_)    activateAndCollect(boss,     MonsterKind::Boss);

		for (auto& [objPtr, kind] : justDied) {
			const u32t corpseId =
				migrateToCorpse(objPtr, kind, static_cast<uint16>(objPtr->getId()));
			if (finalScoreboardPending_ && kind == MonsterKind::Boss) {
				finalBossRewardCorpseId_ = corpseId;
				finalBossRewardCorpseTracked_ = true;
			}
		}
	}

	// Advance client-authored corpses: ragdoll hold -> orb dissolve -> pool return.
	updateCorpses(simulationDeltaTime, tPhysicInterpolation);
	if (finalScoreboardPending_
		&& !camera_.focusCinematicActive()
		&& finalBossRewardCorpseTracked_
		&& finalBossRewardOrbsSpawned_
		&& !orbSystem_.hasActiveOrbs(finalBossRewardCorpseId_)) {
		showFinalScoreboard();
	}

	// HP 바 위치 및 값 갱신
	{
		constexpr float kBarHalfWidth = 40.f;
		updatePlayerHpHudLayout();

		if (player_) {
			const int playerHp = player_->hp();
			const int playerMaxHp = std::max(1, player_->maxHp());
			if (playerHpBar_) {
				playerHpBar_->setProgress(static_cast<float>(playerHp) / static_cast<float>(playerMaxHp));
			}
			if (playerHpText_) {
				WCHAR hpText[32] = {};
				swprintf_s(hpText, L"%d / %d", playerHp, playerMaxHp);
				playerHpText_->setText(hpText);
			}
			if (playerNameText_) {
				playerNameText_->setText(partyDisplayName(static_cast<uint16>(player_->getId())));
			}
		}
		updatePartyHpHudValues();
		updateBossHpHud();

		for (auto& [id, entry] : otherPlayerHpBars_) {
			if (!entry.player || entry.player->hp() <= 0) {
				entry.hpBar->visible = false;
				continue;
			}
			const mu::Vec3 barWorldPos = entry.player->renderState().pos
				+ mu::Vec3{ 0.f, 2.5f, 0.f };
			float sx{}, sy{};
			const bool onScreen = worldToScreen(
				barWorldPos,
				camera_.view(), camera_.proj(),
				uiManager_.screenWidth(), uiManager_.screenHeight(),
				sx, sy
			);
			entry.hpBar->visible = onScreen;
			if (onScreen) {
				entry.hpBar->offsetX = UI::DimValue::px(uiManager_.screenToLayoutX(sx) - kBarHalfWidth);
				entry.hpBar->offsetY = UI::DimValue::px(uiManager_.screenToLayoutY(sy));
				entry.hpBar->setProgress(
					static_cast<float>(entry.player->hp()) /
					static_cast<float>(std::max(1, entry.player->maxHp()))
				);
			}
		}

		const float dtSec = std::chrono::duration<float>(deltaTime).count();
		updateNpcStatusIcons(dtSec);
		for (auto& [id, entry] : goblinHpBars_) {
			if (!entry.goblin || entry.goblin->hidden() || entry.goblin->hp() <= 0 || entry.goblin->maxHp() <= 0) {
				entry.hpBar->visible = false;
				entry.hpBarVisibleSeconds = 0.f;
				continue;
			}
			entry.hpBarVisibleSeconds = std::max(0.f, entry.hpBarVisibleSeconds - dtSec);
			if (entry.hpBarVisibleSeconds <= 0.f) {
				entry.hpBar->visible = false;
				continue;
			}
			const mu::Vec3 barWorldPos = entry.goblin->renderState().pos
				+ mu::Vec3{ 0.f, entry.worldYOffset, 0.f };
			float sx{}, sy{};
			const bool onScreen = worldToScreen(
				barWorldPos,
				camera_.view(), camera_.proj(),
				uiManager_.screenWidth(), uiManager_.screenHeight(),
				sx, sy
			);
			entry.hpBar->visible = onScreen;
			if (onScreen) {
				entry.hpBar->offsetX = UI::DimValue::px(uiManager_.screenToLayoutX(sx) - kBarHalfWidth);
				entry.hpBar->offsetY = UI::DimValue::px(uiManager_.screenToLayoutY(sy));
				entry.hpBar->setProgress(
					static_cast<float>(entry.goblin->hp()) /
					static_cast<float>(entry.goblin->maxHp())
				);
			}
		}

		auto updateMonsterHpBar = [&](std::unordered_map<uint16, MonsterHpEntry>& bars) {
			for (auto& [id, entry] : bars) {
				if (!entry.monster || entry.monster->hp() <= 0) {
					entry.hpBar->visible = false;
					entry.hpBarVisibleSeconds = 0.f;
					continue;
				}
				entry.hpBarVisibleSeconds = std::max(0.f, entry.hpBarVisibleSeconds - dtSec);
				if (entry.hpBarVisibleSeconds <= 0.f) {
					entry.hpBar->visible = false;
					continue;
				}
				const mu::Vec3 barWorldPos = entry.monster->renderState().pos
					+ mu::Vec3{ 0.f, entry.worldYOffset, 0.f };
				float sx{}, sy{};
				const bool onScreen = worldToScreen(
					barWorldPos,
					camera_.view(), camera_.proj(),
					uiManager_.screenWidth(), uiManager_.screenHeight(),
					sx, sy
				);
				entry.hpBar->visible = onScreen;
				if (onScreen) {
					entry.hpBar->offsetX = UI::DimValue::px(uiManager_.screenToLayoutX(sx) - kBarHalfWidth);
					entry.hpBar->offsetY = UI::DimValue::px(uiManager_.screenToLayoutY(sy));
					entry.hpBar->setProgress(
						static_cast<float>(entry.monster->hp()) /
						static_cast<float>(std::max(1, entry.monster->maxHp()))
					);
				}
			}
		};
		updateMonsterHpBar(snakeHpBars_);
		updateMonsterHpBar(mushroomHpBars_);
		updateMonsterHpBar(bomberHpBars_);
		updateMonsterHpBar(birdyHpBars_);
		updateMonsterHpBar(slimeHpBars_);
		updateMonsterHpBar(treantHpBars_);

		for (auto& [id, entry] : strongholdHpBars_) {
			if (!entry.obj || entry.obj->isDead() || entry.obj->maxHp() <= 0) {
				entry.hpBar->visible = false;
				entry.hpBarVisibleSeconds = 0.f;
				continue;
			}
			entry.hpBarVisibleSeconds = std::max(0.f, entry.hpBarVisibleSeconds - dtSec);
			if (entry.hpBarVisibleSeconds <= 0.f) {
				entry.hpBar->visible = false;
				continue;
			}
			const mu::Vec3 barWorldPos = entry.obj->renderState().pos
				+ mu::Vec3{ 0.f, entry.worldYOffset, 0.f };
			float sx{}, sy{};
			const bool onScreen = worldToScreen(
				barWorldPos,
				camera_.view(), camera_.proj(),
				uiManager_.screenWidth(), uiManager_.screenHeight(),
				sx, sy
			);
			entry.hpBar->visible = onScreen;
			if (onScreen) {
				entry.hpBar->offsetX = UI::DimValue::px(uiManager_.screenToLayoutX(sx) - kBarHalfWidth);
				entry.hpBar->offsetY = UI::DimValue::px(uiManager_.screenToLayoutY(sy));
				entry.hpBar->setProgress(
					static_cast<float>(entry.obj->hp()) /
					static_cast<float>(entry.obj->maxHp())
				);
			}
		}

		tacticalZoneIntro_.update(dtSec);
		tacticalDialogueOverlay_.update(dtSec);
		const bool dialogueWasActive = dialogueSystem_.active();
		dialogueSystem_.update(dtSec);
		if (dialogueWasActive != dialogueSystem_.active()) {
			applyCursorPolicy();
		}
		inventoryPanel_.update(dtSec);
		if (finalScoreboard_.isVisible()) {
			finalScoreboard_.update(dtSec);
			hideCombatHudForFinalScoreboard();
		}
		uiManager_.layout();
		uiManager_.update(std::chrono::duration<float>(deltaTime).count(), gfx_, gfx_.defaultFont());
	}

	// particle update
	if (player_) {
		flameParticleSystem_.update(deltaTime);
		smokeParticleSystem_.update(deltaTime);
		bloodEffect_.update(simulationDeltaTime);
		swordSlash1Effect_.update(simulationDeltaTime);
		swordSlash7Effect_.update(simulationDeltaTime);
		swordSlashComboEffect_.update(simulationDeltaTime);
		slashWaveEffect_.update(simulationDeltaTime);
		spikesAttackEffect_.update(simulationDeltaTime);
		piercingEffect_.update(simulationDeltaTime);
		piercingMultiEffect_.update(simulationDeltaTime);
		piercingSlashEffect_.update(simulationDeltaTime);
		piercingCircleSlashEffect_.update(simulationDeltaTime);
		crystalsFrontAttackEffect_.update(simulationDeltaTime);
		aoESlashGreenEffect_.update(simulationDeltaTime);
		crystalsCrossFadeEffect_.update(simulationDeltaTime);
		earthSpikeWarnEffect_.update(simulationDeltaTime);
		earthSpikeEffect_.update(simulationDeltaTime);
		redEnergyExplosionEffect_.update(simulationDeltaTime);
		arrowEffect_.update(simulationDeltaTime);
		arrowVolleyMuzzleEffect_.update(simulationDeltaTime);
		arrowVolleyEffect_.update(simulationDeltaTime);
		arrowRainMuzzleEffect_.update(simulationDeltaTime);
		arrowRainEffect_.update(simulationDeltaTime);
		energyExplosionArrowEffect_.update(simulationDeltaTime);
		tornadoShotEffect_.update(simulationDeltaTime);
		tornadoMuzzleEffect_.update(simulationDeltaTime);
		tornadoHitEffect_.update(simulationDeltaTime);
		dustParticleSystem_.update(deltaTime);
		debugBVView_.update(deltaTime);

		if ( tornadoShotActive_ ) {
			constexpr float kSpeed    = 10.f;
			constexpr float kDuration = 0.8f;
			const Seconds dt = simulationDeltaTime;
			tornadoShotElapsed_ += dt;
			tornadoShotPos_ = tornadoShotPos_ + tornadoShotDir_ * kSpeed * dt.count();
			tornadoShotEffect_.setOrigin( tornadoShotPos_, tornadoShotOrient_ );
			if ( tornadoShotElapsed_.count() >= kDuration ) {
				tornadoShotEffect_.stop();
				tornadoHitEffect_.play( tornadoShotPos_ );
				tornadoShotActive_  = false;
				tornadoShotElapsed_ = 0s;
			}
		}

		// foot dust emit
		if (footBoneIdxLeft_ >= 0 && footBoneIdxRight_ >= 0
			&& player_->renderState().animBlender) {
			auto* animBlender = static_cast<AnimBlenderPlayer*>(
				player_->renderState().animBlender.get());

			const auto vel = player_->velocity();
			const float hSpeed2 = vel.x() * vel.x() + vel.z() * vel.z();
			constexpr float kDustMinSpeed = 1.0f;

			if (animBlender->isRunning() && hSpeed2 >= kDustMinSpeed * kDustMinSpeed) {
				const auto duration  = animBlender->runDuration();
				const auto currTime  = animBlender->runAnimTime();
				const float currPhase = currTime / duration;
				const float prevPhase = prevAnimTimeRun_ / duration;

				constexpr float kLeftFootContact  = 0.0f;
				constexpr float kRightFootContact = 0.5f;

				auto crossedPhase = [&](float phase) -> bool {
					if (currPhase >= prevPhase)
						return prevPhase < phase && currPhase >= phase;
					else
						return prevPhase < phase || currPhase >= phase;
				};

				const auto& skeleton   = player_->model()->skeleton;
				const auto& boneXforms = animBlender->finalXformData();
				const auto& world      = player_->renderState().world;

				auto getBoneWorldPos = [&](int boneIdx) -> mu::Vec3 {
					const auto& bone = skeleton.bones->at(boneIdx);
					const mu::Mat4x4 boneToWorld = bone.toDress * boneXforms[boneIdx] * world;
					return mu::Vec3(mu::Vec4(0.f, 0.f, 0.f, 1.f) * boneToWorld);
				};

				if (crossedPhase(kLeftFootContact)) {
					dustParticleSystem_.config().shape.position = getBoneWorldPos(footBoneIdxLeft_);
					dustParticleSystem_.emit(4);
				}
				if (crossedPhase(kRightFootContact)) {
					dustParticleSystem_.config().shape.position = getBoneWorldPos(footBoneIdxRight_);
					dustParticleSystem_.emit(4);
				}

				prevAnimTimeRun_ = currTime;
			} else {
				prevAnimTimeRun_ = 0s;
			}
		}
	}

	clearEvents(eventList_);

	INet::ClientApp::send();
}

void Game::submitBossHeatSources() {
	constexpr float kHeatFadeIn  = 0.8f;   // spawn ramp 0 -> 1 (seconds)
	constexpr float kHeatFadeOut = 1.2f;   // death ramp 1 -> 0 (seconds)

	static const auto heatT0 = std::chrono::steady_clock::now();
	const float now = std::chrono::duration<float>(std::chrono::steady_clock::now() - heatT0).count();

	++heatFrame_;

	// Liveness: a boss still present in idMonsterMap_ is alive; refresh its pivot + stamps.
	for (const auto& [id, obj] : idMonsterMap_) {
		if (!obj) continue;
		auto it = bossHeatProfiles_.find(id);
		if (it == bossHeatProfiles_.end()) continue;
		auto& st = it->second;
		if (st.bornSec < 0.f) st.bornSec = now;
		st.lastPos       = obj->pos();
		st.lastSeenStamp = heatFrame_;
		st.lastSeenSec   = now;
	}

	// Per-source amplitudes are encoded per boss; global multipliers stay at 1.0.
	gfx_.setHeatGlobals(now, 1.0f, 1.0f);

	const mu::Mat4x4 view = camera_.view();
	const mu::Mat4x4 proj = camera_.proj();
	const auto  pm  = proj.getXmf();
	const float p00 = pm._11;   // 0.5 * worldRadius * p00 / viewZ -> horizontal UV radius
	const float p11 = pm._22;   // ditto vertical

	for (auto it = bossHeatProfiles_.begin(); it != bossHeatProfiles_.end(); ) {
		auto& st = it->second;
		const bool aliveNow = (st.lastSeenStamp == heatFrame_);

		float env;
		if (aliveNow) {
			const float fadeIn = (st.bornSec < 0.f)
				? 0.f : std::clamp((now - st.bornSec) / kHeatFadeIn, 0.f, 1.f);
			env = st.intensity * fadeIn;
		} else {
			if (st.bornSec < 0.f) { it = bossHeatProfiles_.erase(it); continue; }   // never shown
			const float deathAge = now - st.lastSeenSec;
			if (deathAge >= kHeatFadeOut) { it = bossHeatProfiles_.erase(it); continue; }
			env = st.intensity * (1.f - std::clamp(deathAge / kHeatFadeOut, 0.f, 1.f));
		}
		if (env <= 1e-3f) { ++it; continue; }

		// Project the (raised) halo center to screen UV; reject when behind/way off-screen.
		const mu::Vec3 center = st.lastPos + mu::Vec3(0.f, st.heightBias, 0.f);
		const mu::Vec4 clip   = mu::Vec4(center, 1.f) * (view * proj);
		if (clip.w() <= 0.05f) { ++it; continue; }
		const float ndcX = clip.x() / clip.w();
		const float ndcY = clip.y() / clip.w();
		if (std::fabs(ndcX) > 1.8f || std::fabs(ndcY) > 1.8f) { ++it; continue; }
		const float centerU = ndcX * 0.5f + 0.5f;
		const float centerV = 0.5f - ndcY * 0.5f;

		// Boss pivot linear view-space Z (matches GB4) for depth gating + radius projection.
		const mu::Vec4 viewPos = mu::Vec4(st.lastPos, 1.f) * view;
		const float viewZ = viewPos.z();
		if (viewZ <= 0.1f) { ++it; continue; }
		const float invZ = 1.f / viewZ;

		const float rUx = 0.5f * st.worldRadius * p00 * invZ;
		const float rUy = 0.5f * st.worldRadius * p11 * invZ * st.aspectY;
		const float depthMargin = st.worldRadius;   // view-Z units

		HeatDistortionShader::HeatSource s{};
		s.centerRadius     = XMFLOAT4(centerU, centerV, rUx, rUy);
		s.zMarginIntensity = XMFLOAT4(viewZ, depthMargin, env, st.shimmerSpeed);
		s.tint             = XMFLOAT4(st.tint.x(), st.tint.y(), st.tint.z(), st.warpAmp);
		gfx_.addHeatSource(s);

		++it;
	}
}

void Game::renderInGame() {
	if (skipNextRender_) {
		skipNextRender_ = false;
		return;
	}

	if (player_ == nullptr) {
		return;
	}

	cullObjects();

	skybox_.render( gfx_ );
	// ground_->render(gfx_);
	player_->render(gfx_);

	for ( auto& obj : otherPlayers_ ) {
		obj->render( gfx_ );
	}

	for (auto& goblin   : goblins_)   goblin->render(gfx_);
	for (auto& snake    : snakes_)    snake->render(gfx_);
	for (auto& mushroom : mushrooms_) mushroom->render(gfx_);
	for (auto& bomber   : bombers_)   bomber->render(gfx_);
	for (auto& birdy    : birdys_)    birdy->render(gfx_);
	for (auto& slime    : slimes_)    slime->render(gfx_);
	for (auto& treant   : treants_)   treant->render(gfx_);
	for (auto& boss     : bosses_)    boss->render(gfx_);

	// Client-authored corpses render their ragdoll mesh until they dissolve into orbs
	// (orb phase is drawn by orbSystem_.submitDrawEvents).
	for (auto& c : corpses_)
		if (c.phase == Corpse::Phase::Ragdoll && c.obj) c.obj->render(gfx_);

	for (auto& sh : strongholds_) {
		if (sh->isDead()) continue;   // hide destroyed structure (isDead set by EvDeath)
		sh->render(gfx_);
	}

	// 월드 드롭 보석 + 조준 대상의 실루엣 강조.
	{
		gfx_.addCameraData(OutlinePipeline::CameraData{ camera_.view(), camera_.proj() });
		gfx_.addFrameData(OutlinePipeline::FrameData{
			static_cast<float>(gClientRect.right - gClientRect.left),
			static_cast<float>(gClientRect.bottom - gClientRect.top) });

		for (auto& [dropId, drop] : gemDrops_) {
			if (!drop.obj) continue;
			drop.obj->render(gfx_);
			if (dropId != aimedDropId_) continue;

			// Inverted hull은 SceneColorHDR에 가산 합성된다 — 색을 HDR 범위로 내보내면
			// 뒤따르는 bloom이 테두리를 발광으로 만든다.
			const Model* model = drop.obj->model();
			if (!model) continue;
			const mu::Mat4x4 world = drop.obj->renderState().world;
			for (const auto& mwx : model->meshWithDressXforms) {
				for (const auto& sub : mwx.mesh.subMeshes) {
					gfx_.addDrawEvent(OutlinePipeline::DrawEvent{
						.world       = mwx.dressXform * world,
						.pMesh       = &mwx.mesh,
						.pSubMesh    = &sub,
						.color       = { 2.6f, 2.1f, 0.8f, 1.f },
						.thicknessPx = 3.0f,
					});
				}
			}
		}
	}

	camera_.updateGFX(gfx_);
	dirLight_.render(gfx_);
	submitBossHeatSources();
	renderBarrierMagicCircleQuads();

	flameParticleSystem_.render(gfx_);
	smokeParticleSystem_.render(gfx_);
	bloodEffect_.render(gfx_);
	swordSlash1Effect_.render(gfx_);
	swordSlash7Effect_.render(gfx_);
	swordSlashComboEffect_.render(gfx_);
	slashWaveEffect_.render(gfx_);
	spikesAttackEffect_.render(gfx_);
	piercingEffect_.render(gfx_);
	piercingMultiEffect_.render(gfx_);
	piercingSlashEffect_.render(gfx_);
	piercingCircleSlashEffect_.render(gfx_);
	crystalsFrontAttackEffect_.render(gfx_);
	aoESlashGreenEffect_.render(gfx_);
	crystalsCrossFadeEffect_.render(gfx_);
	earthSpikeWarnEffect_.render(gfx_);
	earthSpikeEffect_.render(gfx_);
	redEnergyExplosionEffect_.render(gfx_);
	arrowEffect_.render(gfx_);
	arrowVolleyMuzzleEffect_.render(gfx_);
	arrowVolleyEffect_.render(gfx_);
	arrowRainMuzzleEffect_.render(gfx_);
	arrowRainEffect_.render(gfx_);
	energyExplosionArrowEffect_.render(gfx_);
	tornadoShotEffect_.render(gfx_);
	tornadoMuzzleEffect_.render(gfx_);
	tornadoHitEffect_.render(gfx_);
	dustParticleSystem_.render(gfx_);
	orbSystem_.submitDrawEvents(gfx_);

	// Path guidance: HDR ribbon (pre-bloom trail) + guiding wisp (free orb).
	pathGuide_.submitDrawEvents(gfx_, assetManager_.trail62Tex(), &orbProxyMesh_);
	debugBVView_.render(gfx_);

	auto frameDataPBR = PBRPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData(frameDataPBR);
	auto frameDataPBRSkinned = PBRSkinnedPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData(frameDataPBRSkinned);
	auto frameDataPBRDeferred = PBRDeferredPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData( frameDataPBRDeferred );
	auto frameDataPBRDeferredSkinned = PBRDeferredSkinnedPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData( frameDataPBRDeferredSkinned );

	if (!chunkManager_.empty()) {
		chunkManager_.setCullCamera(extractFrustum(camera_.view() * camera_.proj()), camera_.eye());
		chunkManager_.submitDrawEvents(gfx_, dirLight_);
		gfx_.addFrameData(TerrainPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
		gfx_.addFrameData(TerrainDeferredPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });

		// Minimap background cache. The texture covers a fixed kMinimapCoverageWorld-sized
		// square centered on the player; the HUD scrolls it via a per-frame UV sub-rect. It is
		// a single shared RT, re-baked only when needed: on a chunk load/unload (new terrain),
		// or when the player drifts > kMinimapRebakeMoveThreshold from the baked center (so the
		// view never reaches the texture edge). Coverage is sized to the view (not the 200m
		// chunk) to keep enough px/m for splat + prop detail.
		{
			const mu::Vec3 p = player_->pos();
			const float dx = p.x() - minimapBakedCenter_.x();
			const float dz = p.z() - minimapBakedCenter_.z();
			const bool moved = (dx * dx + dz * dz) >
				(GFX::kMinimapRebakeMoveThreshold * GFX::kMinimapRebakeMoveThreshold);
			const bool firstBake = (minimapBakedCoverage_ <= 0.f);
			if (chunkManager_.minimapDirty() || moved || firstBake) {
				chunkManager_.clearMinimapDirty();
				minimapBakedCenter_   = mu::Vec3(p.x(), 0.f, p.z());
				minimapBakedCoverage_ = GFX::kMinimapCoverageWorld;

				gfx_.requestMinimapRebake();
				chunkManager_.submitMinimapDrawEvents(gfx_);
				chunkManager_.submitMinimapPropDrawEvents(gfx_, minimapBakedCenter_, minimapBakedCoverage_);
				const float H = minimapBakedCoverage_ * 0.5f;
				gfx_.setMinimapCamera(MinimapTerrainPipeline::CameraData{
					.view = mu::lookAt(
						minimapBakedCenter_ + mu::Vec3(0.f, minimapBakedCoverage_ * 4.f, 0.f),
						minimapBakedCenter_, mu::NVec3(0.f, 0.f, 1.f)),
					.proj = mu::ortho(-H, H, -H, H, 0.1f, minimapBakedCoverage_ * 8.f)
				});
			}
		}
	}

	if (!finalScoreboard_.isVisible()) {
		// Floating damage numbers: drawn just before the HUD so they share the UI pass
		// (always-on-top, world-anchored via worldToScreen, screen-uniform size).
		damageNumberSystem_.render(gfx_, camera_, uiManager_.screenWidth(), uiManager_.screenHeight(), uiManager_.uiScale());

		// Skill dial + combo are in-game HUD and must sit BELOW the settings panel,
		// which is an overlay mounted on uiManager_. UI draw order = submission order,
		// so submit the dial first; uiManager_.render() (below) then draws the panel's
		// scrim/popup on top when it is open.
		skillDial_.render(gfx_,
			static_cast<float>(gClientRect.right - gClientRect.left),
			static_cast<float>(gClientRect.bottom - gClientRect.top));

		// Minimap: self (green) + party (blue) from idPlayerMap_, monsters (red) /
		// boss & mid-boss (orange, dynamic_cast against Boss/Grandbaum/Isys) from idMonsterMap_.
		minimapIcons_.clear();
		minimapIcons_.push_back(MinimapEntityIcon{ player_->pos(), MinimapEntityIcon::Kind::Self });
		for (const auto& [id, p] : idPlayerMap_) {
			if (id == static_cast<uint16>(player_->getId()) || !p) continue;
			minimapIcons_.push_back(MinimapEntityIcon{ p->pos(), MinimapEntityIcon::Kind::Party });
		}
		for (const auto& [id, obj] : idMonsterMap_) {
			if (!obj) continue;
			const bool isBossLike = bossNpcIds_.count(id) != 0;
			minimapIcons_.push_back(MinimapEntityIcon{
				obj->pos(), isBossLike ? MinimapEntityIcon::Kind::Boss : MinimapEntityIcon::Kind::Monster
			});
		}
		// Path-guidance overlay: route polyline + off-map edge arrow toward the look-ahead.
		minimapGuidePoly_.clear();
		pathGuide_.activePathPoints(minimapGuidePoly_);
		const MinimapGuide mmGuide{
			pathGuide_.guidanceActive(),
			std::span<const mu::Vec3>(minimapGuidePoly_),
			pathGuide_.guidanceTargetWorld()
		};
		minimap_.render(gfx_, player_->pos(), minimapBakedCenter_, minimapBakedCoverage_, minimapIcons_,
			static_cast<float>(gClientRect.right - gClientRect.left),
			static_cast<float>(gClientRect.bottom - gClientRect.top),
			mmGuide);

		// On-screen destination indicator: beacon when the look-ahead is on screen, an
		// edge arrow (+distance) when it is off screen / behind the camera.
		pathGuideHUD_.render(gfx_, camera_.view(), camera_.proj(),
			pathGuide_.guidanceTargetWorld(), pathGuide_.distanceToGoal(), pathGuide_.guidanceActive(),
			static_cast<float>(gClientRect.right - gClientRect.left),
			static_cast<float>(gClientRect.bottom - gClientRect.top));

	// 조준된 보석: 상호작용 안내 + 습득 실패 안내.
	{
		const float sw = static_cast<float>(gClientRect.right - gClientRect.left);
		const float sh = static_cast<float>(gClientRect.bottom - gClientRect.top);
		if (const auto it = gemDrops_.find(aimedDropId_);
			aimedDropId_ != 0 && it != gemDrops_.end() && it->second.obj) {
			std::wstring label = L"[F] 줍기";
			if (const ItemDefinition* def = itemCatalog_.find(it->second.itemId))
				label += L" · " + gemUtf8ToWide(def->name);
			pickupPromptHUD_.render(gfx_, camera_.view(), camera_.proj(),
				it->second.obj->pos(), label, true, sw, sh);
		}
		if (pickupNoticeSec_ > 0.f)
			pickupPromptHUD_.renderNotice(gfx_, pickupNotice_,
				std::min(1.f, pickupNoticeSec_ / 0.4f), sw, sh);
	}

	// Combo counter above the dial: kill-streak accelerator feedback. Shown while
	// an active combo (>=2) is within its window; size eases down as it expires.
	if (skillDial_.visible() && comboCount_ >= 2 && comboSecLeft_ > 0.f) {
		const float sw = static_cast<float>(gClientRect.right - gClientRect.left);
		const float sh = static_cast<float>(gClientRect.bottom - gClientRect.top);
		const float frac = (comboWindowMs_ > 0.f)
			? std::clamp(comboSecLeft_ / (comboWindowMs_ / 1000.f), 0.f, 1.f) : 1.f;
		DigitAtlas::emitNumber(gfx_, assetManager_.digitAtlasTex(),
			sw - 96.f, sh - 232.f, 30.f + frac * 12.f, sh,
			static_cast<int>(comboCount_), XMFLOAT4{ 1.f, 0.55f, 0.18f, 1.f },
			DigitAtlas::Align::Center);
	}

	uiManager_.render(gfx_);
	lobbyReturnFade_.render(
		gfx_, uiManager_.screenWidth(), uiManager_.screenHeight());

	static const auto uiT0 = std::chrono::steady_clock::now();
	const float uiTimeSec = std::chrono::duration<float>(
		std::chrono::steady_clock::now() - uiT0).count();

	auto frameDataUI = UIPipeline::FrameData{
		.screenWidth = static_cast<float>( gClientRect.right - gClientRect.left ),
		.screenHeight = static_cast<float>( gClientRect.bottom - gClientRect.top ),
		.time = uiTimeSec
	};
	gfx_.addFrameData(frameDataUI);

	gfx_.render();
	feedbackCullResultToAnim();
}

// ===========================================================================
// Lobby Scene
// ===========================================================================

void Game::enterLobby() {
	// RoomServer 경기 객체는 LobbyServer 세션보다 수명이 짧다. 로비 UI를 다시 만들기
	// 전에 이전 경기의 id 맵과 소유 컨테이너를 함께 비워야 다음 방의 재사용 id가
	// create* 중복 방지에 걸리지 않는다.
	resetInGameSession();

	camera_.cancelFocusCinematic();
	finalScoreboardPending_ = false;
	finalBossRewardCorpseId_ = 0u;
	finalBossRewardCorpseTracked_ = false;
	finalBossRewardOrbsSpawned_ = false;
	finalScoreboard_.hide();

	// Returning from gameplay leaves many HUD widgets mounted in the shared UI tree.
	// Hide the old tree first; LobbyUI/SettingsPanel rebuild their own roots below.
	uiManager_.resetInteractionState();
	for (const auto& child : uiManager_.root()->children()) {
		child->visible = false;
	}

	scene_      = Scene::Lobby;
	lobbyState_ = LobbyState::MainMenu;
	pendingStart_ = false;
	localArenaPresentationZoneId_ = -1;
	localPresentedArenaZoneIds_.clear();
	completedArenaZoneIds_.clear();
	hideBossHpHud();

	// 로비는 2D UI만 그린다. Deferred 라이팅 풀스크린 패스가 빈 GBuffer를 덮어쓰지 않도록
	// Forward 경로로 전환한다(클리어 + UI). 인게임 진입 시 Deferred로 복원.
	gfx_.setRenderPath(GFX::RenderPath::Forward);

	if (!uiBaseReady_) {
		uiManager_.setScreenSize(
			static_cast<float>(gClientRect.right - gClientRect.left),
			static_cast<float>(gClientRect.bottom - gClientRect.top)
		);
		uiManager_.requestDebugResources(gfx_);
		uiBaseReady_ = true;
	}

	// 로비 UI 텍스처(배경/로고)는 메인 스레드에서 즉시 로드한다.
	// (build가 이 텍스처를 위젯에 연결하므로 빌드 전에 로드해야 한다.)
	lobbyUI_.loadTextures(gfx_);
	lobbyUI_.build(uiManager_, makeLobbyCallbacks());
	// 현재 모니터에 맞는 창모드 해상도 목록 구성(설정창 빌드 전에 준비).
	rebuildAvailableResolutions();
	// 설정창은 씬 비종속 재사용 컴포넌트. 로비/인게임 어디서나 열 수 있도록
	// uiManager_.root() 직속에 빌드한다(로비 9-slice 텍스처 공유).
	settingsPanel_.build(uiManager_, lobbyUI_.panelTexture(),
		lobbyUI_.secondaryButtonTexture(), settings_, availableResolutions_,
		[]() { PostQuitMessage(0); });
	if (!inventoryUiReady_) {
		assetManager_.loadInventoryItemIcons(gfx_, itemCatalog_);
		inventoryPanel_.build(uiManager_, itemCatalog_, {
			.onAction = [this](uint8 slotIndex, InventoryAction action) {
				sendInventoryAction(slotIndex, action);
			}
		}, {
			.panelTexture = lobbyUI_.panelTexture(),
			.buttonTexture = lobbyUI_.secondaryButtonTexture(),
			.resolveItemIcon = [this](ItemId itemId) {
				return assetManager_.inventoryItemIcon(itemId);
			},
		});
		inventoryPanel_.setInventory(inventory_);
		inventoryUiReady_ = true;
	}
	inventoryPanel_.close();
	refreshLobbyUI();
	settingsOpenPrev_ = settingsPanel_.isOpen();
	applyCursorPolicy();

	// 모든 버튼 공용 클릭 효과음 훅을 1회 연결(UI 레이어와 사운드 백엔드 디커플링).
	UI::Button::sClickSfx = []() { INet::ClientApp::sound().playSfx("ui_click"); };

	// 로비 BGM 재생(이미 로비 트랙이면 no-op). 파일이 없으면 1회 경고만 남는다.
	INet::ClientApp::sound().playBgm("lobby");

	// 최소 로드로 로비 진입 후, 인게임 리소스를 백그라운드로 로드한다.
	startInGameAssetLoad();
}

void Game::startInGameAssetLoad() {
	if (inGameLoadStarted_) {
		return;
	}
	inGameLoadStarted_ = true;

	// 백그라운드 로드 동안 메인 스레드가 공유 로그 버퍼(gSharedLog)를
	// 동시에 건드리지 않도록, 실제 로드 구간에서만 dumpLog를 뮤트한다.
	muteLog();

	threadPool_.addJob([this]() {
		// Phase 1: minimum assets to render the waiting-room 3D (cube/player/skybox/anims).
		assetManager_.loadLobbyVisualAssets(gfx_, assetConfigs_);
		lobbyVisualAssetsLoaded_.store(true, std::memory_order_release);
		unmuteLog();

		// GFX load serialization: wait until the main thread finishes setupStageVisual()
		// (terrain GFX load) and sets stageVisualReady_, then start Phase 2 — prevents
		// LoadFence / command-list / descriptor overlap. If no room is ever created,
		// setupStageVisual() never runs, so we simply park here (in-game assets are only
		// needed once the user enters a room to play).
		while (!stageVisualReady_.load(std::memory_order_acquire)) {
			if (assetLoadAbort_.load(std::memory_order_relaxed)) { return; }
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}

		// Phase 2: remaining in-game assets (goblin/effects/materials), then particle prefetch.
		muteLog();
		assetManager_.loadRemainingInGameAssets(gfx_, assetConfigs_);
		inGameMeshAssetsLoaded_.store(true, std::memory_order_release);
		// 파티클 이펙트 JSON 파싱(디스크 I/O + 파싱)도 백그라운드에서 미리 수행해 캐시한다.
		prefetchParticleConfigs();
		// 로드 완료(모든 gSharedLog append 종료) 후 로그를 재개한다.
		// store(true) 이전에 해제하므로, 메인이 완료를 관측하는 시점엔 이미 단독 사용이 끝나 있다.
		unmuteLog();
		inGameAssetsLoaded_.store(true, std::memory_order_release);
	});
}

void Game::prefetchParticleConfigs() {
	namespace fs = std::filesystem;
	const fs::path effectsDir = "../resources/effects";

	std::error_code ec;
	auto it = fs::directory_iterator(effectsDir, ec);
	if (ec) {
		gSharedLog << "[Particle Prefetch] effects 디렉터리를 열 수 없습니다: " << effectsDir.string() << "\n";
		return;
	}

	// 파싱 대상(_ParticleSystems.json, 최대 ~11MB/개)을 먼저 수집한다.
	std::vector<fs::path> files;
	for (const auto& entry : it) {
		const auto& p = entry.path();
		if (p.extension() != ".json") continue;
		if (p.filename().string().find("_ParticleSystems") == std::string::npos) continue;
		files.push_back(p);
	}
	particleFilesTotal_.store(static_cast<u32t>(files.size()), std::memory_order_relaxed);
	particleFilesDone_.store(0u, std::memory_order_relaxed);
	if (files.empty()) {
		gSharedLog << "[Particle Prefetch] 0 files, 0 systems cached\n";
		return;
	}

	// 파일별 디스크 I/O + 파싱은 서로 독립적이므로 여러 스레드로 분산한다.
	// 주의: 공유 threadPool_ 에 하위 잡을 넣고 기다리면 안 된다 — 이 함수 자체가
	// threadPool_ 워커 위에서 실행되므로, 로딩 도중 앱이 종료되어 ThreadPool 이
	// done_=true 가 되면 큐에 남은 하위 잡이 실행되지 않아 영원히 대기하게 된다.
	// 그래서 prefetch 가 직접 소유하고 여기서 join 하는 워커를 쓴다(유한 종료 보장).
	// 각 워커는 자신의 결과를 전용 슬롯에 기록하므로 락이 필요 없다.
	std::vector<std::vector<std::pair<std::string, ps::ParticleSystemConfig>>> results(files.size());
	{
		const unsigned hw = std::max(2u, std::thread::hardware_concurrency());
		const std::size_t workerCnt = std::min<std::size_t>(files.size(), hw);
		std::atomic<std::size_t> nextFile{ 0 };

		auto parseWorker = [&]() {
			for (std::size_t i = nextFile.fetch_add(1, std::memory_order_relaxed);
			     i < files.size();
			     i = nextFile.fetch_add(1, std::memory_order_relaxed)) {
				// 로딩 중 종료 요청이 오면 남은 파일을 건너뛰고 즉시 빠져나온다.
				if (assetLoadAbort_.load(std::memory_order_relaxed))
					return;
				std::vector<ParticleImportSystem> systems;
				ParticleImportOptions opt;
				opt.logDiagnostics = false;
				if (!loadParticleSystemsFromUnityJson(files[i], systems, opt)) {
					particleFilesDone_.fetch_add(1, std::memory_order_relaxed);
					continue;
				}

				const std::string fileKey = files[i].filename().string();
				auto& out = results[i];
				out.reserve(systems.size());
				for (auto& s : systems)
					out.emplace_back(fileKey + "|" + s.relativePath, std::move(s.config));
				particleFilesDone_.fetch_add(1, std::memory_order_relaxed);
			}
		};

		std::vector<std::jthread> workers;
		workers.reserve(workerCnt);
		for (std::size_t w = 0; w < workerCnt; ++w)
			workers.emplace_back(parseWorker);
		// std::jthread 소멸 시 join — 이 블록을 벗어나면 모든 파싱이 완료된다.
	}

	int fileCnt = 0, sysCnt = 0;
	for (auto& fileResult : results) {
		if (!fileResult.empty()) ++fileCnt;
		for (auto& entry : fileResult) {
			particleConfigCache_[std::move(entry.first)] = std::move(entry.second);
			++sysCnt;
		}
	}
	gSharedLog << "[Particle Prefetch] " << fileCnt << " files, " << sysCnt << " systems cached\n";
}

float Game::loadProgress01() const {
	// Keyed on what is currently being waited on.
	if (!stageVisualReady_.load(std::memory_order_acquire)) {
		// Requirement 1: waiting for Phase 1 load + synchronous terrain init.
		if (!lobbyVisualAssetsLoaded_.load(std::memory_order_acquire))
			return (std::min)(gfx_.assetLoadFraction(), 0.9f);
		return 0.95f;  // Phase 1 done; holding through the terrain-init hitch.
	}
	if (!inGameMeshAssetsLoaded_.load(std::memory_order_acquire)) {
		// Terrain streaming is intentionally deferred while Phase 2 records on the shared
		// LoadFence, so the waiting-room cannot be visually complete before this point.
		return 0.95f;
	}
	const float terrainReady = chunkManager_.readyFractionAround(kLobbyCameraBaseAt);
	if (terrainReady < 1.f) {
		return 0.95f + 0.04f * terrainReady;
	}
	if (!inGameAssetsLoaded_.load(std::memory_order_acquire)) {
		// Requirement 3: waiting for Phase 2 (remaining meshes, then particle prefetch).
		const auto total = particleFilesTotal_.load(std::memory_order_relaxed);
		if (total == 0u) return 1.f;  // no files / dir access failure — avoid divide-by-zero
		const auto done = particleFilesDone_.load(std::memory_order_relaxed);
		return 0.5f + 0.5f * static_cast<float>(done) / static_cast<float>(total);
	}
	return 1.f;
}

void Game::refreshLobbyUI() {
	// 현재 씬/세션 상태로 ViewState 스냅샷을 만들어 lobbyUI_에 위임한다.
	LobbyUI::ViewState vs{};
	vs.inLobbyScene       = (scene_ == Scene::Lobby);
	vs.inMainMenu         = (lobbyState_ == LobbyState::MainMenu);
	vs.isAuthenticated    = isAuthenticated_;
	vs.waitingRoom3DReady = stageVisualReady_.load(std::memory_order_acquire);
	vs.isHost             = isHost_;
	vs.nickname           = myNickname_;
	vs.roomCode           = roomCode_;
	vs.maxPlayers         = kMaxLobbyPlayers;
	vs.players.reserve(lobbyPlayers_.size());
	for (const auto& p : lobbyPlayers_) {
		vs.players.push_back(LobbyUI::PlayerSlot{
			p.name,
			p.sessionId == hostId_,
			p.sessionId == myLobbyId_,
			p.weaponType
		});
	}
	lobbyUI_.refresh(vs);
	syncLobbyCharacterWeapons();

	// 메인 메뉴를 벗어나면 설정창은 닫는다(기존 동작 보존).
	if (!vs.inMainMenu) settingsPanel_.close();
}

LobbyUI::Callbacks Game::makeLobbyCallbacks() {
	LobbyUI::Callbacks cb{};
	cb.onLogin = [this](const std::wstring& id, const std::wstring& password) {
		lobbyLogin(id, password);
	};
	cb.onRegister = [this](const std::wstring& id, const std::wstring& password,
		const std::wstring& nickname) {
		lobbyRegister(id, password, nickname);
	};
	cb.onCreateRoom   = [this]() { lobbyCreateRoom(); };
	cb.onJoinRoom     = [this](const std::string& code) { lobbyJoinRoom(code); };
	cb.onLeaveRoom    = [this]() { lobbyLeaveRoom(); };
	cb.onStartGame    = [this]() { lobbyStartGame(); };
	cb.onCopyCode     = [this]() { gSharedLog << "[Lobby] 방 코드: " << roomCode_ << "\n"; };
	cb.onOpenSettings = [this]() { settingsPanel_.open(); };
	cb.onQuit         = []() { PostQuitMessage(0); };
	cb.onSelectWeapon = [this](int direction) { lobbySelectWeapon(direction); };
	return cb;
}

void Game::applyPendingDisplaySettings() {
	if (settings_.resolutionIndex == appliedResolutionIndex_
		&& settings_.fullscreen == appliedFullscreen_) {
		return;
	}

	applyDisplaySettings();

	appliedResolutionIndex_ = settings_.resolutionIndex;
	appliedFullscreen_      = settings_.fullscreen;
}

void Game::rebuildAvailableResolutions() {
	// 후보 창모드 해상도(오름차순). 현재 모니터에 들어가는 것만 노출한다
	// → FHD 모니터에선 2560×1440이 자동으로 숨겨지고, 더 큰 모니터에선 자동 노출된다.
	static constexpr Resolution kCandidates[] = {
		{ 1024, 768 }, { 1280, 720 }, { 1920, 1080 }, { 2560, 1440 }
	};

	int monW = 0, monH = 0;
	getCurrentMonitorSize(&monW, &monH);

	availableResolutions_.clear();
	for (const auto& c : kCandidates) {
		if (c.w <= monW && c.h <= monH) {
			availableResolutions_.push_back(c);
		}
	}
	// 안전장치: 모니터 정보를 못 얻었거나 모든 후보가 잘린 경우 최소 1개는 보장.
	if (availableResolutions_.empty()) {
		availableResolutions_.push_back(Resolution{ 1024, 768 });
	}
}

void Game::applyDisplaySettings() {
	// 현재 모니터 기준으로 창모드 해상도 목록을 갱신하고, 인덱스를 유효 범위로 클램프.
	rebuildAvailableResolutions();
	const int lastIdx = static_cast<int>(availableResolutions_.size()) - 1;
	settings_.resolutionIndex = std::clamp(settings_.resolutionIndex, 0, lastIdx);

	// 창모드 목표 해상도(전체화면이면 모니터 해상도를 쓰므로 무시된다).
	const int windowedW = availableResolutions_[settings_.resolutionIndex].w;
	const int windowedH = availableResolutions_[settings_.resolutionIndex].h;

	// 1. UIManager 입력 포인터 무효화 — 곧 위젯 트리를 재빌드하므로 dangling 방지.
	uiManager_.resetInteractionState();

	// 2. 창모드/전체화면(borderless) 전환 + 윈도우/전역 RECT 갱신. 결과 클라이언트 크기를 받는다.
	//    (gClientRect는 GFX 뷰포트/깊이버퍼가 읽는다.)
	int clientW = windowedW, clientH = windowedH;
	applyDisplayMode(settings_.fullscreen, windowedW, windowedH, &clientW, &clientH);

	// 3. 스왑체인 백버퍼/깊이/GBuffer/HiZ 재생성 (GPU idle 후).
	gfx_.resize(static_cast<u32t>(clientW), static_cast<u32t>(clientH));

	// 4. UI 화면 크기 갱신 + 픽셀 기반 UI(로비/설정창) 재빌드.
	//    (HUD/데미지 넘버는 앵커·월드 좌표 기반이라 setScreenSize + 매 프레임 worldToScreen로 추종.)
	uiManager_.setScreenSize(static_cast<float>(clientW), static_cast<float>(clientH));
	lobbyUI_.build(uiManager_, makeLobbyCallbacks());
	settingsPanel_.build(uiManager_, lobbyUI_.panelTexture(), lobbyUI_.secondaryButtonTexture(),
		settings_, availableResolutions_, []() { PostQuitMessage(0); });

	// 5. 로비 위젯 상태 복원 + 설정창 다시 열기(사용자가 설정창에서 변경 중이므로 유지).
	refreshLobbyUI();
	settingsPanel_.open();
	settingsOpenPrev_ = settingsPanel_.isOpen();
	applyCursorPolicy();
}

void Game::LobbyScene(Milliseconds deltaTime) {
	// 로비 네트워킹 펌핑: 수신 APC 처리 + 큐잉된 송신 flush (InGameScene와 동일한 단일 스레드 모델).
	SleepEx(1, true);
	INet::ClientApp::send();

	// RoomServer 핸드오프: onGameStart가 적재한 요청을, APC 밖이며 인게임 에셋 로드가 끝난 뒤에만 실행한다.
	// (비호스트는 로드 완료 전 S_GameStart를 받을 수 있다. 그 전엔 대기실에 머물고, RoomServer는
	//  이 클라의 C_Enter가 늦게 와도 그때 입장 처리한다.)
	if (pendingHandoff_ && inGameAssetsLoaded_.load(std::memory_order_acquire)) {
		pendingHandoff_ = false;
		INet::ClientApp::reconnectToRoomServer(handoffIp_, handoffPort_);
		INet::ClientApp::addSendBuffer(PacketManager::makeCEnterPacket(handoffTicket_, selectedLobbyWeapon_));
		INet::ClientApp::send();
		enterInGame();   // scene_=InGame → 이후 InGameScene가 펌핑, RoomServer의 S_Enter로 플레이어 생성
		return;
	}

	const bool loaded = inGameAssetsLoaded_.load(std::memory_order_acquire);

	// Start was pressed while still loading → enter in-game once assets are ready (req 3).
	if (pendingStart_ && loaded) {
		pendingStart_ = false;
		enterInGame();
		return;
	}

	if (loaded && !inGameAssetsReady_) {
		inGameAssetsReady_ = true;
		refreshLobbyUI();  // 게임 시작 버튼 활성 상태 갱신 (로그 재개는 워커가 수행)
	}

	// 대기실: Phase 1(대기실 3D 에셋) 로드 완료 후 setupStageVisual()로 3D를 준비한다.
	// setupStageVisual() 완료 시 stageVisualReady_가 서며, 이를 워커가 보고 Phase 2를 시작한다.
	if (lobbyState_ == LobbyState::WaitingRoom && lobbyVisualAssetsLoaded_.load(std::memory_order_acquire)) {
		setupStageVisual();   // idempotent (지형/스카이박스/라이트 1회 init)
		gfx_.setRenderPath(GFX::RenderPath::Deferred);
		lobbyUI_.setFlatBackgroundVisible(false);

		const float aspect = static_cast<float>(gClientRect.right - gClientRect.left)
			/ std::max(1.f, static_cast<float>(gClientRect.bottom - gClientRect.top));
		lobbyCamera_.setPerspective(mu::Degree(60.f), aspect, 0.1f, 1000.f);

		// 대기실 카메라 연출: standalone에서 잡은 로비 쇼케이스 위치를 기준으로 약하게 좌우 패닝한다.
		// 캐릭터 슬롯이 들어올 자리가 크게 흔들리지 않도록 eye 이동보다 at 이동을 작게 둔다.
		lobbyCameraTime_ += std::chrono::duration<float>(deltaTime).count();
		const float sway = std::sin(lobbyCameraTime_ * 0.25f) * 0.3f;
		const mu::Vec3 eye = kLobbyCameraBaseEye + mu::Vec3(sway, 0.f, 0.f);
		const mu::Vec3 at  = kLobbyCameraBaseAt  + mu::Vec3(sway * 0.25f, 0.f, 0.f);
		lobbyCamera_.setView(eye, at);
		// Terrain streaming also records on the shared GFX "LoadFence". While the worker is
		// running Phase 2 loadRequestedAssets, a concurrent recordTerrainResourceLoad here
		// would clear fence.associatedResources_ and delete the worker's in-flight upload
		// buffers (D3D12 OBJECT_DELETED_WHILE_STILL_IN_USE). The synchronous baseline terrain
		// loaded in setupStageVisual() already covers the near-static lobby camera, so defer
		// streaming until Phase 2 GPU loading is done.
		if (inGameMeshAssetsLoaded_.load(std::memory_order_acquire)) {
			chunkManager_.update(kLobbyCameraBaseAt, deltaTime);
		}

		// 전시 캐릭터: 최초 진입 시 생성 후, 현재 카메라 기준으로 재배치해 화면 슬롯 위치를 고정한다.
		// 매 프레임 update(idle 선택/월드 갱신) + 본 행렬 계산(animSystem).
		if (lobbyChars_.empty()) {
			setupLobbyCharacters();
		}
		updateLobbyCharacterTransforms();
		for (auto& ch : lobbyChars_) {
			ch->update(deltaTime, 1.f);
		}
		animSystem_.update(0.01s);
	}

	// Loading overlay: visible while the waiting-room 3D isn't ready (req 1) or while a
	// pending start waits for the remaining in-game assets (req 3).
	// stageVisualReady_가 서더라도 곧장 내리지 않고, renderWaitingRoom()이 오버레이 뒤에서
	// kWarmupFrames만큼 실제로 렌더를 돌릴 때까지 유지한다(지형/캐릭터/포트레이트 RT/GPU 업로드가
	// settle된 뒤 노출 → 첫 프레임이 그려지는 "팝인"을 가린다).
	constexpr int kWarmupFrames = 8;
	const bool stageReady = stageVisualReady_.load(std::memory_order_acquire);
	const bool waitingRoomTerrainReady =
		stageReady
		&& inGameMeshAssetsLoaded_.load(std::memory_order_acquire)
		&& chunkManager_.readyAround(kLobbyCameraBaseAt);
	if (!waitingRoomTerrainReady) {
		waitingRoomWarmupFrames_ = 0;
	}
	const bool waitingRoomWarm =
		waitingRoomTerrainReady && waitingRoomWarmupFrames_ >= kWarmupFrames;
	const bool showLoading =
		(lobbyState_ == LobbyState::WaitingRoom && !waitingRoomWarm)
		|| pendingStart_;
	lobbyUI_.updateLoading(std::chrono::duration<float>(deltaTime).count(), showLoading, loadProgress01());

	uiManager_.layout();
	uiManager_.update(std::chrono::duration<float>(deltaTime).count(), gfx_, gfx_.defaultFont());
}

void Game::renderLobby() {
	// 대기실 3D 준비 완료 시 맵 배경 + UI, 아니면(메인화면/대기실-로딩중) UI만.
	if (lobbyState_ == LobbyState::WaitingRoom && stageVisualReady_.load(std::memory_order_acquire)) {
		renderWaitingRoom();
		return;
	}

	uiManager_.render(gfx_);
	lobbyReturnFade_.render(
		gfx_, uiManager_.screenWidth(), uiManager_.screenHeight());

	auto frameDataUI = UIPipeline::FrameData{
		.screenWidth  = static_cast<float>(gClientRect.right - gClientRect.left),
		.screenHeight = static_cast<float>(gClientRect.bottom - gClientRect.top)
	};
	gfx_.addFrameData(frameDataUI);

	gfx_.render();
}

void Game::renderWaitingRoom() {
	// renderInGame()의 3D 부분 최소 복제: 스카이박스 + 지형 + 캐릭터 + 카메라 + 방향광.
	// (이펙트/HiZ/그림자 제외 — 그림자는 후속.)
	skybox_.render(gfx_);

	if (!chunkManager_.empty()) {
		chunkManager_.setCullCamera(extractFrustum(camera_.view() * camera_.proj()), camera_.eye());
		chunkManager_.submitDrawEvents(gfx_, dirLight_);
		gfx_.addFrameData(TerrainPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
		gfx_.addFrameData(TerrainDeferredPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
	}

	// 배경(지형/스카이박스)용 카메라 + 방향광.
	lobbyCamera_.updateGFX(gfx_);
	dirLight_.render(gfx_);

	gfx_.addFrameData(PBRPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
	gfx_.addFrameData(PBRSkinnedPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
	gfx_.addFrameData(PBRDeferredPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
	gfx_.addFrameData(PBRDeferredSkinnedPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });

	// ===== 슬롯 캐릭터: 오프스크린 포트레이트 RT로 분리 렌더 → UI 슬롯에 합성 =====
	// 배경 카메라와 무관하게 각 캐릭터를 슬롯 전용 카메라로 RT 셀에 그린다(투명 배경).
	const int filled = static_cast<int>(lobbyPlayers_.size());
	gfx_.setLobbyPortraitActive(true);
	gfx_.addLobbyPortraitFrameData(PBRSkinnedPipeline::FrameData{ .globalAmbient = mu::Vec3(0.20f, 0.20f, 0.22f), .iblIntensity = 0.92f });
	// 정면-상단 키 라이트(고정 방향, 배경광과 무관하게 캐릭터를 일관되게 비춤). shadow는 GFX에서 off.
	{
		const mu::NVec3 keyDir(-0.25f, -0.5f, -0.83f);
		PBRSkinnedPipeline::LightData keyLight{};
		keyLight.type      = PBRSkinnedPipeline::LightData::Type::DirectionalLight;
		keyLight.dir       = mu::Vec3(keyDir.x(), keyDir.y(), keyDir.z());
		keyLight.color     = (dirLight_.intensity > 0.f) ? dirLight_.color : mu::Vec3(1.f, 1.f, 1.f);
		keyLight.intensity = (dirLight_.intensity > 0.f) ? dirLight_.intensity : 3.f;
		keyLight.isMainDirectionalLight = false;
		gfx_.addLobbyPortraitLightData(keyLight);
	}

	const Texture* portraitTex = gfx_.lobbyPortraitTextureForThisFrame();
	for (int i = 0; i < kMaxLobbyPlayers; ++i) {
		const bool isFilled = (i < filled) && (i < static_cast<int>(lobbyChars_.size())) && lobbyChars_[i];
		if (isFilled) {
			gfx_.setLobbyPortraitCamera(static_cast<u32t>(i), PBRSkinnedPipeline::CameraData{
				.view = lobbyPortraitCams_[i].view(),
				.proj = lobbyPortraitCams_[i].proj(),
				.pos  = lobbyPortraitCams_[i].eye()
			});
			lobbyChars_[i]->renderPortrait(gfx_, static_cast<u32t>(i));
		}
		// UI 슬롯 이미지: 채워진 슬롯만 표시, 셀 sub-rect 샘플.
		if (auto* bay = lobbyUI_.slotBay(i)) {
			bay->visible = isFilled && (portraitTex != nullptr);
			if (bay->visible) {
				bay->texture     = portraitTex;
				bay->uvScaleBias = gfx_.lobbyPortraitCellUvScaleBias(static_cast<u32t>(i));
			}
		}
	}

	uiManager_.render(gfx_);
	lobbyReturnFade_.render(
		gfx_, uiManager_.screenWidth(), uiManager_.screenHeight());
	gfx_.addFrameData(UIPipeline::FrameData{
		.screenWidth  = static_cast<float>(gClientRect.right - gClientRect.left),
		.screenHeight = static_cast<float>(gClientRect.bottom - gClientRect.top)
	});

	gfx_.render();

	// 실제로 렌더된 대기실 프레임만 카운트(워밍업 게이트). 오버레이가 위를 덮고 있는 동안
	// 누적되며, kWarmupFrames에 도달하면 LobbyScene이 오버레이를 내린다. 포화 방지로 상한.
	if (waitingRoomWarmupFrames_ < 64) ++waitingRoomWarmupFrames_;
}

void Game::enterInGame() {
	// 닫힌 RoomServer의 이미 완료된 recv APC가 로비의 alertable wait에서 뒤늦게
	// 전달될 수 있다. reconnectToRoomServer가 retired session을 드레인한 다음인 이
	// 지점에서 한 번 더 정리해, 그 패킷이 복원한 이전 경기 객체도 제거한다.
	resetInGameSession();

	camera_.cancelFocusCinematic();
	finalScoreboardPending_ = false;
	finalBossRewardCorpseId_ = 0u;
	finalBossRewardCorpseTracked_ = false;
	finalBossRewardOrbsSpawned_ = false;
	lobbyReturnFade_.cancel();
	finalScoreboard_.hide();
	pendingStart_ = false;
	localArenaPresentationZoneId_ = -1;
	localPresentedArenaZoneIds_.clear();
	completedArenaZoneIds_.clear();
	hideBossHpHud();
	waitingRoomWarmupFrames_ = 0;  // 대기실 재입장 시 다시 워밍업하도록 리셋
	lobbyUI_.setLoadingVisible(false);
	lobbyUI_.setRootVisible(false);

	// 인게임은 Deferred 경로로 렌더한다(로비에서 Forward로 바꿔둔 것을 복원).
	gfx_.setRenderPath(GFX::RenderPath::Deferred);

	// 대기실 전시 캐릭터 정리: animSystem 트랙/shared_ptr를 제거해 인게임 씬으로 누수되지 않게 한다.
	clearLobbyCharacters();

	// setupStageVisual은 최초 한 번만 실행되므로 경기 단위 진행 상태는 여기서 다시 만든다.
	clientZoneSystem_.build(chunkManager_.zones());
	bindZoneHandlers();
	rebuildBarrierMagicCircleQuads();
	pathGuide_.build(chunkManager_.markers());

	std::string inventoryError;
	if (inventory_.initializeEmpty(itemCatalog_, &inventoryError))
		inventoryPanel_.setInventory(inventory_);
	else
		gSharedLog << "[Inventory] reset failed: " << inventoryError << "\n";
	inventoryPanel_.close();

	setupStage();
	scene_ = Scene::InGame;
	settingsOpenPrev_ = settingsPanel_.isOpen();
	applyCursorPolicy();

	// 인게임 BGM으로 크로스페이드.
	INet::ClientApp::sound().playBgm("ingame");

	// 플레이어/오브젝트 생성은 서버의 S_Enter 패킷이 담당한다.
	// (패킷은 InGameScene의 SleepEx(alertable)에서 처리되므로, 씬 전환 후 첫 프레임부터 수신된다.)
	// 여기서 별도로 플레이어를 만들면 S_Enter가 다시 setupPlayer를 호출해
	// 이전 플레이어의 물리 바디가 물리월드에 dangling으로 남아 크래시한다.
}

std::wstring Game::lobbyDisplayName(uint16 sessionId, const wchar_t* nickname) const {
	// 본인 슬롯은 계속 "나"로 표시한다 — 4칸 대기실에서 어느 쪽이 나인지가 닉네임보다 중요하다.
	if (sessionId == myLobbyId_) {
		return L"나";
	}
	// 로그인으로 확정된 계정 닉네임. 서버가 못 채운 경우에만 sessionId로 폴백한다.
	if (const size_t nickLen = nickname ? wcsnlen(nickname, kNicknameMax) : 0; nickLen > 0) {
		return std::wstring(nickname, nickLen);
	}
	return L"Player_" + std::to_wstring(sessionId);
}

namespace {

// 입력 가능 길이. protocol.hpp의 고정 배열 크기에서 널 종료 1자를 뺀 값이며,
// db/schema.sql의 NVARCHAR 길이와 짝이다.
constexpr size_t kLoginIdLenMax  = kLoginIdMax - 1;
constexpr size_t kPasswordLenMax = kPasswordMax - 1;
constexpr size_t kNicknameLenMax = kNicknameMax - 1;

// loginId/password는 wire 상 char[]다. 비ASCII가 섞이면 변환에서 깨지므로 아예 보내지 않는다.
// (닉네임은 wchar_t[]라 변환 없이 그대로 나간다 — 한글 가능.)
bool toAscii(const std::wstring& src, std::string& out) {
	out.clear();
	out.reserve(src.size());
	for (const wchar_t ch : src) {
		if (ch < 0x20 || ch > 0x7E) {
			return false;
		}
		out.push_back(static_cast<char>(ch));
	}
	return true;
}

const wchar_t* accountResultMessage(AccountResult result) {
	switch (result) {
	case AccountResult::InvalidInput:      return L"입력 형식이 올바르지 않습니다.";
	case AccountResult::DuplicateId:       return L"이미 사용 중인 아이디입니다.";
	case AccountResult::DuplicateNickname: return L"이미 사용 중인 닉네임입니다.";
	case AccountResult::NoSuchAccount:     return L"존재하지 않는 아이디입니다.";
	case AccountResult::WrongPassword:     return L"비밀번호가 일치하지 않습니다.";
	case AccountResult::AlreadyLoggedIn:   return L"이미 접속 중인 계정입니다.";
	case AccountResult::DbError:           return L"서버 오류입니다. 잠시 후 다시 시도해 주세요.";
	default:                               return L"알 수 없는 오류가 발생했습니다.";
	}
}

}   // namespace

void Game::lobbyLogin(const std::wstring& id, const std::wstring& password) {
	if (authPending_) {
		return;   // 응답 대기 중. 중복 전송하면 두 번째가 AlreadyLoggedIn으로 튕긴다.
	}

	if (id.empty() || password.empty()) {
		lobbyUI_.setMainMenuMessage(L"아이디와 비밀번호를 입력하세요.");
		return;
	}

	if (id.size() > kLoginIdLenMax || password.size() > kPasswordLenMax) {
		lobbyUI_.setMainMenuMessage(L"아이디는 " + std::to_wstring(kLoginIdLenMax)
			+ L"자, 비밀번호는 " + std::to_wstring(kPasswordLenMax) + L"자까지 입력할 수 있습니다.");
		return;
	}

	std::string asciiId, asciiPassword;
	if (!toAscii(id, asciiId) || !toAscii(password, asciiPassword)) {
		lobbyUI_.setMainMenuMessage(L"아이디와 비밀번호는 영문·숫자만 사용할 수 있습니다.");
		return;
	}

	authPending_ = true;
	INet::ClientApp::addSendBuffer(PacketManager::makeCLoginPacket(asciiId, asciiPassword));
	INet::ClientApp::send();
	lobbyUI_.setMainMenuMessage(L"로그인 중...", false);
	gSharedLog << "[Auth] 로그인 요청 전송\n";
}

void Game::lobbyRegister(const std::wstring& id, const std::wstring& password,
	const std::wstring& nickname) {
	if (authPending_) {
		return;
	}

	if (id.empty() || password.empty() || nickname.empty()) {
		lobbyUI_.setSignupMessage(L"아이디, 비밀번호, 닉네임을 모두 입력하세요.");
		return;
	}

	if (id.size() > kLoginIdLenMax || password.size() > kPasswordLenMax
		|| nickname.size() > kNicknameLenMax) {
		lobbyUI_.setSignupMessage(L"아이디 " + std::to_wstring(kLoginIdLenMax)
			+ L"자, 비밀번호 " + std::to_wstring(kPasswordLenMax)
			+ L"자, 닉네임 " + std::to_wstring(kNicknameLenMax) + L"자까지 입력할 수 있습니다.");
		return;
	}

	std::string asciiId, asciiPassword;
	if (!toAscii(id, asciiId) || !toAscii(password, asciiPassword)) {
		lobbyUI_.setSignupMessage(L"아이디와 비밀번호는 영문·숫자만 사용할 수 있습니다.");
		return;
	}

	authPending_       = true;
	pendingRegisterId_ = id;   // 가입 성공 응답에 loginId가 없어, 로그인 칸을 채우려면 필요하다.
	INet::ClientApp::addSendBuffer(
		PacketManager::makeCRegisterPacket(asciiId, asciiPassword, nickname));
	INet::ClientApp::send();
	lobbyUI_.setSignupMessage(L"회원가입 요청 중...");
	gSharedLog << "[Auth] 회원가입 요청 전송\n";
}

void Game::lobbyCreateRoom() {
	if (!isAuthenticated_) {
		lobbyUI_.setMainMenuMessage(L"로그인 후 이용할 수 있습니다.");
		gSharedLog << "[Lobby] 방 생성 차단: 로그인 필요\n";
		return;
	}

	// 방 생성 요청만 보낸다. 대기실 전환은 S_CreateRoom 수신(onLobbyCreated) 시 수행.
	INet::ClientApp::addSendBuffer(PacketManager::makeCCreateRoomPacket());
	INet::ClientApp::send();
	gSharedLog << "[Lobby] 방 생성 요청 전송\n";
}

void Game::lobbyJoinRoom(const std::string& code) {
	if (!isAuthenticated_) {
		lobbyUI_.setMainMenuMessage(L"로그인 후 이용할 수 있습니다.");
		gSharedLog << "[Lobby] 방 참가 차단: 로그인 필요\n";
		return;
	}

	// 코드 정규화: 대문자 + 영숫자만 (TextInput에서 이미 필터되지만 방어적으로 한 번 더).
	std::string norm;
	for (char c : code) {
		if (c >= 'a' && c <= 'z') c = static_cast<char>(c - ('a' - 'A'));
		const bool isAlnum = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
		if (isAlnum) norm += c;
	}

	if (norm.size() != 6) {
		lobbyUI_.setMainMenuMessage(L"6자리 코드를 입력하세요");
		gSharedLog << "[Lobby] 방 참가 실패: 잘못된 코드 길이\n";
		return;
	}

	// 참가 요청만 보낸다. 성공/실패 처리는 S_JoinRoom 수신(onLobbyJoined) 시 수행.
	INet::ClientApp::addSendBuffer(PacketManager::makeCJoinRoomPacket(norm));
	INet::ClientApp::send();
	gSharedLog << "[Lobby] 방 참가 요청 전송 (code=" << norm << ")\n";
}

void Game::lobbyLeaveRoom() {
	// 서버에 퇴장 통보 후 로컬 UI를 메인화면으로 되돌린다.
	INet::ClientApp::addSendBuffer(PacketManager::makeCLeaveRoomPacket());
	INet::ClientApp::send();

	roomCode_.clear();
	isHost_ = false;
	hostId_ = 0;
	myLobbyId_   = 0;
	selectedLobbyWeapon_ = PlayerWeaponType::Katana;
	lobbyPlayers_.clear();
	lobbyState_ = LobbyState::MainMenu;
	lobbyCameraTime_ = 0.f;
	pendingStart_ = false;
	waitingRoomWarmupFrames_ = 0;  // 대기실 재입장 시 다시 워밍업하도록 리셋
	lobbyUI_.setLoadingVisible(false);
	lobbyUI_.clearRoomCodeInput();
	lobbyUI_.setMainMenuMessage(L"");
	settingsPanel_.close();

	// 대기실 3D 배경 → 메인화면 복귀: Forward(UI-only) 경로 복원 + 키아트 bg 다시 표시.
	gfx_.setRenderPath(GFX::RenderPath::Forward);
	lobbyUI_.setFlatBackgroundVisible(true);

	// 전시 캐릭터 정리(animSystem 트랙 해제 포함) — 재입장 시 다시 생성된다.
	clearLobbyCharacters();

	refreshLobbyUI();
	applyCursorPolicy();
}

void Game::lobbyStartGame() {
	if (!isHost_) {
		gSharedLog << "[Lobby] 호스트만 시작할 수 있습니다.\n";
		return;
	}
	
	// 게임 시작 요청만 보낸다(C_GameStart). S_GameStart 수신 시 onGameStart에서 처리.
	INet::ClientApp::addSendBuffer(PacketManager::makeCGameStartPacket());
	INet::ClientApp::send();
	gSharedLog << "[Lobby] 게임 시작 요청 전송\n";
}

void Game::lobbySelectWeapon(int direction) {
	if (lobbyState_ != LobbyState::WaitingRoom || myLobbyId_ == 0) {
		return;
	}

	const int cur = std::clamp(static_cast<int>(selectedLobbyWeapon_), 0, 3);
	const int next = (cur + (direction >= 0 ? 1 : 3)) % 4;
	selectedLobbyWeapon_ = static_cast<PlayerWeaponType>(next);

	for (auto& p : lobbyPlayers_) {
		if (p.sessionId == myLobbyId_) {
			p.weaponType = selectedLobbyWeapon_;
			break;
		}
	}

	INet::ClientApp::addSendBuffer(PacketManager::makeCSelectWeaponPacket(selectedLobbyWeapon_));
	INet::ClientApp::send();
	refreshLobbyUI();
}

// --- LobbyServer 응답 핸들러 (메인 스레드 alertable 대기에서 호출) ---

void Game::onRegisterResult(AccountResult result) {
	authPending_ = false;

	if (result != AccountResult::Ok) {
		lobbyUI_.setSignupMessage(accountResultMessage(result));
		pendingRegisterId_.clear();
		gSharedLog << "[Auth] 회원가입 실패. result: " << static_cast<int>(result) << '\n';
		return;
	}

	uiManager_.resetInteractionState();
	lobbyUI_.completeRegistration(pendingRegisterId_);
	pendingRegisterId_.clear();
	gSharedLog << "[Auth] 회원가입 성공\n";
}

void Game::onLoginResult(AccountResult result, int64 accountId, const std::wstring& nickname) {
	authPending_ = false;

	if (result != AccountResult::Ok) {
		lobbyUI_.setMainMenuMessage(accountResultMessage(result));
		gSharedLog << "[Auth] 로그인 실패. result: " << static_cast<int>(result) << '\n';
		return;
	}

	accountId_       = accountId;
	myNickname_      = nickname;
	isAuthenticated_ = true;

	lobbyUI_.clearLoginPassword();
	lobbyUI_.setMainMenuMessage(L"");
	uiManager_.resetInteractionState();
	refreshLobbyUI();
	gSharedLog << "[Auth] 로그인 성공. accountId: " << accountId << '\n';
}

void Game::onLobbyCreated(const std::string& code, uint16 myId) {
	roomCode_ = code;
	myLobbyId_     = myId;
	hostId_   = myId;       // 생성자가 곧 호스트
	isHost_   = true;
	selectedLobbyWeapon_ = PlayerWeaponType::Katana;
	lobbyPlayers_.clear();
	// 방 생성 응답(S_CreateRoom)에는 LobbyPlayerInfo가 없다. 어차피 본인 슬롯이라 "나"로 나온다.
	lobbyPlayers_.push_back({ myId, lobbyDisplayName(myId, nullptr), selectedLobbyWeapon_ });
	lobbyState_ = LobbyState::WaitingRoom;
	refreshLobbyUI();
	applyCursorPolicy();
	gSharedLog << "[Lobby] 방 생성됨: " << code << " (myId=" << myId << ")\n";
}

void Game::onLobbyJoined(bool success, uint16 hostId, uint16 myId, const std::string& code, const std::vector<LobbyPlayerInfo>& playerInfos) {
	if (!success) {
		lobbyUI_.setMainMenuMessage(L"방을 찾을 수 없습니다");
		gSharedLog << "[Lobby] 방 참가 실패\n";
		return;
	}

	myLobbyId_     = myId;
	hostId_   = hostId;
	roomCode_ = code;
	isHost_   = (myId == hostId);
	selectedLobbyWeapon_ = PlayerWeaponType::Katana;

	lobbyPlayers_.clear();
	for (const LobbyPlayerInfo& info : playerInfos) {
		lobbyPlayers_.push_back({ info.sessionId, lobbyDisplayName(info.sessionId, info.nickname), info.weaponType });
		if (info.sessionId == myLobbyId_) {
			selectedLobbyWeapon_ = info.weaponType;
		}
	}

	lobbyState_ = LobbyState::WaitingRoom;
	refreshLobbyUI();
	applyCursorPolicy();
	gSharedLog << "[Lobby] 방 참가 성공: " << code << " (myId=" << myId << ", host=" << hostId << ")\n";
}

void Game::onLobbyPlayerJoined(const LobbyPlayerInfo& info) {
	const bool exists = std::any_of(lobbyPlayers_.begin(), lobbyPlayers_.end(),
		[&info](const LobbyPlayer& p) { return p.sessionId == info.sessionId; });
	if (!exists) {
		lobbyPlayers_.push_back({ info.sessionId, lobbyDisplayName(info.sessionId, info.nickname), info.weaponType });
	}
	refreshLobbyUI();
	gSharedLog << "[Lobby] 플레이어 입장: " << info.sessionId << "\n";
}

void Game::onLobbyPlayerLeft(uint16 sessionId) {
	std::erase_if(lobbyPlayers_, [sessionId](const LobbyPlayer& p) { return p.sessionId == sessionId; });

	// 호스트가 떠났으면 서버 규칙과 동일하게 남은 목록의 front를 새 호스트로 본다.
	if (sessionId == hostId_ && !lobbyPlayers_.empty()) {
		hostId_ = lobbyPlayers_.front().sessionId;
		isHost_ = (myLobbyId_ == hostId_);
	}

	refreshLobbyUI();
	gSharedLog << "[Lobby] 플레이어 퇴장: " << sessionId << "\n";
}

void Game::onLobbyWeaponSelected(uint16 sessionId, PlayerWeaponType weaponType) {
	const auto ordinal = static_cast<uint8>(weaponType);
	if (ordinal > static_cast<uint8>(PlayerWeaponType::HeavyArrow)) {
		return;
	}

	for (auto& p : lobbyPlayers_) {
		if (p.sessionId == sessionId) {
			p.weaponType = weaponType;
			break;
		}
	}
	if (sessionId == myLobbyId_) {
		selectedLobbyWeapon_ = weaponType;
	}
	refreshLobbyUI();
}

void Game::onGameStart(const std::string& roomServerIp, uint16 roomServerPort, const std::string& lobbyCode, const EntryTicket& ticket) {
	// 이 함수는 로비 recv APC 안에서 실행되므로 여기서 소켓을 건드리지 않고, 핸드오프 요청만 적재한다.
	// 실제 재접속/씬 전환은 LobbyScene이 APC 밖(안전 지점)에서 에셋 로드 완료 후 수행한다.
	handoffIp_     = roomServerIp;
	handoffPort_   = roomServerPort;
	handoffCode_   = lobbyCode;
	handoffTicket_ = ticket;   // POD 복사라 APC 안에서도 안전하다.
	pendingHandoff_ = true;
	gSharedLog << "[Lobby] 게임 시작 신호 수신: RoomServer " << roomServerIp << ":" << roomServerPort
		<< " (code=" << lobbyCode << ")\n";
}

// 윈도우 프로시저에서 특정한 메시지 처리를 위임받는다.
LRESULT Game::receiveWndMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	// The full-screen fade is modal: ignore UI activation while the old scene is
	// disappearing and while the newly built lobby is being revealed.
	if (lobbyReturnFade_.active()) {
		switch (msg) {
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_CHAR:
			return 0;
		}
	}

	// Dialogue input has priority during gameplay, but the topmost settings modal
	// must receive pointer/key input while it is open.
	if (!settingsPanel_.isOpen()
		&& dialogueSystem_.handleWndMsg(msg, wParam, lParam)) {
		return 0;
	}
	if (inventoryPanel_.isOpen()
		&& (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN)) {
		inventoryPanel_.handleGlobalPointerDown(
			static_cast<float>(LOWORD(lParam)),
			static_cast<float>(HIWORD(lParam)));
	}

	switch (msg) {
		// WM_INPUT 메시지
		// 마우스 움직임의 경우 WM_MOUSEMOVE 메시지 대신 이 메시지로 처리하는 것이
		// 정확하고 안정적인 처리가 가능하다.
		// 윈도우 경계로부터 영향을 받지 않고, 가상 커서 속도/가속도 설정을 무시하며,
		// Alt-Tab / 창 이동 후에도 상태 복구가 명확하다.
		// (DPI 스케일링 환경에서도 입력이 왜곡되지 않는다고 한다.)
	case WM_INPUT: {
		static auto sRawInputBuffer = std::vector<std::uint8_t>(256);
		UINT rawInputSize{};
		UINT rawInputResult{};

		// 입력 구조체 크기 수신
		rawInputResult = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT, nullptr, &rawInputSize, sizeof(RAWINPUTHEADER)
		);
		// Input-read failures are survivable: log and drop the event. Exiting here
		// killed every client in turn when sibling windows on the same PC closed
		// (focus churn delivers broken/synthesized WM_INPUT to the next window).
		DISPLAY_ERROR_GLE(rawInputResult != -1, false);
		if (rawInputResult == static_cast<UINT>(-1)) {
			return 0;
		}

		if (rawInputSize > sRawInputBuffer.size()) {
			sRawInputBuffer.resize(rawInputSize);
		}

		// 입력 구조체 내용 수신
		rawInputResult = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT, sRawInputBuffer.data(), &rawInputSize, sizeof(RAWINPUTHEADER)
		);
		DISPLAY_ERROR_GLE(rawInputResult == rawInputSize, false);
		if (rawInputResult != rawInputSize) {
			return 0;
		}

		auto ri = reinterpret_cast<const RAWINPUT*>(sRawInputBuffer.data());
		if (ri->header.dwType == RIM_TYPEMOUSE) {
			// Absolute moves (input synthesized during focus churn, RDP, tablets)
			// are not camera input; drop the event instead of exiting the process.
			if (ri->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
				return 0;
			}

			// 마우스 이동량 기록
			mouseDeltaX_ += ri->data.mouse.lLastX;
			mouseDeltaY_ += ri->data.mouse.lLastY;
		}
		return 0;
	}

	// Alt+Tab 등으로 윈도우가 포커스를 잃었다가 되찾은 경우,
	// 커서와 관련된 플래그들을 읽어 커서 캡처, 커서 숨기기 등을 다시 수행한다.
	case WM_SETFOCUS:
		applyCursorPolicy();
		break;

	// Alt+Tab 등으로 윈도우가 포커스를 잃은 경우
	// 커서 캡처 해제, 커서 보이기 등을 수행한다.
	// 다른 윈도우로 전환되었는데 커서가 보이지 않거나 안 움직여지면 곤란할 것이다.
	case WM_KILLFOCUS:
		releaseCursor();
		showCursor();
		break;

	case WM_SIZE:
		return DefWindowProcA(hWnd, msg, wParam, lParam);

	case WM_MOUSEWHEEL:
		// In-game: accumulate wheel notches for the skill dial (consumed in
		// processInputGame). With a cursor-capturing UI open, forward to the UI.
		if (inventoryPanel_.isOpen() || uiManager_.needsCursor())
			uiManager_.onWndMsg(msg, wParam, lParam);
		else
			wheelAccum_ += GET_WHEEL_DELTA_WPARAM(wParam);
		return 0;

	default:
		uiManager_.onWndMsg(msg, wParam, lParam);
		break;
	}

	return DefWindowProcA(hWnd, msg, wParam, lParam);
}

void Game::sendMovePacket() {
	auto sendBuffer = PacketManager::makeCMovePacket(player_->pos().getXmf(), player_->velocity().getXmf());
	INet::ClientApp::addSendBuffer(sendBuffer);
}

void Game::sendInventoryAction(uint8 slotIndex, InventoryAction action) {
	if (!player_ || !inventoryPanel_.isOpen()) {
		inventoryPanel_.setPending(false);
		return;
	}
	INet::ClientApp::addSendBuffer(PacketManager::makeCInventoryActionPacket(
		inventory_.revision(), slotIndex, action));
}

void Game::setInventoryOpen(bool open) {
	if (open == inventoryPanel_.isOpen())
		return;
	if (open) inventoryPanel_.open();
	else inventoryPanel_.close();
	applyCursorPolicy();
}

bool Game::findZoneCenter(const std::string& tag, mu::Vec3& out) const {
	for (const auto& z : chunkManager_.zones()) {
		if (z.tag != tag || z.volumes.empty()) continue;
		out = z.volumes[0].center;
		return true;
	}
	return false;
}

void Game::debugTeleportToArena(const std::string& tag) {
	if (!player_) return;
	mu::Vec3 center{};
	if (!findZoneCenter(tag, center)) {
		std::cout << "[DebugTeleport] zone '" << tag << "' not found\n";
		return;
	}
	// Local prediction: snap the player to the zone center (prev == curr via setPos), drop velocity.
	player_->setPos(center);
	player_->setVelocity(mu::Vec3{});
	moveChange_ = true;
	// Authoritative jump: the server applies this without the 7m/packet clamp, so its zone Enter fires.
	INet::ClientApp::addSendBuffer(PacketManager::makeCDebugTeleportPacket(center.getXmf()));
	std::cout << "[DebugTeleport] -> '" << tag << "' (" << center.x() << ", " << center.y() << ", " << center.z() << ")\n";
}

// ── 스킬 오브젝트 조회 테이블 ────────────────────────────────────────────────────
// 서버 오브젝트 id로 색인하는 희소 배열. id는 서버 IdPool에서 나오며 룸이 생길 때마다
// 수백씩 커지므로, 등록은 전부 여기를 거쳐 배열을 필요한 만큼 늘린다.
// 배경: RoomServer/docs/objectIdLifecycle.md

void Game::registerSkillObject(i32t id, Object* obj) {
	if (id < 0) return;
	const auto sid = static_cast<size_t>(id);
	if (sid >= skillObjectById_.size()) skillObjectById_.resize(sid + 1, nullptr);
	skillObjectById_[sid] = obj;
}

void Game::unregisterSkillObject(i32t id) {
	if (id < 0) return;
	const auto sid = static_cast<size_t>(id);
	if (sid < skillObjectById_.size()) skillObjectById_[sid] = nullptr;
}

// ── 오브젝트 id 정합성 감시 ──────────────────────────────────────────────────────
// 계획 문서: async-launching-quokka.md. 수정이 아니라 관측용이며, 원인이 확정되면
// 이 두 함수와 lastNpcMoveAt_/diagElapsed_는 제거하거나 정식 디버그 기능으로 승격한다.

void Game::debugLogSkillOwnerResolution(const char* phase, uint32 assetId, i32t ownerId) const {
	const bool inRange = (ownerId >= 0 &&
		static_cast<size_t>(ownerId) < skillObjectById_.size());
	const Object* owner = inRange ? skillObjectById_[static_cast<size_t>(ownerId)] : nullptr;

	// 정상 경로에서 매 시전마다 찍히면 로그가 시끄러우므로, 해석에 실패한 경우만 남긴다.
	if (owner != nullptr) return;

	std::cout << "[Skill] owner unresolved: " << phase << " asset=" << assetId
		<< " owner=" << ownerId
		<< " resolved=0 (" << (inRange ? "slot is null" : "index out of range")
		<< ") tableSize=" << skillObjectById_.size()
		<< " -- VFX/SFX will play at world origin\n";
}

void Game::debugAuditObjectRegistry() const {
	std::cout << "\n===== [Audit] object registry =====\n";

	const i32t localId = player_ ? player_->getId() : -1;
	std::cout << "localPlayerId=" << localId
		<< " skillObjectById_.size=" << skillObjectById_.size()
		<< " idMonsterMap_=" << idMonsterMap_.size()
		<< " corpses_=" << corpses_.size()
		<< " detachedNpcIds_=" << detachedNpcIds_.size() << '\n';

	int issues = 0;

	// ① 로컬 플레이어 슬롯. 여기가 어긋나면 자기 스킬의 VFX/SFX가 통째로 사라진다.
	if (player_) {
		const auto pid = static_cast<size_t>(player_->getId());
		if (pid >= skillObjectById_.size()) {
			std::cout << "  [PLAYER SLOT MISMATCH] id=" << pid
				<< " >= tableSize=" << skillObjectById_.size() << " (never registered)\n";
			++issues;
		}
		else if (skillObjectById_[pid] != player_.get()) {
			std::cout << "  [PLAYER SLOT MISMATCH] id=" << pid
				<< " slot=" << static_cast<const void*>(skillObjectById_[pid])
				<< " expected=" << static_cast<const void*>(player_.get()) << '\n';
			++issues;
		}
	}

	// ② 서버 동기 컨테이너 전수 검사. 렌더는 되는데 id 맵에 없으면 이동·피격을
	//    영영 못 받는 "유령"이다.
	std::unordered_set<const Object*> seen;
	auto auditPool = [&](const char* kindName, const auto& pool) {
		for (const auto& sp : pool) {
			const Object* obj = sp.get();
			if (!obj) continue;
			seen.insert(obj);

			const auto id = static_cast<uint16>(obj->getId());
			const auto it = idMonsterMap_.find(id);
			const bool mapped = (it != idMonsterMap_.end() && it->second == obj);

			if (!mapped) {
				std::cout << "  [ORPHAN] " << kindName << " id=" << obj->getId()
					<< " pos=(" << obj->pos().x() << ", " << obj->pos().y()
					<< ", " << obj->pos().z() << ")"
					<< " hp=" << obj->hp()
					<< " renderObjId=" << obj->renderObjectId()
					<< (it == idMonsterMap_.end() ? " (absent from idMonsterMap_)"
					                              : " (idMonsterMap_ points elsewhere)")
					<< '\n';
				++issues;
			}

			const auto sid = static_cast<size_t>(obj->getId());
			if (sid >= skillObjectById_.size() || skillObjectById_[sid] != obj) {
				std::cout << "  [SKILL SLOT MISMATCH] " << kindName << " id=" << obj->getId()
					<< " tableSize=" << skillObjectById_.size() << '\n';
				++issues;
			}

			// STALE: 서버가 이 개체의 이동을 안 보낸 지 오래됐다 = 서버에선 hp<=0
			// (enter 스냅샷에 사망 몬스터가 실려 온 경우가 대표적).
			if (mapped) {
				const auto seenAt = lastNpcMoveAt_.find(id);
				const float ageSec = (seenAt == lastNpcMoveAt_.end())
					? std::chrono::duration<float>(diagElapsed_).count()
					: std::chrono::duration<float>(diagElapsed_ - seenAt->second).count();
				if (ageSec > 5.f) {
					std::cout << "  [STALE] " << kindName << " id=" << obj->getId()
						<< " noMoveFor=" << ageSec << "s"
						<< " hp=" << obj->hp()
						<< (seenAt == lastNpcMoveAt_.end() ? " (never moved since spawn)" : "")
						<< " pos=(" << obj->pos().x() << ", " << obj->pos().y()
						<< ", " << obj->pos().z() << ")\n";
					++issues;
				}
			}
		}
	};

	auditPool("Goblin",   goblins_);
	auditPool("Snake",    snakes_);
	auditPool("Mushroom", mushrooms_);
	auditPool("Bomber",   bombers_);
	auditPool("Birdy",    birdys_);
	auditPool("Slime",    slimes_);
	auditPool("Treant",   treants_);
	auditPool("Boss",     bosses_);

	// ③ 역방향: 맵에는 있는데 어느 컨테이너에도 없는 항목(해제된 포인터일 수 있다).
	for (const auto& [id, obj] : idMonsterMap_) {
		if (seen.count(obj) == 0) {
			std::cout << "  [DANGLING MAP ENTRY] id=" << id
				<< " ptr=" << static_cast<const void*>(obj)
				<< " (not in any typed container)\n";
			++issues;
		}
	}

	std::cout << "containers: goblins=" << goblins_.size()
		<< " snakes=" << snakes_.size()
		<< " mushrooms=" << mushrooms_.size()
		<< " bombers=" << bombers_.size()
		<< " birdys=" << birdys_.size()
		<< " slimes=" << slimes_.size()
		<< " treants=" << treants_.size()
		<< " bosses=" << bosses_.size() << '\n';
	std::cout << "===== [Audit] done: " << issues << " issue(s) =====\n\n";
}

void Game::sendMouseMovePacket() {
	const auto forward = player_->forward();
	const auto yawRad = std::atan2(forward.x(), forward.z());

	lastSentAimPitch_ = static_cast<float>(cameraPitch_);
	auto sendBuffer = PacketManager::makeCMouseMovePacket(yawRad, lastSentAimPitch_);
	INet::ClientApp::addSendBuffer(sendBuffer);
}

void Game::sendAttackPacket() {
	INet::ClientApp::addSendBuffer(PacketManager::makeCAttackPacket(estimatedServerTimeMs()));
}

void Game::sendSkillStartPacket(uint32 skillAssetId, uint32 skillSeed) {
	const uint64 actionServerMs = estimatedServerTimeMs();
	// 시전 시점 조준 pitch 스냅샷을 동봉한다(서버 판정·원격 재현이 캐스터 예측과 일치 — seed와 동일 원리).
	INet::ClientApp::addSendBuffer(PacketManager::makeCSkillStartPacket(
		skillAssetId, actionServerMs, skillSeed, static_cast<float>(cameraPitch_)));
}

void Game::beginServerTimeSync() {
	serverClockSynchronized_ = false;
	serverClockOffsetMs_ = 0;
	requestServerTimeSync();
	nextTimeSyncClientMs_ = networkNowMs() + kTimeSyncIntervalMs;
}

void Game::updateServerTimeSync() {
	if (nextTimeSyncClientMs_ == 0) return;
	const uint64 now = networkNowMs();
	if (now < nextTimeSyncClientMs_) return;
	requestServerTimeSync();
	nextTimeSyncClientMs_ = now + kTimeSyncIntervalMs;
}

void Game::requestServerTimeSync() {
	const uint64 clientSendMs = networkNowMs();
	INet::ClientApp::addSendBuffer(PacketManager::makeCTimeSyncPacket(clientSendMs));
}

void Game::onServerTimeSync(uint64 clientSendMs, uint64 serverReceiveMs, uint64 serverSendMs) {
	const uint64 clientReceiveMs = networkNowMs();
	if (clientReceiveMs < clientSendMs || serverSendMs < serverReceiveMs) return;

	const int64 t0 = static_cast<int64>(clientSendMs);
	const int64 t1 = static_cast<int64>(serverReceiveMs);
	const int64 t2 = static_cast<int64>(serverSendMs);
	const int64 t3 = static_cast<int64>(clientReceiveMs);
	const int64 rttMs = (t3 - t0) - (t2 - t1);
	if (rttMs < 0 || rttMs > kMaxAcceptedSyncRttMs) return;

	serverClockOffsetMs_ = ((t1 - t0) + (t2 - t3)) / 2;
	serverClockSynchronized_ = true;
}

uint64 Game::estimatedServerTimeMs() const {
	if (!serverClockSynchronized_) return 0;
	const int64 estimated = static_cast<int64>(networkNowMs()) + serverClockOffsetMs_;
	return estimated > 0 ? static_cast<uint64>(estimated) : 0;
}

void Game::castSkillByName(std::string_view name) {
	const SkillAsset* asset = skillSystem_.findAsset(name);
	if (!asset) return;
	if (skillSystem_.hasActiveSkill(player_->getId())) return;

	// Per-cast deterministic seed: used locally AND sent to the server
	// (C_SkillStart) so server hitboxes / remote visuals match exactly.
	const uint32 skillSeed = std::random_device{}();
	// owner 해석 실패 = PlayVFX/PlaySound가 월드 원점에서 재생되고 로컬 히트박스가
	// 캐스터에 붙지 않는다(= "피격은 되는데 VFX/SFX가 없다" 증상의 직접 원인).
	debugLogSkillOwnerResolution("cast", asset->id, player_->getId());
	skillSystem_.startSkill(asset->id, player_->getId(), skillCtx_, skillSeed);
	sendSkillStartPacket(asset->id, skillSeed);
}

void Game::sendSelectSkillPacket(uint8 slot) {
	INet::ClientApp::addSendBuffer(PacketManager::makeCSelectSkillPacket(slot));
}

void Game::setupSkillDial(PlayerWeaponType weaponType) {
	myWeaponOrdinal_ = static_cast<unsigned>(weaponType);
	skillLoadout_ = SkillLoadout::build(skillSystem_.assets());
	const WeaponLoadout& wl = skillLoadout_.forWeapon(myWeaponOrdinal_);

	const Texture* slotIcons[SkillDialHUD::kSlots] = { nullptr, nullptr, nullptr };
	float costs[SkillDialHUD::kSlots] = { 0.f, 0.f, 0.f };
	float cds[SkillDialHUD::kSlots]   = { 0.f, 0.f, 0.f };
	for (int s = 0; s < SkillDialHUD::kSlots; ++s) {
		dialSlotAssetId_[s] = wl.slotAssetId[s];
		costs[s] = wl.slotCost[s];
		cds[s]   = wl.slotCooldownMs[s];
		if (wl.slotAssetId[s] >= 0) {
			if (const SkillAsset* a = skillSystem_.findAsset(static_cast<u32t>(wl.slotAssetId[s])))
				slotIcons[s] = assetManager_.skillIconByAssetName(a->name);
		}
	}
	basicSkillAssetId_ = wl.basicAssetId;
	skillDial_.configure(myWeaponOrdinal_, slotIcons, assetManager_.playerWeaponIcon(weaponType),
	                     costs, cds, assetManager_.digitAtlasTex());
	skillDial_.setVisible(true);
}

void Game::onSkillCharge(uint16 playerId, uint8 slot, float charge) {
	if (!player_) return;
	if (playerId == static_cast<uint16>(player_->getId())) {
		// Server-authoritative target; the displayed bar fills as energy orbs are
		// absorbed (matchPendingOrbSpawns + onAbsorb). delta < 0 (spend) reflects now.
		skillDial_.setChargeTarget(slot, charge);
		const float delta = charge - prevServerCharge_[slot];
		prevServerCharge_[slot] = charge;
		if (delta > 0.f)
			pendingOrbCharges_.push_back(PendingOrbCharge{ static_cast<int>(slot), delta, 0.f });
		else
			skillDial_.syncDisplayToTarget(slot);
	} else if (slot < SkillDialHUD::kSlots) {
		teammateCharge_[playerId][slot] = charge;
	}
}

void Game::onSkillSelect(uint16 playerId, uint8 slot) {
	if (player_ && playerId == static_cast<uint16>(player_->getId())) return;  // own selection is local
	teammateSelected_[playerId] = slot;
}

void Game::onSkillUseReject(uint8 slot) {
	skillDial_.clearPredictedCooldown(slot);
	if (player_)
		skillSystem_.interruptAll(static_cast<i32t>(player_->getId()), skillCtx_);  // roll back rejected cast
}

void Game::onComboState(uint16 playerId, uint16 comboCount, float windowMs) {
	if (player_ && playerId != static_cast<uint16>(player_->getId())) return;  // own combo only
	comboCount_    = comboCount;
	comboWindowMs_ = windowMs;
	comboSecLeft_  = windowMs / 1000.f;
}

void Game::onPlayerHp(uint16 playerId, int32 newHp) {
	// Authoritative regen HP: set it straight on the player object (covers the local
	// player and remotes, both registered in idPlayerMap_). No EvHit/EvDeath is posted,
	// so no hit animation or blood plays; the per-frame HP UI read reflects it directly.
	if (auto it = idPlayerMap_.find(playerId); it != idPlayerMap_.end())
		it->second->setHp(newHp);
}

void Game::onInventorySnapshot(uint32 revision,
	const std::vector<InventorySlotInfo>& slots) {
	std::vector<ItemStack> stacks;
	stacks.reserve(slots.size());
	for (const InventorySlotInfo& slot : slots)
		stacks.push_back(ItemStack{ slot.itemId, slot.quantity });

	std::string error;
	if (!inventory_.applySnapshot(itemCatalog_, revision, stacks, &error)) {
		gSharedLog << "[Inventory] rejected server snapshot: " << error << "\n";
		return;
	}
	inventoryPanel_.setInventory(inventory_);
	inventoryPanel_.setPending(false);
}

void Game::onInventoryActionResult(uint32 revision, uint8 slotIndex,
	InventoryAction action, InventoryActionResult result, InventorySlotInfo slot) {
	(void)action;
	if (result == InventoryActionResult::Success) {
		std::string error;
		if (!inventory_.applyAuthoritativeSlot(
				itemCatalog_, revision, slotIndex,
				ItemStack{ slot.itemId, slot.quantity }, &error)) {
			gSharedLog << "[Inventory] rejected action result: " << error << "\n";
		} else {
			inventoryPanel_.setInventory(inventory_);
		}
	}
	inventoryPanel_.showActionResult(result);
}

void Game::processInput(Milliseconds deltaTime) {
	if (GetForegroundWindow() != ghWnd) {
		return;
	}
	if (player_ == nullptr) {
		return;
	}

	keyboardStatePrev_ = keyboardStateCurr_;
	DISPLAY_ERROR_GLE( GetKeyboardState(keyboardStateCurr_.data()), false );

	const bool escapePressed =
		(keyboardStateCurr_[VK_ESCAPE] & 0x80)
		&& !(keyboardStatePrev_[VK_ESCAPE] & 0x80);
	const bool inventoryPressed =
		(keyboardStateCurr_['E'] & 0x80)
		&& !(keyboardStatePrev_['E'] & 0x80);

	if (finalScoreboard_.isVisible()) {
		mouseDeltaX_ = 0;
		mouseDeltaY_ = 0;
		return;
	}

	if (inventoryPanel_.isOpen()) {
		if (escapePressed || inventoryPressed)
			setInventoryOpen(false);
		mouseDeltaX_ = 0;
		mouseDeltaY_ = 0;
		return;
	}

	// ESC: 인게임 설정창 토글(로비의 "설정" 버튼과 동일한 패널을 재사용).
	if (escapePressed) {
		settingsPanel_.toggle();
	}

	// 설정창 열림/닫힘 전이에 맞춰 커서 모드를 전환한다.
	// 전이 기반이라 ESC 토글이든 패널의 "닫기" 버튼이든 동일하게 처리된다.
	const bool settingsOpen = settingsPanel_.isOpen();
	if (settingsOpen != settingsOpenPrev_) {
		applyCursorPolicy();
		settingsOpenPrev_ = settingsOpen;
	}

	// 설정창이 열려 있는 동안에는 인게임 입력을 차단한다(카메라/이동/공격/스킬/커서 토글).
	// 누적된 마우스 델타도 비워 닫은 직후 카메라가 튀지 않게 한다.
	if (settingsOpen) {
		mouseDeltaX_ = 0;
		mouseDeltaY_ = 0;
		return;
	}

	// Dialogue input has priority over gameplay: while a dialogue/monologue
	// window is open (including its fade-out, where active() is still true),
	// suppress movement/camera/attack/skill. Enter and left-click are consumed
	// by handleWndMsg to advance pages; here we drop accumulated look delta so
	// the camera doesn't drift, then skip gameplay until the window hides.
	if (dialogueSystem_.active()) {
		mouseDeltaX_ = 0;
		mouseDeltaY_ = 0;
		return;
	}

	if (inventoryPressed) {
		setInventoryOpen(true);
		mouseDeltaX_ = 0;
		mouseDeltaY_ = 0;
		return;
	}

	processInputGame(deltaTime);
}

void Game::processInputGame(Milliseconds deltaTime) {
	const auto prevForward = player_->forward();

	// Grandbaum 넉백/이동잠금: 이동 권한은 클라에 있으므로 여기서 직접 강제 이동/입력잠금을 실행한다.
	// 넉백 중에는 WASD를 무시하고 서버가 준 방향·속도로 밀려나고, 이어 입력잠금 동안 수평 정지한다.
	// (둘 다 그 위치가 매 프레임 C_Move로 서버에 반영된다. 서버는 넉백 동안 클램프를 면제한다.)
	bool movementLocked = false;
	{
		const float kbDt = Seconds(deltaTime).count();
		if (knockbackTimer_ > 0.f) {
			knockbackTimer_ -= kbDt;
			const float vy = player_->velocity().y();
			player_->setVelocity(mu::Vec3(knockbackDir_.x() * knockbackSpeed_, vy, knockbackDir_.z() * knockbackSpeed_));
			if (knockbackTimer_ <= 0.f) {
				knockbackTimer_ = 0.f;
				knockbackSpeed_ = 0.f;
			}
			movementLocked = true;
		}
		else if (postKnockbackLockTimer_ > 0.f) {
			postKnockbackLockTimer_ -= kbDt;
			const float vy = player_->velocity().y();
			player_->setVelocity(mu::Vec3(0.f, vy, 0.f));   // 입력잠금: 수평 정지(y는 중력 보존)
			movementLocked = true;
		}
	}

	// 이동 가속도만 담당. 감속은 PhysicsWorld의 linearDamping(마찰)이 처리한다.
	// 속도 상한은 kPlayerMaxSpeed, 가속률은 kPlayerAccelRate (파일 상단 상수 참조).
	const auto moveXSign = !playerDead_ * ( (keyboardStateCurr_['D'] & 0x80) - (keyboardStateCurr_['A'] & 0x80) );
	const auto moveZSign = !playerDead_ * ( (keyboardStateCurr_['W'] & 0x80) - (keyboardStateCurr_['S'] & 0x80) );
	const bool rightMouseDragging = !uiManager_.needsCursor() && (keyboardStateCurr_[VK_RBUTTON] & 0x80);

	if (!movementLocked && (moveXSign || moveZSign)) {
		if (!rightMouseDragging && std::abs(static_cast<float>(cameraYaw_)) > 1e-5f) {
			player_->setOrient(player_->orient() * mu::NQuat(mu::Radian(0.f), mu::Radian(0.f), cameraYaw_));
			cameraYaw_ = 0.f;
		}

		// 'W'/'S' 입력으로 판정된 Z 부호는 플레이어의 forward 벡터,
		// 'D'/'A' 입력으로 판정된 X 부호는 플레이어의 right 벡터와 곱해 속도의 방향을 정한다.
		const auto moveDirection = mu::NVec3(
			static_cast<float>(moveXSign) * player_->right() + static_cast<float>(moveZSign) * player_->forward()
		);

		// x/z 방향으로만 가속. y(중력)는 물리 엔진이 담당한다.
		// (구버전은 전체 3D 속도를 kPlayerMaxSpeed로 클램프해, 이동 중 낙하 시
		//  물리가 적분한 y속도까지 매 프레임 재스케일 → 물리 damping과 이중으로 감속됐다.
		//  standalone Game::processInput과 동일하게 y를 보존한다.)
		// [임시 디버그] F8 부스트 시 가속률과 속도 상한을 함께 배율로 키운다(평상시 배율=1).
		const float dbgMaxSpeed = kPlayerMaxSpeed * debugSpeedMultiplier_;

		const auto fullVel = player_->velocity();
		const auto accel   = mu::Vec3(moveDirection) * (kPlayerAccelRate * debugSpeedMultiplier_ * Seconds(deltaTime).count());
		float newX = fullVel.x() + accel.x();
		float newZ = fullVel.z() + accel.z();

		// x/z 속도만 클램프 (y는 건드리지 않음).
		const float hSpd2 = newX * newX + newZ * newZ;
		if (hSpd2 > dbgMaxSpeed * dbgMaxSpeed) {
			const float scale = dbgMaxSpeed / std::sqrt(hSpd2);
			newX *= scale;
			newZ *= scale;
		}
		player_->setVelocity(mu::Vec3(newX, fullVel.y(), newZ));
	}

	currVelocity_ = player_->velocity();

	// F key: 조준된 월드 아이템 습득 요청. 서버가 거리/중복/용량을 권위로 판정하므로
	// 여기서는 연타만 막는다(pickupPendingId_는 응답 또는 1초 타임아웃에 해제).
	if ( (keyboardStateCurr_['F'] & 0x80) && !(keyboardStatePrev_['F'] & 0x80)
		&& aimedDropId_ != 0 && pickupPendingId_ == 0 && !playerDead_ ) {
		pickupPendingId_  = aimedDropId_;
		pickupPendingSec_ = 0.f;
		INet::ClientApp::addSendBuffer(PacketManager::makeCItemPickupPacket(aimedDropId_));
		INet::ClientApp::send();
	}

	// C key: toggle CSM cascade debug visualization
	if ( (keyboardStateCurr_['C'] & 0x80) && !(keyboardStatePrev_['C'] & 0x80) ) {
		gfx_.toggleCsmDebugVisualization();
	}

	// H key: toggle Hi-Z occlusion culling
	if ( (keyboardStateCurr_['H'] & 0x80) && !(keyboardStatePrev_['H'] & 0x80) ) {
		gfx_.setHiZCullEnabled(!gfx_.isHiZCullEnabled());
	}

	// G key: cycle GBuffer debug view (deferred path only)
	if ( (keyboardStateCurr_['G'] & 0x80) && !(keyboardStatePrev_['G'] & 0x80) ) {
		gfx_.cycleGBufferDebugMode();
	}

	// F5/F6/F7: debug teleport to each arena to test the mid-boss encounters.
	if ( (keyboardStateCurr_[VK_F5] & 0x80) && !(keyboardStatePrev_[VK_F5] & 0x80) )
		debugTeleportToArena( "Arena_Hobgoblin" );
	if ( (keyboardStateCurr_[VK_F6] & 0x80) && !(keyboardStatePrev_[VK_F6] & 0x80) )
		debugTeleportToArena( "Arena_Grandbaum" );
	if ( (keyboardStateCurr_[VK_F7] & 0x80) && !(keyboardStatePrev_[VK_F7] & 0x80) )
		debugTeleportToArena( "Arena_Isys" );
	if ( (keyboardStateCurr_[VK_F9] & 0x80) && !(keyboardStatePrev_[VK_F9] & 0x80) )
		debugTeleportToArena( "Arena_Boss" );

	// F12: 오브젝트 id 정합성 감사. 유령 몬스터를 목격한 즉시 눌러
	// ORPHAN / STALE / SLOT MISMATCH 중 무엇인지 콘솔에서 확정한다.
	if ( (keyboardStateCurr_[VK_F12] & 0x80) && !(keyboardStatePrev_[VK_F12] & 0x80) )
		debugAuditObjectRegistry();

	// F8: [임시 디버그] 로컬 플레이어 이동 속도 부스트 토글(가벽 텍스처 위치 등 빠른 이동 점검용).
	if ( (keyboardStateCurr_[VK_F8] & 0x80) && !(keyboardStatePrev_[VK_F8] & 0x80) ) {
		debugSpeedMultiplier_ = (debugSpeedMultiplier_ > 1.f) ? 1.f : 5.f;
		std::cout << "[Debug] player speed multiplier = " << debugSpeedMultiplier_ << "x\n";
	}


	// 마우스 민감도를 기반으로 1인칭 카메라 모드와 3인칭 카메라 모드일 때
	// 각각의 플레이어 yaw, 카메라 pitch를 계산한다.
	// (pitch를 플레이어에 적용하게 되면, 플레이어가 고개를 들고 내리는 게 아니라 굴러버린다.)
	const auto mouseSensitivity = mu::pi * 2.f;

	const auto yaw = mu::Radian(mouseDeltaX_ * mouseSensitivity / static_cast<float>(gClientRect.right - gClientRect.left));
	auto yawRotation = mu::NQuat(mu::Radian(0.f), mu::Radian(0.f), yaw);

	cameraPitch_ = std::clamp(
		static_cast<float>(cameraPitch_) + mouseDeltaY_ * mouseSensitivity / static_cast<float>(gClientRect.bottom - gClientRect.top),
		-mu::pi * 0.16f,
		mu::pi * 0.3f
	);
	// 조준 pitch를 로컬 플레이어에 반영한다(스파인 굽힘·스킬 aim 합성이 읽음).
	// body orient에는 넣지 않는다(pitch를 orient에 넣으면 캐릭터가 굴러버림).
	player_->setAimPitch(static_cast<float>(cameraPitch_));

	if (!playerDead_) {
		if (rightMouseDragging) {
			cameraYaw_ += yaw;
		}
		else if (std::abs(static_cast<float>(yaw)) > 1e-6f) {
			player_->setOrient(player_->orient() * yawRotation);
			cameraYaw_ = 0.f;
		}
		camera_.setOffsetFromTargetPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, cameraYaw_) );
	}
	else {
		cameraYaw_ += yaw;
		camera_.setOffsetFromTargetPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, cameraYaw_) );
	}

	const auto currForward = player_->forward();

	// yaw(전방)뿐 아니라 pitch만 변한 경우에도 송신한다 — 종전 조건은 forward가
	// yaw 전용이라 pitch 변화가 패킷으로 나가지 않았다.
	if (prevForward != currForward
		|| std::fabs(static_cast<float>(cameraPitch_) - lastSentAimPitch_) > 0.01f) {
		sendMouseMovePacket();
	}

	mouseDeltaX_ = 0;
	mouseDeltaY_ = 0;

	// --- Skill dial: wheel selects, wheel-click uses, left-click = basic attack ---
	skillDial_.update(deltaTime.count() / 1000.f);
	comboSecLeft_ = std::max(0.f, comboSecLeft_ - deltaTime.count() / 1000.f);   // local combo countdown

	// DEV: arrow keys nudge the skill dial's resolution-relative pivot margin so a
	// good position can be found live, without recompiling. Logs the settled
	// fraction/pixel values on key release so repeated nudges are easy to read back.
	if (skillDial_.visible() && !uiManager_.needsCursor()) {
		const float dtSec = deltaTime.count() / 1000.f;
		const bool  shift = (keyboardStateCurr_[VK_SHIFT] & 0x80) != 0;
		const float speed = (shift ? 600.f : 120.f) * dtSec;   // px/sec, at current resolution

		const float dx = (keyboardStateCurr_[VK_RIGHT] & 0x80) ? speed
		                : (keyboardStateCurr_[VK_LEFT]  & 0x80) ? -speed : 0.f;
		const float dy = (keyboardStateCurr_[VK_DOWN]   & 0x80) ? -speed
		                : (keyboardStateCurr_[VK_UP]     & 0x80) ?  speed : 0.f;

		const float w = static_cast<float>(gClientRect.right - gClientRect.left);
		const float h = static_cast<float>(gClientRect.bottom - gClientRect.top);
		if (dx != 0.f || dy != 0.f) skillDial_.nudgeMarginPx(dx, dy, w, h);

		const bool anyDown     = ((keyboardStateCurr_[VK_LEFT] | keyboardStateCurr_[VK_RIGHT] |
		                            keyboardStateCurr_[VK_UP]  | keyboardStateCurr_[VK_DOWN]) & 0x80) != 0;
		const bool anyDownPrev = ((keyboardStatePrev_[VK_LEFT] | keyboardStatePrev_[VK_RIGHT] |
		                            keyboardStatePrev_[VK_UP]  | keyboardStatePrev_[VK_DOWN]) & 0x80) != 0;
		if (anyDownPrev && !anyDown) {
			gSharedLog << "[SkillDial Tune] marginXFrac=" << skillDial_.marginXFrac()
			           << " marginYFrac=" << skillDial_.marginYFrac()
			           << " (px@" << w << "x" << h << ": "
			           << w * skillDial_.marginXFrac() << ", " << h * skillDial_.marginYFrac() << ")\n";
			dumpLog();
		}
	}

	const float nowSec = std::chrono::duration<float>(
		std::chrono::steady_clock::now().time_since_epoch()).count();

	if (!playerDead_ && !uiManager_.needsCursor()) {
		// Shift+wheel zooms the minimap; plain wheel selects a skill slot.
		// (WM_MOUSEWHEEL carries no MK_SHIFT here, so read the keyboard snapshot.)
		const bool shiftHeld = (keyboardStateCurr_[VK_SHIFT] & 0x80) != 0;
		if (shiftHeld) {
			while (wheelAccum_ >= WHEEL_DELTA) { wheelAccum_ -= WHEEL_DELTA; minimap_.zoomIn(); }
			while (wheelAccum_ <= -WHEEL_DELTA) { wheelAccum_ += WHEEL_DELTA; minimap_.zoomOut(); }
		} else {
		// Wheel selection (one notch = one slot). Up = prev, down = next.
		while (wheelAccum_ >= WHEEL_DELTA) {
			wheelAccum_ -= WHEEL_DELTA;
			skillDial_.selectPrev();
			sendSelectSkillPacket(static_cast<uint8>(skillDial_.selected()));
		}
		while (wheelAccum_ <= -WHEEL_DELTA) {
			wheelAccum_ += WHEEL_DELTA;
			skillDial_.selectNext();
			sendSelectSkillPacket(static_cast<uint8>(skillDial_.selected()));
		}
		}

		if ( (keyboardStateCurr_[VK_LBUTTON] & 0x80) && !(keyboardStatePrev_[VK_LBUTTON] & 0x80) ) {
			// Shift + Left click : use the selected skill if it has a stack and
			// is off (predicted) cooldown. Instant cast; the server re-validates.
			if (keyboardStateCurr_[VK_SHIFT] & 0x80) {
				const int slot = skillDial_.selected();
				if (skillDial_.canUse(slot, nowSec) && dialSlotAssetId_[slot] >= 0) {
					if (const SkillAsset* a = skillSystem_.findAsset(static_cast<u32t>(dialSlotAssetId_[slot]))) {
						castSkillByName(a->name);
						skillDial_.notePredictedUse(slot, nowSec);
					}
				}
			}
			// Normal Left click: basic attack (ungated weapon skill).
			else if ( basicSkillAssetId_ >= 0 ) {
				if (const SkillAsset* a = skillSystem_.findAsset(static_cast<u32t>(basicSkillAssetId_)))
					castSkillByName(a->name);
			}
		}
	} else {
		wheelAccum_ = 0;   // discard notches while a cursor UI is open or the player is dead
	}
}

void Game::cullObjects() {
	auto entities = std::vector< std::shared_ptr<Object> >();
	entities.reserve(otherPlayers_.size() + goblins_.size() + snakes_.size() + mushrooms_.size()
	                 + bombers_.size() + birdys_.size() + slimes_.size() + treants_.size() + bosses_.size());
	std::ranges::copy(otherPlayers_, std::back_inserter(entities));
	std::ranges::copy(goblins_,      std::back_inserter(entities));
	std::ranges::copy(snakes_,       std::back_inserter(entities));
	std::ranges::copy(mushrooms_,    std::back_inserter(entities));
	std::ranges::copy(bombers_,      std::back_inserter(entities));
	std::ranges::copy(birdys_,       std::back_inserter(entities));
	std::ranges::copy(slimes_,       std::back_inserter(entities));
	std::ranges::copy(treants_,      std::back_inserter(entities));
	std::ranges::copy(bosses_,       std::back_inserter(entities));

	// perform view frusutum culling
	for (auto& entt : entities) {
		auto& rootShape = entt->body().worldBVH().nodes[0].shape;

		auto vertices = std::vector<mu::Vec3>();
		vertices.reserve(8u);
		auto out = std::back_inserter(vertices);

		if (std::holds_alternative<AABB>(rootShape)) {
			const auto& aabb = std::get<AABB>(rootShape);

			const mu::Vec3 c = aabb.center;
			const mu::Vec3 h = aabb.size * 0.5f;

			out = mu::Vec3(c.x() - h.x(), c.y() - h.y(), c.z() - h.z());
			out = mu::Vec3(c.x() + h.x(), c.y() - h.y(), c.z() - h.z());
			out = mu::Vec3(c.x() - h.x(), c.y() - h.y(), c.z() + h.z());
			out = mu::Vec3(c.x() + h.x(), c.y() - h.y(), c.z() + h.z());
			out = mu::Vec3(c.x() - h.x(), c.y() + h.y(), c.z() - h.z());
			out = mu::Vec3(c.x() + h.x(), c.y() + h.y(), c.z() - h.z());
			out = mu::Vec3(c.x() - h.x(), c.y() + h.y(), c.z() + h.z());
			out = mu::Vec3(c.x() + h.x(), c.y() + h.y(), c.z() + h.z());

		} else {
			const auto& obb = std::get<OBB>(rootShape);
			// Compute OBB axes from orientation
			const mu::Vec3 ax = obb.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
			const mu::Vec3 ay = obb.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
			const mu::Vec3 az = obb.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));
			const float    hx = obb.halfExtents.x();
			const float    hy = obb.halfExtents.y();
			const float    hz = obb.halfExtents.z();
			// All 8 corners; testVertex will filter by penetration depth
			for (int sx : {-1, 1})
				for (int sy : {-1, 1})
					for (int sz : {-1, 1}) {
						out = obb.center
							+ ax * (hx * sx)
							+ ay * (hy * sy)
							+ az * (hz * sz);
					}
		}

		entt->setFrustumCulled(true);

		for (auto& v : vertices) {
			auto ndc = mu::Vec4(v, 1.f) * camera_.view() * camera_.proj();
			ndc /= ndc.w();

			if (ndc.x() >= -1.f && ndc.x() <= 1.f
				&& ndc.y() >= -1.f && ndc.y() <= 1.f
				&& ndc.z() >= 0.f && ndc.z() <= 1.f
			) {
				// a vertex is in the view frustum, it should not be culled.
				entt->setFrustumCulled(false);
				break;
			}
		}
	}
}

// applyHiZCulling이었던 함수. 컬링 자체를 결정하지 않고, 이미 계산된 Hi-Z/frustum
// 결과를 Object/AnimBlender에 반영(피드백)하는 역할이라 이름을 바꿨다.
// 새로 생성된 오브젝트는 컬링 판정과 무관하게 최초 1회는 애니메이션이 갱신되도록
// (animBlender->hasEverUpdated() == false면) culled를 강제로 false로 둔다.
// 서버에서 전달되어 생성되자마자 화면 밖/Hi-Z invisible로 판정되면 한 번도 갱신되지
// 못한 채 방치(T-pose 등)될 수 있기 때문이다.
void Game::feedbackCullResultToAnim() {
	if (!gfx_.isHiZCullEnabled()) {
		for (auto& p : otherPlayers_) {
			p->setHiZCulled(false);
			if (auto* blender = p->animBlender())
				blender->setCulled(blender->hasEverUpdated() && p->isFrustumCulled());
		}
		auto resetHiZ = [&](const std::shared_ptr<Object>& m) {
			m->setHiZCulled(false);
			if (auto* blender = m->animBlender())
				blender->setCulled(blender->hasEverUpdated() && m->isFrustumCulled());
		};
		for (auto& g : goblins_)   resetHiZ(g);
		for (auto& s : snakes_)    resetHiZ(s);
		for (auto& m : mushrooms_) resetHiZ(m);
		for (auto& b : bombers_)   resetHiZ(b);
		for (auto& b : birdys_)    resetHiZ(b);
		for (auto& s : slimes_)    resetHiZ(s);
		for (auto& t : treants_)   resetHiZ(t);
		for (auto& b : bosses_)    resetHiZ(b);
		return;
	}

	auto applyToEntity = [&](const std::shared_ptr<Object>& entt) {
		const bool hiZVisible = gfx_.getHiZObjectVisible(entt->renderObjectId());
		entt->setHiZCulled(!hiZVisible);
		if (auto* blender = entt->animBlender())
			blender->setCulled(blender->hasEverUpdated() && (entt->isFrustumCulled() || !hiZVisible));
	};
	for (auto& g : goblins_)   applyToEntity(g);
	for (auto& s : snakes_)    applyToEntity(s);
	for (auto& m : mushrooms_) applyToEntity(m);
	for (auto& b : bombers_)   applyToEntity(b);
	for (auto& b : birdys_)    applyToEntity(b);
	for (auto& s : slimes_)    applyToEntity(s);
	for (auto& t : treants_)   applyToEntity(t);
	for (auto& b : bosses_)    applyToEntity(b);
	for (auto& p : otherPlayers_) applyToEntity(p);
}

// 커서가 클라이언트 영역 바깥으로 나가지 못하도록 한다.
// 한번 설정해놓으면, releaseCursor를 호출하기 전까지 커서는 계속 클라이언트 영역에 갇혀있는다.
void Game::captureCursor() {
    auto ul = POINT{ gClientRect.left, gClientRect.top };
    auto lr = POINT{ gClientRect.right, gClientRect.bottom };

	// 클라이언트의 외곽 좌표를 윈도우 좌표로 변환
    MapWindowPoints(ghWnd, nullptr, &ul, 1);
    MapWindowPoints(ghWnd, nullptr, &lr, 1);

    auto clipRect = RECT{ ul.x, ul.y, lr.x, lr.y };

    ClipCursor(&clipRect);
}

// 커서 캡처가 설정되어있다면 해제한다.
// captureCursor로 활성화된 커서 캡처를 해제하는 역할을 한다.
void Game::releaseCursor() {
	ClipCursor(nullptr);
}

void Game::hideCursor() {
	// ShowCursor()는 내부적으로 display counter를 증가/감소시키는 구조라서
	// 반복 호출해 정확히 숨기거나 표시해야 한다.
	while (ShowCursor(false) >= 0) {}
}

void Game::showCursor() {
	// ShowCursor()는 내부적으로 display counter를 증가/감소시키는 구조라서
	// 반복 호출해 정확히 숨기거나 표시해야 한다.
	while (ShowCursor(true) < 0) {}
}

void Game::applyCursorPolicy() {
	if (GetForegroundWindow() != ghWnd) {
		return;
	}

	// 로비 메인 메뉴와 포인터를 사용하는 모달 UI에서는 커서를 표시하고
	// 클라이언트 영역 구속을 해제한다. 대화 종료 시 이 함수를 다시 호출해
	// 현재 씬/모달 상태에 맞는 게임플레이 커서 모드로 복귀한다.
	const bool releaseCursorNow = (scene_ == Scene::Lobby && lobbyState_ == LobbyState::MainMenu)
		|| finalScoreboard_.isVisible()
		|| settingsPanel_.isOpen() || inventoryPanel_.isOpen() || dialogueSystem_.active();
	const bool showCursorNow = (scene_ == Scene::Lobby)
		|| finalScoreboard_.isVisible()
		|| settingsPanel_.isOpen() || inventoryPanel_.isOpen() || dialogueSystem_.active();
	cursorCaptureEnabled_ = !releaseCursorNow;
	cursorShowEnabled_ = showCursorNow;

	if (releaseCursorNow) {
		releaseCursor();
	} else {
		captureCursor();
	}
	if (showCursorNow) {
		showCursor();
	} else {
		hideCursor();
	}
}

}	// namespace Online
