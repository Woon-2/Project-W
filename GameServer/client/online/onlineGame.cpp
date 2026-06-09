#include "pch.hpp"
#include "onlineGame.hpp"
#include "../errorHandling.hpp"
#include "../binaryImport.hpp"
#include "../timer.hpp"
#include "../particleImporter.hpp"
#include "../ui/widgets/Dropdown.hpp"
#include "SendBuffer.hpp"
#include "../PacketManager.hpp"
#include "../ClientApp.hpp"
#include "../skill/skillCompiler.hpp"

extern HWND ghWnd;
extern RECT gClientRect;

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

static constexpr int     kRenderSkipLagFrames = 4;
static constexpr int     kMaxPhysicsStepsPerFrame = 3;
static constexpr Seconds kMaxPhysicsDeltaTime{ 1.f / 60.f * kMaxPhysicsStepsPerFrame };
static constexpr int     kMaxPhysicsScaleK    = 4;
static constexpr int     kLagScaleUpFrames    = 2;
static constexpr int     kLagScaleDownFrames  = 100;

static const mu::Vec3 kLobbyCameraBaseEye(5142.34f, 84.0199f, 5125.55f);
static const mu::Vec3 kLobbyCameraBaseAt(5140.71f, 83.0828f, 5123.8f);

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
}

Game::~Game() {
	// 소멸자 본문은 모든 멤버 소멸(threadPool_ 워커 join 포함)보다 먼저 실행된다.
	// 백그라운드 파티클 JSON 파싱이 진행 중이면 즉시 중단을 요청해, 로딩 워커가
	// 남은 파일을 끝까지 파싱하지 않고 빠르게 빠져나오도록 한다.
	assetLoadAbort_.store(true, std::memory_order_relaxed);
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

	skybox_.setModel( assetManager_.modelCube( ) );
	skybox_.setSkyboxMaterial( assetManager_.skyboxMaterial( ) );

	dirLight_.setOrient( mu::NQuat( mu::Degree( 0.f ), mu::Degree( 132.f ), mu::Degree( 0.f ) ) );
	dirLight_.color = mu::Vec3( 0.8f, 0.8f, 0.8f );
	dirLight_.intensity = 2.f;
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

	// 전시 캐릭터(스킨드)의 renderObjectId 인덱싱이 안전하도록 가시성 배열을 확보한다.
	gfx_.setMaxRenderObjectId(1000u);

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
	for (auto* bay : slotBays_) {
		if (bay) { bay->visible = false; bay->texture = nullptr; }
	}
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

	constexpr float kPlayerHpUiX = 20.f;
	constexpr float kPlayerHpUiY = 20.f;
	constexpr float kPlayerHpHeartSize = 32.f;
	constexpr float kPlayerHpHeartGap = 8.f;
	constexpr float kPlayerHpBarHeight = 18.f;

	playerHpHeart_ = static_cast<UI::Image*>(
		uiManager_.root()->addChild(std::make_unique<UI::Image>())
	);
	playerHpHeart_->name    = "playerHpHeart";
	playerHpHeart_->anchor  = UI::Anchors::TopLeft;
	playerHpHeart_->pivot   = UI::Pivots::TopLeft;
	playerHpHeart_->width   = UI::DimValue::px(kPlayerHpHeartSize);
	playerHpHeart_->height  = UI::DimValue::px(kPlayerHpHeartSize);
	playerHpHeart_->offsetX = UI::DimValue::px(kPlayerHpUiX);
	playerHpHeart_->offsetY = UI::DimValue::px(kPlayerHpUiY - (kPlayerHpHeartSize - kPlayerHpBarHeight) * 0.5f);
	playerHpHeart_->texture = assetManager_.playerHpHeart();

	playerHpBar_ = static_cast<UI::ProgressBar*>(
		uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
	);
	playerHpBar_->name    = "playerHpBar";
	playerHpBar_->anchor  = UI::Anchors::TopLeft;
	playerHpBar_->pivot   = UI::Pivots::TopLeft;
	playerHpBar_->width   = UI::DimValue::px(300.f);
	playerHpBar_->height  = UI::DimValue::px(kPlayerHpBarHeight);
	playerHpBar_->offsetX = UI::DimValue::px(kPlayerHpUiX + kPlayerHpHeartSize + kPlayerHpHeartGap);
	playerHpBar_->offsetY = UI::DimValue::px(kPlayerHpUiY);
	playerHpBar_->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
	playerHpBar_->fillColor = { 1.0f, 0.0f, 0.0f, 1.00f };
	playerHpBar_->setProgress(1.f);

	playerHpText_ = static_cast<UI::Label*>(
		uiManager_.root()->addChild(std::make_unique<UI::Label>())
	);
	playerHpText_->name    = "playerHpText";
	playerHpText_->anchor  = UI::Anchors::TopLeft;
	playerHpText_->pivot   = UI::Pivots::TopLeft;
	playerHpText_->width   = UI::DimValue::px(300.f);
	playerHpText_->height  = UI::DimValue::px(kPlayerHpBarHeight);
	playerHpText_->offsetX = playerHpBar_->offsetX;
	playerHpText_->offsetY = playerHpBar_->offsetY;
	playerHpText_->zOrder  = playerHpBar_->zOrder + 1;
	playerHpText_->setTextHAlign(UI::TextHAlign::Center);
	playerHpText_->setTextVAlign(UI::TextVAlign::Center);
	playerHpText_->setFontSize(14.0f);
	playerHpText_->setTextColor(1.0f, 1.0f, 1.0f, 1.0f);
	playerHpText_->setText(L"100 / 100");

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

	effectDropdown_ = static_cast<UI::Dropdown*>(
		uiManager_.root()->addChild(std::make_unique<UI::Dropdown>())
	);
	effectDropdown_->name    = "effectDropdown";
	effectDropdown_->anchor  = UI::Anchors::TopRight;
	effectDropdown_->pivot   = UI::Pivots::TopRight;
	effectDropdown_->offsetX = UI::DimValue::px(-12.f);
	effectDropdown_->offsetY = UI::DimValue::px(12.f);
	effectDropdown_->width   = UI::DimValue::px(180.f);
	effectDropdown_->setup({ "Slash Wave", "Slash Combo", "Slash 7", "Slash 1", "Spikes", "Crystals Front Attack", "AoE Slash Green", "Red Energy Explosion", "Crystals Cross Fade", "Arrow", "Arrow Volley", "Arrow Rain", "Energy Explosion Arrow", "Tornado Shot", "Piercing", "Piercing Slash", "Piercing Circle Slash", "Piercing Multi" });
	effectDropdown_->onSelectionChanged = [this](int idx) {
		currentEffect_ = static_cast<SwordEffect>(idx);
	};
	hiZStatsLabel_ = static_cast<UI::Label*>(
		uiManager_.root()->addChild(std::make_unique<UI::Label>())
	);
	hiZStatsLabel_->name    = "hiZStatsLabel";
	hiZStatsLabel_->anchor  = UI::Anchors::TopRight;
	hiZStatsLabel_->pivot   = UI::Pivots::TopRight;
	hiZStatsLabel_->width   = UI::DimValue::px(300.0f);
	hiZStatsLabel_->height  = UI::DimValue::px(60.0f);
	hiZStatsLabel_->offsetX = UI::DimValue::px(-40.f);
	hiZStatsLabel_->offsetY = UI::DimValue::px(50.f);
	hiZStatsLabel_->setTextHAlign(UI::TextHAlign::Trailing);
	hiZStatsLabel_->setTextVAlign(UI::TextVAlign::Top);
	hiZStatsLabel_->setFontSize(18.0f);
	hiZStatsLabel_->setTextColor(0.2f, 1.0f, 0.2f, 1.0f);
	hiZStatsLabel_->setText(L"HiZ: OFF");

	// 1000개 이상의 render object가 필요하다면 여기를 수정
	// hi-z culling 대상 개수
	gfx_.setMaxRenderObjectId(1000u);
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

void Game::setupPlayer(const PlayerInfo& playerInfo) {
	player_ = std::make_shared<Player>();

	player_->setId(playerInfo.playerId);
	player_->setPos(DirectX::XMLoadFloat3(&playerInfo.pos));
	player_->setOrient(DirectX::XMLoadFloat4(&playerInfo.orient));
	player_->setScale(DirectX::XMLoadFloat3(&playerInfo.scale));
	player_->setModel(assetManager_.modelPlayer());
	player_->setAnimBlender(animSystem_, assetManager_);
	player_->setHp(playerInfo.hp);
	player_->setMaxHp(playerInfo.maxHp);
	player_->enableBVRendering();

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

	setParticle();

	// Compile skills and build dispatch context (online mode: prediction-only, no damage events).
	{
		SkillCompiler compiler;
		const Skeleton* pSkeleton = player_->model() ? &player_->model()->skeleton : nullptr;
		auto assets = compiler.compileAll("../resources/skills", pSkeleton);
		skillSystem_.registerAssets(std::move(assets));

		skillObjectById_.assign(256, nullptr);
		player_->setFaction(Faction::Players);
		skillObjectById_[player_->getId()] = player_.get();
		// Register goblins that arrived before setupPlayer() was called.
		for (auto& [gobId, goblin] : idGoblinMap_) {
			auto sid = static_cast<size_t>(gobId);
			if (sid >= skillObjectById_.size()) skillObjectById_.resize(sid + 1, nullptr);
			skillObjectById_[sid] = goblin.get();
		}

		skillVfxById_.assign(2, nullptr);
		skillVfxById_[1] = &swordSlash1Effect_;   // effects/sword_slash_1.json

		skillCtx_.objectById          = skillObjectById_.data();
		skillCtx_.objectByIdSize      = static_cast<int>(skillObjectById_.size());
		skillCtx_.vfxById             = skillVfxById_.data();
		skillCtx_.vfxByIdSize         = static_cast<int>(skillVfxById_.size());
		skillCtx_.camera              = &camera_;
		skillCtx_.clientPredictionOnly = true;
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

	{
		auto* bar = static_cast<UI::ProgressBar*>(
			uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
		);
		bar->anchor    = UI::Anchors::TopLeft;
		bar->pivot     = UI::Pivots::TopLeft;
		bar->width     = UI::DimValue::px(80.f);
		bar->height    = UI::DimValue::px(8.f);
		bar->fillColor = { 0.2f, 0.6f, 1.0f, 1.f };
		bar->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
		bar->visible   = false;
		otherPlayerHpBars_[otherPlayerInfo.objectId] = { otherPlayer.get(), bar };
	}

	otherPlayer->setRenderObjectId(nextRenderObjId_++);

	if (!skillObjectById_.empty()) {
		auto id = static_cast<size_t>(otherPlayerInfo.objectId);
		if (id >= skillObjectById_.size()) skillObjectById_.resize(id + 1, nullptr);
		skillObjectById_[id] = otherPlayer.get();
	}

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

	{
		auto* bar = static_cast<UI::ProgressBar*>(
			uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
		);
		bar->anchor    = UI::Anchors::TopLeft;
		bar->pivot     = UI::Pivots::TopLeft;
		bar->width     = UI::DimValue::px(80.f);
		bar->height    = UI::DimValue::px(8.f);
		bar->fillColor = { 0.2f, 0.6f, 1.0f, 1.f };
		bar->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
		bar->visible   = false;
		otherPlayerHpBars_[otherPlayerInfo.playerId] = { otherPlayer.get(), bar };
	}

	otherPlayer->setRenderObjectId(nextRenderObjId_++);

	if (!skillObjectById_.empty()) {
		auto id = static_cast<size_t>(otherPlayerInfo.playerId);
		if (id >= skillObjectById_.size()) skillObjectById_.resize(id + 1, nullptr);
		skillObjectById_[id] = otherPlayer.get();
	}

	otherPlayers_.push_back(otherPlayer);
	idPlayerMap_[otherPlayerInfo.playerId] = otherPlayer;
}

void Game::createGoblin(const ObjectInfo& goblinInfo) {
	auto goblin = std::make_shared<Goblin>();

	goblin->setId(goblinInfo.objectId);
	goblin->setPos(DirectX::XMLoadFloat3(&goblinInfo.pos));
	goblin->setOrient(DirectX::XMLoadFloat4(&goblinInfo.orient));
	goblin->setScale(DirectX::XMLoadFloat3(&goblinInfo.scale));
	goblin->setModel(assetManager_.modelGoblin());
	goblin->setAnimBlender(animSystem_, assetManager_);

	if (goblin->model() && goblin->model()->ragdollDef) {
		goblin->ragdoll().build(
			goblin->model()->skeleton,
			*goblin->model()->ragdollDef,
			physicsWorld_
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

	// Register goblin in skill system's object lookup table.
	if (!skillObjectById_.empty()) {
		auto id = static_cast<size_t>(goblinInfo.objectId);
		if (id >= skillObjectById_.size()) skillObjectById_.resize(id + 1, nullptr);
		skillObjectById_[id] = goblin.get();
	}

	goblins_.push_back(goblin);
	idGoblinMap_[goblinInfo.objectId] = goblin;
}

void Game::createStronghold(const ObjectInfo& info) {
	auto sh = std::make_shared<Stronghold>();   // placeholder mesh (cube); structure model TBD

	sh->setId(info.objectId);
	sh->setPos(DirectX::XMLoadFloat3(&info.pos));
	sh->setOrient(DirectX::XMLoadFloat4(&info.orient));
	sh->setScale(DirectX::XMLoadFloat3(&info.scale));
	sh->setModel(assetManager_.modelCube());
	sh->setFaction(Faction::Monsters);
	// HP/maxHp arrive in the enter packet (server-authoritative).
	sh->setHp(info.hp);
	sh->setMaxHp(info.maxHp);
	sh->setRenderObjectId(nextRenderObjId_++);

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
	// 예시: 특정 구역 진입 시 카메라/사운드 연출을 트리거하는 자리.
	//   clientZoneSystem_.on("forest_bgm", ZoneEvent::Enter, [this](Zone&){ /* BGM 전환 */ });
	//   clientZoneSystem_.on("forest_bgm", ZoneEvent::Leave, [this](Zone&){ /* BGM 복귀 */ });
}

// 서버 권위 존 상태 동기화(S_ZoneState). state==1이면 아레나 진입으로 간주해
// "Wall" 마커(WallHobgoblin_0/1)로부터 로컬 충돌 벽을 생성하여 예측 플레이어가
// 통과하지 못하게 한다. state==0이면 벽을 제거한다. 벽은 렌더링하지 않는다(가상의 벽).
void Game::onZoneState( uint16 zoneId, uint8 state ) {
	zoneStates_[zoneId] = state;
	std::cout << "[Zone] onZoneState zoneId=" << zoneId << " state=" << (int)state << '\n';

	if ( state == 1 ) {
		if ( !barriers_.empty() ) return;   // already built (one-shot per arena)
		for ( const auto& m : chunkManager_.markers() ) {
			if ( m.type != "Wall" ) continue;
			if ( m.name != "WallHobgoblin_0" && m.name != "WallHobgoblin_1" ) continue;

			auto bar = std::make_shared<Cube>();
			bar->setModel( assetManager_.modelCube() );   // unit cube BVH -> scaled collision shape
			bar->setFaction( Faction::Neutral );
			bar->setPos( m.pos );
			bar->setOrient( m.orient );
			bar->setScale( m.scale );
			bar->body().setMotionType( MotionType::Static );
			bar->body().snapToCurrent();
			physicsWorld_.registerBody( &bar->body(), [p = bar.get()]() { p->rebuildBodyBVH(); } );
			barriers_.push_back( std::move( bar ) );

			std::cout << "[Zone] local wall built: '" << m.name << "' at ("
			          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
		}
	}
	else {
		for ( auto& b : barriers_ ) physicsWorld_.unregisterBody( &b->body() );
		barriers_.clear();
	}
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

	animSystem_.untrackAnimBlender(itPlayer->get()->renderState().animBlender.get());
	physicsWorld_.unregisterBody(&(*itPlayer)->body());

	// Stop any skills this player owns and drop the skill-system reference before
	// the Object is destroyed, so checkHitboxCollisions never dereferences a
	// dangling pointer through skillObjectById_.
	skillSystem_.interruptAll(static_cast<i32t>(playerId), skillCtx_);
	if (playerId >= 0 && static_cast<size_t>(playerId) < skillObjectById_.size())
		skillObjectById_[playerId] = nullptr;

	if (auto it = otherPlayerHpBars_.find(playerId); it != otherPlayerHpBars_.end()) {
		uiManager_.root()->removeChild(it->second.hpBar);
		otherPlayerHpBars_.erase(it);
	}

	otherPlayers_.erase(itPlayer);
	idPlayerMap_.erase( playerId );
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

void Game::movePlayer(uint16 playerId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 velocity) {
	auto player = idPlayerMap_[playerId];

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

void Game::rotatePlayer(uint16 playerId, float yawRad) {
	auto player = idPlayerMap_[playerId];

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
}

void Game::moveGoblin(uint16 npcId, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 orient, DirectX::XMFLOAT3 velocity) {
	auto goblin = idGoblinMap_[npcId];

	DISPLAY_ERROR_STR(goblin != nullptr,
		"[Game Error] Game::moveGoblin: 이동하려는 고블린이 존재하지 않습니다.\n",
		false
	);

	if (goblin == nullptr) {
		return;
	}

	if (goblin->isDead()) {
		return;
	}

	goblin->body().advanceState();
	goblin->setCurrPos(DirectX::XMLoadFloat3(&pos));
	goblin->setOrient(DirectX::XMLoadFloat4(&orient));
	goblin->setVelocity(DirectX::XMLoadFloat3(&velocity));
}

void Game::onNpcAttack( uint16 npcId ) {
	auto npc = idGoblinMap_[ npcId ];

	DISPLAY_ERROR_STR( npc != nullptr,
		"[Game Error] Game::onNpcAttack: 공격하는 NPC가 존재하지 않습니다.\n",
		false
	);

	if ( npc == nullptr ) {
		return;
	}

	holdEvent( eventList_, EvAttack( npcId ) );
}

void Game::onPlayerAttack( uint16 attackerId ) {
	holdEvent( eventList_, EvAttack( attackerId ) );
}

void Game::applyHit( uint16 targetId, int32 newHp ) {
	// HP바 가시성은 EventBus가 다루지 않는 시각 상태이므로 여기서 갱신한다(고블린/거점 공통).
	if ( auto barIt = goblinHpBars_.find( targetId ); barIt != goblinHpBars_.end() )
		barIt->second.hpBarVisibleSeconds = 5.f;
	if ( auto it = strongholdHpBars_.find( targetId ); it != strongholdHpBars_.end() )
		it->second.hpBarVisibleSeconds = 5.f;

	// HP / isDead / 래그돌 / 피격·사망 애니메이션은 EvHit/EvDeath 핸들러(Object::EventBus)가 소유한다.
	// 디스패치 루프(InGameScene)에서 대상 객체의 eventBus로 분배되며, 데미지 넘버도 그곳에서 생성된다.
	// 거점도 이제 Stronghold::EventBus를 가지므로 고블린/플레이어와 동일 경로를 탄다.
	if ( newHp <= 0 )
		holdEvent( eventList_, EvDeath( targetId ) );
	else
		holdEvent( eventList_, EvHit( targetId, newHp ) );
}

void Game::onNpcRespawn( uint16 npcId, int32 newHp, DirectX::XMFLOAT3 spawnPos ) {
	auto npc = idGoblinMap_[ npcId ];

	DISPLAY_ERROR_STR( npc != nullptr,
		"[Game Error] Game::onNpcRespawn: 리스폰하는 NPC가 존재하지 않습니다.\n",
		false
	);

	if ( npc == nullptr ) {
		return;
	}

	npc->setHp( newHp );
	// isDead_ 리셋 및 사망/부활 애니메이션은 EvRespawn 핸들러(EventBus)가 소유한다.
	holdEvent( eventList_, EvRespawn( npcId ) );
	if (npc->ragdoll().isActive())
		npc->ragdoll().deactivate(physicsWorld_);
	npc->setPos( DirectX::XMLoadFloat3( &spawnPos ) );
}

void Game::onSkillStart( uint16 ownerId, uint32 skillAssetId, uint16 elapsedMs ) {
	// Trigger attack animation on the remote player that cast the skill.
	holdEvent( eventList_, EvAttack( ownerId ) );

	// Start skill visuals for the remote owner (clientPredictionOnly — no damage).
	skillSystem_.startSkill(skillAssetId, static_cast<i32t>(ownerId), skillCtx_,
	                        Milliseconds{ static_cast<float>(elapsedMs) });
}

void Game::onSkillHit( uint16 attackerId, uint16 targetId, int32 newHp, uint32 skillAssetId, DirectX::XMFLOAT3 targetVelocity ) {
	// Store hit velocity on goblin before applyHit so ragdoll activation can use it.
	if (newHp <= 0) {
		if (auto it = idGoblinMap_.find(targetId); it != idGoblinMap_.end()) {
			it->second->setRagdollInitVelocity(DirectX::XMLoadFloat3(&targetVelocity));
		}
	}
	applyHit(targetId, newHp);

	// Spawn impact VFX at the target's position.
	const SkillAsset* asset = skillSystem_.findAsset(skillAssetId);
	if (asset && !asset->hitboxDefs.empty()) {
		const u8t vfxId = asset->hitboxDefs[0].onHit.hitVfxId;
		if (vfxId != 0xFF && vfxId < static_cast<u8t>(skillVfxById_.size())
		    && skillVfxById_[vfxId])
		{
			Object* target = nullptr;
			if (player_ && static_cast<uint16>(player_->getId()) == targetId)
				target = player_.get();
			else if (auto it = idPlayerMap_.find(targetId); it != idPlayerMap_.end())
				target = it->second.get();
			else if (auto it = idGoblinMap_.find(targetId); it != idGoblinMap_.end())
				target = it->second.get();

			if (target)
				skillVfxById_[vfxId]->play(target->pos());
		}
	}
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
			Milliseconds{ 100.f }
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
	switch (scene_) {
	case Scene::Lobby:  LobbyScene(deltaTime);  break;
	case Scene::InGame: InGameScene(deltaTime); break;
	}
}

void Game::render() {
	switch (scene_) {
	case Scene::Lobby:  renderLobby();  break;
	case Scene::InGame: renderInGame(); break;
	}
}

void Game::InGameScene(Milliseconds deltaTime) {
	SleepEx(1, true);

	if (player_ == nullptr) {
		return;
	}

	// Skill dispatch context: refresh per-frame pointers.
	skillCtx_.evList         = &eventList_;
	skillCtx_.pTimer         = pTimer_;
	skillCtx_.objectById     = skillObjectById_.data();
	skillCtx_.objectByIdSize = static_cast<int>(skillObjectById_.size());

	// 이전 프레임 속도 저장
	prevVelocity_ = currVelocity_;

	// 입력 처리 (속도는 프레임 간 유지 - processInputGame이 감속/가속 관리)
	processInput(deltaTime);

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
	const Seconds clampedDt = std::min(Seconds(deltaTime), kMaxPhysicsDeltaTime);
	physicUpdateAcc_ += clampedDt;

	// move 패킷 전송 주기 판단
	moveStateSendAcc_ += deltaTime;

	const Seconds effectiveInterval = physicUpdateInterval * static_cast<float>(physicUpdateScaleK_);

	int physicsStepsDone = 0;
	while (physicUpdateAcc_ >= effectiveInterval
		   && physicsStepsDone < kMaxPhysicsStepsPerFrame) {
		physicsWorld_.step(effectiveInterval);
		// 물리 적분 직후, 로컬 플레이어를 다른 플레이어와 reciprocal soft separation.
		// (setCurrPos로 curr만 갱신 → 렌더 보간의 prev는 보존된다)
		resolvePlayerSeparation(effectiveInterval);
		physicUpdateAcc_ -= effectiveInterval;
		++physicsStepsDone;
	}

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
	for (auto& g : goblins_) {
		g->rebuildBodyBVH();
	}

	if (!playerDead_)
		skillSystem_.update(deltaTime, skillCtx_);

	// 이벤트 디스패치
	//
	// 애니메이션 트리거(피격/공격/사망/부활)는 trigger* 직접 호출 대신
	// EventBus로 일원화한다. 패킷 핸들러(프레임 시작 SleepEx)와 스킬 시스템(update)이
	// eventList_에 post한 이벤트를, 대상 객체의 eventBus로 분배한다.
	// (서버 권위 게임이므로 데미지 적용은 applyHit이 이미 끝냈고, 여기서는 HP/연출/애니메이션만 다룬다.)
	// 객체 갱신(아래) 이전에 처리해 같은 프레임에 애니메이션 상태가 반영되도록 한다.
	{
		const Seconds evDt = std::chrono::duration_cast<Seconds>(deltaTime);

		auto resolveObject = [&](i32t id) -> Object* {
			if (player_ && player_->getId() == id) return player_.get();
			if (auto it = idPlayerMap_.find(static_cast<uint16>(id)); it != idPlayerMap_.end())
				return it->second.get();
			if (auto it = idGoblinMap_.find(static_cast<uint16>(id)); it != idGoblinMap_.end())
				return it->second.get();
			if (auto it = strongholdHpBars_.find(static_cast<uint16>(id)); it != strongholdHpBars_.end())
				return it->second.obj;   // Stronghold* upcast → Object*
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

			// Combat feedback (game thread, no locking): compute damage from the
			// pre-receive HP since the EvHit/EvDeath handler below mutates hp/dead.
			if (pEv->type == EventType::Hit || pEv->type == EventType::Death) {
				const int prevHp = obj->hp();
				const int dmg = (pEv->type == EventType::Hit)
					? prevHp - static_cast<const EvHit*>(pEv)->hp
					: prevHp;   // Death: remaining HP is the killing-blow damage
				if (dmg > 0) {
					const uint16 targetId = static_cast<uint16>(routeId);
					DamageKind kind = DamageKind::EnemyHit;
					if (player_ && player_->getId() == routeId)
						kind = DamageKind::PlayerHit;
					else if (strongholdHpBars_.find(targetId) != strongholdHpBars_.end())
						kind = DamageKind::StrongholdHit;
					const mu::Vec3 anchor = obj->renderState().pos
						+ mu::Vec3{ 0.f, damageNumberSystem_.tuning().worldHeadOffsetY, 0.f };
					damageNumberSystem_.spawn(anchor, dmg, kind, targetId);
				}
				// Kill count: a goblin death not already counted (guards duplicate EvDeath).
				if (pEv->type == EventType::Death && killCountWidget_ && !obj->isDead()
					&& idGoblinMap_.find(static_cast<uint16>(routeId)) != idGoblinMap_.end())
				{
					killCountWidget_->addKill();
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
	player_->update(deltaTime, tPhysicInterpolation );

	for ( auto& obj : otherPlayers_ ) {
		obj->netInterpAcc_ += deltaTime;
		const float tNet = std::min(obj->netInterpAcc_ / obj->netInterpDuration_, 1.f);

		// 패킷 2개 간격(100ms) 이상 새 패킷이 없으면 멈춘 것으로 확정.
		// 1개 간격(50ms)이면 정상 패킷 도착 타이밍과 겹쳐 oscillation이 발생하므로 2배로 여유를 준다.
		if ( obj->netInterpAcc_ >= obj->netInterpDuration_ * 2.f ) {
			obj->setVelocity(mu::Vec3{});
		}

		obj->update( deltaTime, tNet );
	}

	for (auto& goblin : goblins_) {
		goblin->update(deltaTime, tPhysicInterpolation);
	}

	for (auto& sh : strongholds_) {
		sh->update(deltaTime, tPhysicInterpolation);
	}

	animSystem_.updatePriorities(
		std::chrono::duration_cast<Seconds>(deltaTime),
		player_->pos()
	);

	// 지형 청크 스트리밍 틱 (카메라 갱신 전에 호출).
	chunkManager_.update(player_->pos(), deltaTime);

	// 연출 존 판정 (로컬 예측 플레이어 위치 기준, 패킷 없음).
	clientZoneSystem_.update(player_->pos());

	camera_.update(deltaTime);
	dirLight_.update(deltaTime);
	dirLight_.updateCSMCascades(camera_.view(), camera_.proj(), assetConfigs_.cascade, assetConfigs_.shadowMap);

	// 애니메이션 업데이트
	animSystem_.update(0.01s);

	// Ragdoll 활성화/동기화: animSystem_.update() 이후 finalXformData 확정된 시점에 실행
	{
		auto activateRagdollIfPending = [&](Goblin& g) {
			if (!g.ragdollPendingActivation()) return;
			g.setRagdollPendingActivation(false);
			Ragdoll& rd = g.ragdoll();
			if (!rd.isBuilt() || !g.animBlender() || !g.model()) return;
			rd.seedFromFinalXforms(
				g.animBlender()->finalXformData(),
				g.model()->skeleton,
				g.renderState().world
			);
			rd.buildPassengers(g.model()->skeleton, g.animBlender()->finalXformData());
			rd.activate(physicsWorld_);

			// Apply death velocity so the ragdoll flies in the knockback direction.
			const mu::Vec3 initVel = g.ragdollInitVelocity();
			if (initVel.len2() > 0.01f) {
				for (auto& rb : rd.bones()) {
					if (rb.body) rb.body->setLinearVel(initVel);
				}
				g.setRagdollInitVelocity(mu::Vec3{});
			}

			// Per-bone random noise impulse, biased toward the death velocity direction.
			// velDir * kNoiseBias + randomUnit * (1-kNoiseBias) gives a cosine-like
			// distribution: closer to velDir is more probable, but still varied.
			constexpr float kNoiseBias = 0.6f;
			const mu::Vec3 velDir = (initVel.len2() > 0.01f)
			    ? mu::Vec3(mu::NVec3(initVel)) : mu::Vec3{};
			for (const auto& rb : rd.bones()) {
				if (rb.noiseImpulse <= 0.f || !rb.body) continue;
				mu::Vec3 rnd(rand(-1.f, 1.f), rand(-1.f, 1.f), rand(-1.f, 1.f));
				if (rnd.len2() < 1e-8f) rnd = mu::Vec3(0.f, 0.f, 1.f);
				mu::Vec3 dir = velDir * kNoiseBias + mu::Vec3(mu::NVec3(rnd)) * (1.f - kNoiseBias);
				if (dir.len2() < 1e-8f) dir = mu::Vec3(0.f, 0.f, 1.f);
				rb.body->applyImpulse(mu::Vec3(mu::NVec3(dir)) * rb.noiseImpulse, rb.body->pos());
			}
		};

		auto syncRagdollToAnim = [&](Goblin& g) {
			Ragdoll& rd = g.ragdoll();
			if (!rd.isActive() || !g.animBlender() || !g.model()) return;
			rd.syncToFinalXforms(
				g.animBlender()->finalXformData(),
				g.model()->skeleton,
				g.renderState().world
			);
			g.rebuildBodyBVH();
		};

		for (auto& goblin : goblins_) {
			activateRagdollIfPending(*goblin);
			syncRagdollToAnim(*goblin);
		}
	}

	// HP 바 위치 및 값 갱신
	{
		constexpr float kBarHalfWidth = 40.f;

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
		}

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
				entry.hpBar->offsetX = UI::DimValue::px(sx - kBarHalfWidth);
				entry.hpBar->offsetY = UI::DimValue::px(sy);
				entry.hpBar->setProgress(
					static_cast<float>(entry.player->hp()) /
					static_cast<float>(entry.player->maxHp())
				);
			}
		}

		const float dtSec = std::chrono::duration<float>(deltaTime).count();
		for (auto& [id, entry] : goblinHpBars_) {
			if (!entry.goblin || entry.goblin->hp() <= 0) {
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
				entry.hpBar->offsetX = UI::DimValue::px(sx - kBarHalfWidth);
				entry.hpBar->offsetY = UI::DimValue::px(sy);
				entry.hpBar->setProgress(
					static_cast<float>(entry.goblin->hp()) /
					static_cast<float>(entry.goblin->maxHp())
				);
			}
		}

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
				entry.hpBar->offsetX = UI::DimValue::px(sx - kBarHalfWidth);
				entry.hpBar->offsetY = UI::DimValue::px(sy);
				entry.hpBar->setProgress(
					static_cast<float>(entry.obj->hp()) /
					static_cast<float>(entry.obj->maxHp())
				);
			}
		}

		if (hiZStatsLabel_) {
			if (gfx_.isHiZCullEnabled()) {
				const auto stats = gfx_.getHiZStats();
				wchar_t buf[64];
				swprintf_s(buf, 64, L"HiZ: ON  Visible %u / %u", stats.visible, stats.total);
				hiZStatsLabel_->setText(buf);
			} else {
				hiZStatsLabel_->setText(L"HiZ: OFF");
			}
		}

		uiManager_.layout();
		uiManager_.update(std::chrono::duration<float>(deltaTime).count(), gfx_, gfx_.defaultFont());
	}

	// particle update
	if (player_) {
		flameParticleSystem_.update(deltaTime);
		smokeParticleSystem_.update(deltaTime);
		swordSlash1Effect_.update(deltaTime);
		swordSlash7Effect_.update(deltaTime);
		swordSlashComboEffect_.update(deltaTime);
		slashWaveEffect_.update(deltaTime);
		spikesAttackEffect_.update(deltaTime);
		piercingEffect_.update(deltaTime);
		piercingMultiEffect_.update(deltaTime);
		piercingSlashEffect_.update(deltaTime);
		piercingCircleSlashEffect_.update(deltaTime);
		crystalsFrontAttackEffect_.update(deltaTime);
		aoESlashGreenEffect_.update(deltaTime);
		crystalsCrossFadeEffect_.update(deltaTime);
		redEnergyExplosionEffect_.update(deltaTime);
		arrowEffect_.update(deltaTime);
		arrowVolleyMuzzleEffect_.update(deltaTime);
		arrowVolleyEffect_.update(deltaTime);
		arrowRainMuzzleEffect_.update(deltaTime);
		arrowRainEffect_.update(deltaTime);
		energyExplosionArrowEffect_.update(deltaTime);
		tornadoShotEffect_.update(deltaTime);
		tornadoMuzzleEffect_.update(deltaTime);
		tornadoHitEffect_.update(deltaTime);
		dustParticleSystem_.update(deltaTime);
		debugBVView_.update(deltaTime);

		if ( tornadoShotActive_ ) {
			constexpr float kSpeed    = 10.f;
			constexpr float kDuration = 0.8f;
			const Seconds dt = deltaTime;
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

	for (auto& goblin : goblins_) {
		goblin->render(gfx_);
	}

	for (auto& sh : strongholds_) {
		if (sh->isDead()) continue;   // hide destroyed structure (isDead set by EvDeath)
		sh->render(gfx_);
	}

	camera_.updateGFX(gfx_);
	dirLight_.render(gfx_);

	flameParticleSystem_.render(gfx_);
	smokeParticleSystem_.render(gfx_);
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
		chunkManager_.submitDrawEvents(gfx_);
		gfx_.addFrameData(TerrainPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
		gfx_.addFrameData(TerrainDeferredPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
	}

	// Floating damage numbers: drawn just before the HUD so they share the UI pass
	// (always-on-top, world-anchored via worldToScreen, screen-uniform size).
	damageNumberSystem_.render(gfx_, camera_, uiManager_.screenWidth(), uiManager_.screenHeight());

	uiManager_.render(gfx_);

	auto frameDataUI = UIPipeline::FrameData{
		.screenWidth = static_cast<float>( gClientRect.right - gClientRect.left ),
		.screenHeight = static_cast<float>( gClientRect.bottom - gClientRect.top )
	};
	gfx_.addFrameData(frameDataUI);

	gfx_.render();
	applyHiZCulling();
}

// ===========================================================================
// Lobby Scene
// ===========================================================================

void Game::enterLobby() {
	scene_      = Scene::Lobby;
	lobbyState_ = LobbyState::MainMenu;
	pendingStart_ = false;

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
	// (buildLobbyUI가 이 텍스처를 위젯에 연결하므로 빌드 전에 로드해야 한다.)
	loadLobbyTextures();

	buildLobbyUI();
	refreshLobbyUI();

	// 최소 로드로 로비 진입 후, 인게임 리소스를 백그라운드로 로드한다.
	startInGameAssetLoad();
}

void Game::loadLobbyTextures() {
	gfx_.addRequestTextureLoad(RequestTextureLoad{
		.name            = "LobbyBg",
		.texturePath     = "../resources/UI/ui_lobby_bg.dds",
		.pDest           = &lobbyBgTex_,
		.pTexHashMap     = &lobbyTexHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	});
	gfx_.addRequestTextureLoad(RequestTextureLoad{
		.name            = "LobbyLogo",
		.texturePath     = "../resources/UI/ui_lobby_logo.dds",
		.pDest           = &lobbyLogoTex_,
		.pTexHashMap     = &lobbyTexHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	});

	// 9-slice UI 프레임(패널/버튼/입력창) — 클램프 샘플러로 모서리 번짐 방지.
	gfx_.addRequestTextureLoad(RequestTextureLoad{
		.name            = "LobbyPanel",
		.texturePath     = "../resources/UI/ui_panel_frame.dds",
		.pDest           = &lobbyPanelTex_,
		.pTexHashMap     = &lobbyTexHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	});
	gfx_.addRequestTextureLoad(RequestTextureLoad{
		.name            = "LobbyBtnPrimary",
		.texturePath     = "../resources/UI/ui_btn_primary.dds",
		.pDest           = &lobbyBtnPrimaryTex_,
		.pTexHashMap     = &lobbyTexHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	});
	gfx_.addRequestTextureLoad(RequestTextureLoad{
		.name            = "LobbyBtnSecondary",
		.texturePath     = "../resources/UI/ui_btn_secondary.dds",
		.pDest           = &lobbyBtnSecondaryTex_,
		.pTexHashMap     = &lobbyTexHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	});
	gfx_.addRequestTextureLoad(RequestTextureLoad{
		.name            = "LobbyInput",
		.texturePath     = "../resources/UI/ui_input.dds",
		.pDest           = &lobbyInputTex_,
		.pTexHashMap     = &lobbyTexHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	});

	// 인게임 백그라운드 로드가 시작되기 전(요청 큐가 비어 있을 때) 즉시 처리한다.
	gfx_.loadRequestedAssets();
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

void Game::buildLobbyUI() {
	auto* root = uiManager_.root();

	const float screenW = uiManager_.screenWidth();
	const float screenH = uiManager_.screenHeight();
	const float mainPanelW = std::min(560.f, std::max(360.f, screenW - 40.f));
	const float mainPanelH = 500.f;
	// 대기실은 3D 맵 위에 얹히는 넓은 스쿼드 스테이지 레이아웃.
	const float roomPanelW = std::min(1120.f, std::max(640.f, screenW - 40.f));
	const float roomPanelH = std::min(700.f,  std::max(460.f, screenH - 40.f));

	const XMFLOAT4 skyBlue     = { 0.529f, 0.808f, 0.980f, 1.f };
	const XMFLOAT4 ink         = { 0.090f, 0.125f, 0.200f, 1.f };
	const XMFLOAT4 muted       = { 0.384f, 0.439f, 0.518f, 1.f };
	const XMFLOAT4 surface     = { 1.000f, 1.000f, 1.000f, 1.f };
	const XMFLOAT4 surfaceSoft = { 0.953f, 0.976f, 0.992f, 1.f };
	const XMFLOAT4 primary     = { 0.082f, 0.365f, 0.545f, 1.f };
	const XMFLOAT4 primaryDark = { 0.055f, 0.286f, 0.435f, 1.f };
	const XMFLOAT4 primarySoft = { 0.847f, 0.937f, 0.988f, 1.f };

	lobbyRoot_ = root->addChild(std::make_unique<UI::UIElement>());
	lobbyRoot_->name    = "lobbyRoot";
	lobbyRoot_->anchor  = UI::Anchors::TopLeft;
	lobbyRoot_->pivot   = UI::Pivots::TopLeft;
	lobbyRoot_->offsetX = UI::DimValue::px(0.f);
	lobbyRoot_->offsetY = UI::DimValue::px(0.f);
	lobbyRoot_->width   = UI::DimValue::px(screenW);
	lobbyRoot_->height  = UI::DimValue::px(screenH);

	auto applyRect = [](UI::UIElement* e, UI::Anchor anchor, UI::Pivot pivot,
	                    float x, float y, float w, float h) {
		e->anchor  = anchor;
		e->pivot   = pivot;
		e->offsetX = UI::DimValue::px(x);
		e->offsetY = UI::DimValue::px(y);
		e->width   = UI::DimValue::px(w);
		e->height  = UI::DimValue::px(h);
	};

	auto makeSolid = [&](UI::UIElement* parent, const std::string& name,
	                     float x, float y, float w, float h, XMFLOAT4 color,
	                     int zOrder = 0) -> UI::Button* {
		auto* block = static_cast<UI::Button*>(parent->addChild(std::make_unique<UI::Button>()));
		block->name = name;
		applyRect(block, UI::Anchors::TopLeft, UI::Pivots::TopLeft, x, y, w, h);
		block->interactive = false;
		block->bgColor = color;
		block->zOrder = zOrder;
		return block;
	};

	auto makeGroup = [&](UI::UIElement* parent, const std::string& name,
	                     float w, float h) -> UI::UIElement* {
		auto* group = parent->addChild(std::make_unique<UI::UIElement>());
		group->name = name;
		applyRect(group, UI::Anchors::Center, UI::Pivots::Center, 0.f, 0.f, w, h);
		return group;
	};

	auto makeLabel = [&](UI::UIElement* parent, const std::wstring& text,
	                     float x, float y, float fontSize, float w, float h,
	                     XMFLOAT4 color, UI::TextHAlign hAlign = UI::TextHAlign::Leading,
	                     int zOrder = 1) -> UI::Label* {
		auto* lbl = static_cast<UI::Label*>(parent->addChild(std::make_unique<UI::Label>()));
		applyRect(lbl, UI::Anchors::TopLeft, UI::Pivots::TopLeft, x, y, w, h);
		lbl->setTextHAlign(hAlign);
		lbl->setTextVAlign(UI::TextVAlign::Center);
		lbl->setFontSize(fontSize);
		lbl->setTextColor(color.x, color.y, color.z, color.w);
		lbl->setText(text);
		lbl->zOrder = zOrder;
		return lbl;
	};

	auto makeButton = [&](UI::UIElement* parent, const std::wstring& text,
	                      float x, float y, float w, float h,
	                      XMFLOAT4 normal, XMFLOAT4 hovered, XMFLOAT4 pressed,
	                      float fontSize, std::function<void()> onClick,
	                      XMFLOAT4 textColor) -> UI::Button* {
		auto* btn = static_cast<UI::Button*>(parent->addChild(std::make_unique<UI::Button>()));
		applyRect(btn, UI::Anchors::TopLeft, UI::Pivots::TopLeft, x, y, w, h);
		btn->bgColor        = normal;
		btn->bgColorHovered = hovered;
		btn->bgColorPressed = pressed;
		btn->onClick = std::move(onClick);
		btn->zOrder = 1;

		auto* lbl = static_cast<UI::Label*>(btn->addChild(std::make_unique<UI::Label>()));
		applyRect(lbl, UI::Anchors::TopLeft, UI::Pivots::TopLeft, 0.f, 0.f, w, h);
		lbl->setTextHAlign(UI::TextHAlign::Center);
		lbl->setTextVAlign(UI::TextVAlign::Center);
		lbl->setFontSize(fontSize);
		lbl->setTextColor(textColor.x, textColor.y, textColor.z, textColor.w);
		lbl->setText(text);
		return btn;
	};

	// 9-slice 텍스처 스타일 적용 헬퍼. 텍스처 로드 실패 시 기존 단색을 그대로 둔다.
	// (slice 경계/코너 px는 에셋 모양에 맞춘 튜닝값 — 어색하면 이 숫자만 조정.)
	auto stylePanel = [&](UI::Button* b) -> UI::Button* {
		if (b && lobbyPanelTex_.res) {
			b->texNormal = &lobbyPanelTex_;
			b->sliceUvBorderX = 0.30f; b->sliceUvBorderY = 0.30f;
			b->sliceCornerX = 40.f;    b->sliceCornerY = 40.f;
		}
		return b;
	};
	auto stylePrimary = [&](UI::Button* b) -> UI::Button* {
		if (b && lobbyBtnPrimaryTex_.res) {
			b->texNormal = &lobbyBtnPrimaryTex_;
			b->sliceUvBorderX = 0.40f; b->sliceUvBorderY = 0.40f;
			b->sliceCornerX = 22.f;    b->sliceCornerY = 22.f;
		}
		return b;
	};
	auto styleSecondary = [&](UI::Button* b) -> UI::Button* {
		if (b && lobbyBtnSecondaryTex_.res) {
			b->texNormal = &lobbyBtnSecondaryTex_;
			b->sliceUvBorderX = 0.40f; b->sliceUvBorderY = 0.40f;
			b->sliceCornerX = 22.f;    b->sliceCornerY = 22.f;
		}
		return b;
	};

	// 배경: 텍스처 로드 실패에 대비해 단색 sky를 가장 뒤(-11)에 깔고,
	// 그 위(-10)에 키 아트를 cover 스케일(종횡비 유지 + 화면 덮기)로 올린다.
	lobbySkyBg_ = makeSolid(lobbyRoot_, "lobbySkyBackground", 0.f, 0.f, screenW, screenH, skyBlue, -11);
	if (lobbyBgTex_.res) {
		const float bgAspect = 1672.f / 941.f;   // ui_lobby_bg.dds 원본 비율
		float bgW, bgH;
		if (screenW / screenH < bgAspect) {       // 화면이 더 세로로 길면 높이 기준
			bgH = screenH;
			bgW = screenH * bgAspect;
		} else {                                   // 화면이 더 가로로 길면 너비 기준
			bgW = screenW;
			bgH = screenW / bgAspect;
		}
		auto* bg = static_cast<UI::Image*>(lobbyRoot_->addChild(std::make_unique<UI::Image>()));
		bg->name    = "lobbyBgImage";
		applyRect(bg, UI::Anchors::Center, UI::Pivots::Center, 0.f, 0.f, bgW, bgH);
		bg->texture = &lobbyBgTex_;
		bg->zOrder  = -10;
		lobbyBgImage_ = bg;
	}

	// ---- 메인 메뉴 (mainView) ----
	mainMenuRoot_ = lobbyRoot_->addChild(std::make_unique<UI::UIElement>());
	mainMenuRoot_->name   = "mainMenuRoot";
	mainMenuRoot_->anchor = UI::Anchors::TopLeft;
	mainMenuRoot_->pivot  = UI::Pivots::TopLeft;
	mainMenuRoot_->width  = UI::DimValue::px(screenW);
	mainMenuRoot_->height = UI::DimValue::px(screenH);

	// 게임 로고(OutLander) — 메인 패널 위쪽 중앙에 배치(투명 PNG 기반 텍스처).
	if (lobbyLogoTex_.res) {
		const float logoAspect = 2172.f / 724.f;          // ui_lobby_logo.dds 원본 비율(약 3:1)
		const float logoH = 110.f;
		const float logoW = logoH * logoAspect;
		auto* logo = static_cast<UI::Image*>(mainMenuRoot_->addChild(std::make_unique<UI::Image>()));
		logo->name    = "lobbyLogoImage";
		applyRect(logo, UI::Anchors::Center, UI::Pivots::BottomCenter,
			0.f, -(mainPanelH * 0.5f + 16.f), logoW, logoH);
		logo->texture = &lobbyLogoTex_;
		logo->zOrder  = 5;
		lobbyLogoImage_ = logo;
	}

	auto* mainPanel = makeGroup(mainMenuRoot_, "mainPanel", mainPanelW, mainPanelH);
	stylePanel(makeSolid(mainPanel, "mainPanelBg", 0.f, 0.f, mainPanelW, mainPanelH, surface));
	makeLabel(mainPanel, L"PROJECT-W MULTIPLAYER", 34.f, 30.f, 13.f, mainPanelW - 68.f, 24.f, primary);
	makeLabel(mainPanel, L"비공개 로비", 34.f, 56.f, 42.f, mainPanelW - 68.f, 58.f, ink);

	auto* createBtn = makeButton(mainPanel, L"", 34.f, 132.f, mainPanelW - 68.f, 72.f,
		primary, primaryDark, primaryDark, 22.f, [this]() { lobbyCreateRoom(); },
		{ 1.f, 1.f, 1.f, 1.f });
	stylePrimary(createBtn);
	makeLabel(createBtn, L"방 만들기", 18.f, 0.f, 24.f, 220.f, 72.f, { 1.f, 1.f, 1.f, 1.f });
	makeLabel(createBtn, L"Create Room", mainPanelW - 220.f, 0.f, 13.f, 130.f, 72.f,
		{ 1.f, 1.f, 1.f, 0.82f }, UI::TextHAlign::Trailing);

	// 방 코드 입력 + 참가 (프로토타입 joinRoomForm 대응)
	// 단색 박스 대신 9-slice 패널 프레임으로 표시(작은 박스라 코너는 줄여 비례 보정).
	{
		auto* joinBox = makeSolid(mainPanel, "joinBox", 34.f, 220.f, mainPanelW - 68.f, 116.f, surface);
		if (lobbyPanelTex_.res) {
			joinBox->texNormal     = &lobbyPanelTex_;
			joinBox->sliceUvBorderX = 0.30f; joinBox->sliceUvBorderY = 0.30f;
			joinBox->sliceCornerX   = 22.f;  joinBox->sliceCornerY   = 22.f;
		} else {
			joinBox->bgColor = surfaceSoft;   // 텍스처 미로드 시 기존 단색 폴백
		}
	}
	makeLabel(mainPanel, L"방 코드로 참가", 52.f, 234.f, 18.f, 240.f, 28.f, ink);
	{
		roomCodeInput_ = static_cast<UI::TextInput*>(
			mainPanel->addChild(std::make_unique<UI::TextInput>()));
		roomCodeInput_->name    = "roomCodeInput";
		applyRect(roomCodeInput_, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
			52.f, 274.f, mainPanelW - 232.f, 48.f);
		roomCodeInput_->bgColor        = { 1.f, 1.f, 1.f, 1.f };
		roomCodeInput_->bgColorFocused = { 0.953f, 0.976f, 0.992f, 1.f };
		roomCodeInput_->textColor        = ink;
		roomCodeInput_->placeholderColor = muted;
		roomCodeInput_->uppercase = true;
		roomCodeInput_->alnumOnly = true;
		roomCodeInput_->setMaxLength(6);
		roomCodeInput_->setPlaceholder(L"JOIN01");
		roomCodeInput_->onSubmit = [this](const std::wstring& code) {
			lobbyJoinRoom(std::string(code.begin(), code.end()));
		};
		if (lobbyInputTex_.res) {
			roomCodeInput_->backgroundTex  = &lobbyInputTex_;
			roomCodeInput_->sliceUvBorderX = 0.15f;
			roomCodeInput_->sliceUvBorderY = 0.42f;
			roomCodeInput_->sliceCornerX   = 18.f;
			roomCodeInput_->sliceCornerY   = 18.f;
			roomCodeInput_->texTintFocused = { 1.10f, 1.10f, 1.10f, 1.f };
		}

		auto* joinBtn = makeButton(mainPanel, L"참가", mainPanelW - 146.f, 274.f, 94.f, 48.f,
			primary, primaryDark, primaryDark, 20.f, [this]() {
				const std::wstring& w = roomCodeInput_->text();
				lobbyJoinRoom(std::string(w.begin(), w.end()));
			}, { 1.f, 1.f, 1.f, 1.f });
		joinBtn->name    = "joinButton";
		stylePrimary(joinBtn);
	}

	mainMenuMsgLabel_ = makeLabel(mainPanel, L"", 34.f, 348.f, 18.f, mainPanelW - 68.f, 30.f,
		{ 0.70f, 0.24f, 0.24f, 1.f }, UI::TextHAlign::Center);

	const float quietW = (mainPanelW - 80.f) * 0.5f;
	styleSecondary(makeButton(mainPanel, L"설정", 34.f, 392.f, quietW, 48.f,
		surfaceSoft, { 0.847f, 0.937f, 0.988f, 1.f }, primarySoft, 20.f,
		[]() { gSharedLog << "[Lobby] 설정 (준비 중)\n"; }, ink));
	styleSecondary(makeButton(mainPanel, L"종료", 46.f + quietW, 392.f, quietW, 48.f,
		surfaceSoft, { 0.847f, 0.937f, 0.988f, 1.f }, primarySoft, 20.f,
		[]() { PostQuitMessage(0); }, ink));

	// ---- 대기실 (lobbyView) : 3D 맵 위에 얹히는 반투명 스쿼드 스테이지 ----
	// 색상은 밝은 3D 배경 위 가독성을 위해 어두운 반투명 scrim + 밝은 텍스트로 구성한다.
	const XMFLOAT4 textLight   = { 0.95f, 0.97f, 1.00f, 1.f };
	const XMFLOAT4 textMuted   = { 0.78f, 0.84f, 0.92f, 1.f };
	const XMFLOAT4 buttonInk   = { 0.07f, 0.11f, 0.18f, 1.f };
	const XMFLOAT4 scrimBar    = { 0.06f, 0.09f, 0.14f, 0.52f };  // 툴바 배경
	const XMFLOAT4 wrBtnAlpha  = { 1.f, 1.f, 1.f, 0.92f };        // 텍스처 버튼 반투명 틴트

	// 텍스처 버튼에 반투명 틴트를 입히는 헬퍼(stylePrimary/Secondary 적용 이후 호출).
	auto fadeBtn = [&](UI::Button* b) -> UI::Button* {
		if (b) { b->texTint = wrBtnAlpha; b->texTintHovered = { 1.08f, 1.08f, 1.08f, 0.96f };
		         b->texTintPressed = { 0.82f, 0.82f, 0.82f, 0.96f }; }
		return b;
	};

	waitingRoomRoot_ = lobbyRoot_->addChild(std::make_unique<UI::UIElement>());
	waitingRoomRoot_->name   = "waitingRoomRoot";
	waitingRoomRoot_->anchor = UI::Anchors::TopLeft;
	waitingRoomRoot_->pivot  = UI::Pivots::TopLeft;
	waitingRoomRoot_->width  = UI::DimValue::px(screenW);
	waitingRoomRoot_->height = UI::DimValue::px(screenH);

	auto* roomPanel = makeGroup(waitingRoomRoot_, "waitingRoomPanel", roomPanelW, roomPanelH);

	const float pad   = 28.f;
	const float gap   = 12.f;
	const float innerW = roomPanelW - 2.f * pad;

	// 헤더
	makeLabel(roomPanel, L"PRIVATE ROOM", pad, 18.f, 13.f, 280.f, 22.f, textLight);
	makeLabel(roomPanel, L"대기실", pad, 40.f, 40.f, 320.f, 54.f, textLight);
	makeSolid(roomPanel, "playerCountBadgeBg", roomPanelW - pad - 92.f, 30.f, 92.f, 44.f, scrimBar);
	playerCountLabel_ = makeLabel(roomPanel, L"1 / 4", roomPanelW - pad - 92.f, 30.f, 22.f, 92.f, 44.f,
		textLight, UI::TextHAlign::Center);

	// 툴바: [방 코드 + 복사] [호스트 액션] [방 나가기]
	const float toolbarY = 102.f;
	const float toolbarH = 72.f;
	const float leaveW   = 150.f;
	const float hostW    = 220.f;
	const float codeW    = innerW - hostW - leaveW - 2.f * gap;
	const float hostX    = pad + codeW + gap;
	const float leaveX   = hostX + hostW + gap;

	// 방 코드 영역
	makeSolid(roomPanel, "roomCodeArea", pad, toolbarY, codeW, toolbarH, scrimBar);
	makeLabel(roomPanel, L"방 코드", pad + 18.f, toolbarY + 10.f, 13.f, 160.f, 20.f, textMuted);
	roomCodeLabel_ = makeLabel(roomPanel, L"------", pad + 18.f, toolbarY + 28.f, 34.f,
		codeW - 150.f, 40.f, textLight);
	fadeBtn(stylePrimary(makeButton(roomPanel, L"코드 복사",
		pad + codeW - 124.f, toolbarY + 12.f, 112.f, toolbarH - 24.f,
		primary, primaryDark, primaryDark, 16.f,
		[this]() { gSharedLog << "[Lobby] 방 코드: " << roomCode_ << "\n"; }, textLight)));

	// 호스트 액션: 게임 시작 버튼 / 대기 메시지(겹쳐 두고 refreshLobbyUI에서 토글)
	startGameButton_ = makeButton(roomPanel, L"게임 시작", hostX, toolbarY, hostW, toolbarH,
		primary, primaryDark, primaryDark, 22.f, [this]() { lobbyStartGame(); }, textLight);
	fadeBtn(stylePrimary(startGameButton_));
	startGameLabel_  = static_cast<UI::Label*>(startGameButton_->children().front().get());

	waitMessageBg_ = makeSolid(roomPanel, "waitMessageBg", hostX, toolbarY, hostW, toolbarH, scrimBar);
	hostStatusLabel_ = makeLabel(roomPanel, L"호스트 대기 중...", hostX, toolbarY, 17.f,
		hostW, toolbarH, textMuted, UI::TextHAlign::Center);

	// 방 나가기
	fadeBtn(styleSecondary(makeButton(roomPanel, L"방 나가기", leaveX, toolbarY, leaveW, toolbarH,
		surfaceSoft, primarySoft, primarySoft, 18.f, [this]() { lobbyLeaveRoom(); }, buttonInk)));

	// 스쿼드 스테이지: 가로 4칸 슬롯
	const float slotsY    = toolbarY + toolbarH + 16.f;
	const float debugH    = 40.f;
	const float slotsH    = roomPanelH - slotsY - debugH - 14.f;
	const float slotW     = (innerW - 3.f * gap) / 4.f;
	const float nameH     = 56.f;
	// 슬롯 배경: 캐릭터 뒤 패널(불투명 scrim)과 이름표 배경. 캐릭터 뒤 3D맵이 비치지 않도록 복원.
	const XMFLOAT4 slotPanelCol    = { 0.09f, 0.13f, 0.20f, 0.16f };  // 캐릭터 배경(거의 불투명)
	const XMFLOAT4 slotNameplateCol= { 0.05f, 0.08f, 0.13f, 0.78f };  // 이름표 배경

	for (int i = 0; i < kMaxLobbyPlayers; ++i) {
		const float x = pad + static_cast<float>(i) * (slotW + gap);

		// 슬롯의 반투명 배경(패널/이름표 scrim)은 제거 — 3D 캐릭터 위에 번호/이름만 표시한다.
		slotNumberLabels_[i] = makeLabel(roomPanel, L"", x + 12.f, slotsY + 10.f, 13.f, 60.f, 20.f,
			textMuted, UI::TextHAlign::Leading, 2);

		// 모델 베이: 오프스크린 포트레이트 RT(캐릭터)를 셀 sub-rect로 합성하는 이미지.
		// 텍스처/uvScaleBias/visible는 매 프레임 renderWaitingRoom()에서 갱신한다.
		// 캐릭터 뒤 배경 패널(불투명 scrim, zOrder 0): 캐릭터 뒤로 3D맵이 비치지 않게 한다.
		slotPanels_[i] = makeSolid(roomPanel, "slotPanelBg",
			x + 8.f, slotsY + 8.f, slotW - 16.f, slotsH - nameH - 22.f, slotPanelCol, 0);

		auto* bay = roomPanel->addChild(std::make_unique<UI::Image>());
		bay->name = "slotBay";
		applyRect(bay, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
			x + 8.f, slotsY + 8.f, slotW - 16.f, slotsH - nameH - 22.f);
		bay->zOrder  = 1;       // 배경 패널 위에 캐릭터를 그린다.
		bay->visible = false;   // 빈 슬롯은 숨김(채워진 슬롯만 표시 — 잔상 방지)
		slotBays_[i] = static_cast<UI::Image*>(bay);

		// 이름표: 배경(scrim, zOrder 1) + 이름/호스트 뱃지 텍스트(zOrder 3). 배경은 이름+뱃지를 덮는다.
		const float nameY = slotsY + slotsH - nameH - 4.f;
		slotNameplateBgs_[i] = makeSolid(roomPanel, "nameplateBg",
			x + 10.f, nameY, slotW - 20.f, 52.f, slotNameplateCol, 1);
		slotNameLabels_[i] = makeLabel(roomPanel, L"대기 중",
			x + 10.f, nameY, 18.f, slotW - 20.f, 28.f,
			textLight, UI::TextHAlign::Center, 3);
		slotHostBadgeLabels_[i] = makeLabel(roomPanel, L"",
			x + 10.f, slotsY + slotsH - 30.f, 12.f, slotW - 20.f, 22.f,
			textMuted, UI::TextHAlign::Center, 3);

		// 호스트 이름 사각 테두리(4변, zOrder 2 — 이름표 배경 위). 기본 숨김, refreshLobbyUI에서 호스트만 표시.
		const XMFLOAT4 borderCol = { 0.95f, 0.80f, 0.35f, 1.f };  // 골드(호스트 표식)
		const float bpad = 4.f, bt = 2.f;
		const float bx = x + 10.f - bpad, by = nameY - bpad;
		const float bw = (slotW - 20.f) + 2.f * bpad, bh = 28.f + 2.f * bpad;
		slotNameBorders_[i][0] = makeSolid(roomPanel, "nameBorderT", bx, by, bw, bt, borderCol, 2);
		slotNameBorders_[i][1] = makeSolid(roomPanel, "nameBorderB", bx, by + bh - bt, bw, bt, borderCol, 2);
		slotNameBorders_[i][2] = makeSolid(roomPanel, "nameBorderL", bx, by, bt, bh, borderCol, 2);
		slotNameBorders_[i][3] = makeSolid(roomPanel, "nameBorderR", bx + bw - bt, by, bt, bh, borderCol, 2);
		for (auto* edge : slotNameBorders_[i]) {
			if (edge) edge->visible = false;
		}
	}

	// // 디버그 툴
	// const float debugY = roomPanelH - debugH + 2.f;
	// makeLabel(roomPanel, L"MOCK", pad, debugY, 12.f, 60.f, debugH, textMuted);
	// fadeBtn(styleSecondary(makeButton(roomPanel, L"더미 추가",
	// 	roomPanelW - pad - 218.f, debugY, 104.f, debugH - 4.f,
	// 	surface, primarySoft, primarySoft, 13.f, [this]() { lobbyAddDummy(); }, buttonInk)));
	// fadeBtn(styleSecondary(makeButton(roomPanel, L"더미 제거",
	// 	roomPanelW - pad - 104.f, debugY, 104.f, debugH - 4.f,
	// 	surface, primarySoft, primarySoft, 13.f, [this]() { lobbyRemoveDummy(); }, buttonInk)));

	// Full-screen loading overlay (built last so it sits on top by zOrder).
	buildLoadingScreen();
}

// Black full-screen loading overlay: centered logo, "loading..." text, a progress bar,
// and a 12-dot ring spinner. Hidden by default; toggled/updated from LobbyScene().
void Game::buildLoadingScreen() {
	if (!lobbyRoot_) return;

	const float screenW = uiManager_.screenWidth();
	const float screenH = uiManager_.screenHeight();
	constexpr int kZ = 100;  // above all lobby UI

	loadingRoot_ = lobbyRoot_->addChild(std::make_unique<UI::UIElement>());
	loadingRoot_->name    = "loadingRoot";
	loadingRoot_->anchor  = UI::Anchors::TopLeft;
	loadingRoot_->pivot   = UI::Pivots::TopLeft;
	loadingRoot_->offsetX = UI::DimValue::px(0.f);
	loadingRoot_->offsetY = UI::DimValue::px(0.f);
	loadingRoot_->width   = UI::DimValue::px(screenW);
	loadingRoot_->height  = UI::DimValue::px(screenH);
	loadingRoot_->zOrder  = kZ;
	loadingRoot_->visible = false;

	// Opaque black background covering the whole screen.
	auto* bg = static_cast<UI::Button*>(loadingRoot_->addChild(std::make_unique<UI::Button>()));
	bg->name        = "loadingBg";
	bg->anchor      = UI::Anchors::TopLeft;
	bg->pivot       = UI::Pivots::TopLeft;
	bg->offsetX     = UI::DimValue::px(0.f);
	bg->offsetY     = UI::DimValue::px(0.f);
	bg->width       = UI::DimValue::px(screenW);
	bg->height      = UI::DimValue::px(screenH);
	bg->interactive = true;  // absorb clicks so hidden lobby controls cannot fire through the overlay
	bg->bgColor     = { 0.f, 0.f, 0.f, 1.f };
	bg->zOrder      = kZ;

	// Centered logo (above middle). Skipped if the logo texture failed to load.
	if (lobbyLogoTex_.res) {
		auto* logo = static_cast<UI::Image*>(loadingRoot_->addChild(std::make_unique<UI::Image>()));
		logo->name    = "loadingLogo";
		logo->anchor  = UI::Anchors::Center;
		logo->pivot   = UI::Pivots::Center;
		logo->offsetX = UI::DimValue::px(0.f);
		logo->offsetY = UI::DimValue::px(-120.f);
		logo->width   = UI::DimValue::px(420.f);
		logo->height  = UI::DimValue::px(160.f);
		logo->texture = &lobbyLogoTex_;
		logo->zOrder  = kZ + 1;
	}

	// "loading..." label, just above the progress bar.
	loadingTextLabel_ = static_cast<UI::Label*>(loadingRoot_->addChild(std::make_unique<UI::Label>()));
	loadingTextLabel_->name    = "loadingText";
	loadingTextLabel_->anchor  = UI::Anchors::Center;
	loadingTextLabel_->pivot   = UI::Pivots::Center;
	loadingTextLabel_->offsetX = UI::DimValue::px(0.f);
	loadingTextLabel_->offsetY = UI::DimValue::px(64.f);
	loadingTextLabel_->width   = UI::DimValue::px(420.f);
	loadingTextLabel_->height  = UI::DimValue::px(28.f);
	loadingTextLabel_->setTextHAlign(UI::TextHAlign::Center);
	loadingTextLabel_->setTextVAlign(UI::TextVAlign::Center);
	loadingTextLabel_->setFontSize(18.f);
	loadingTextLabel_->setTextColor(0.92f, 0.95f, 1.f, 1.f);
	loadingTextLabel_->setText(L"loading...");
	loadingTextLabel_->zOrder = kZ + 1;

	// Progress bar (real load progress).
	loadingBar_ = static_cast<UI::ProgressBar*>(loadingRoot_->addChild(std::make_unique<UI::ProgressBar>()));
	loadingBar_->name      = "loadingBar";
	loadingBar_->anchor    = UI::Anchors::Center;
	loadingBar_->pivot     = UI::Pivots::Center;
	loadingBar_->offsetX   = UI::DimValue::px(0.f);
	loadingBar_->offsetY   = UI::DimValue::px(100.f);
	loadingBar_->width     = UI::DimValue::px(420.f);
	loadingBar_->height    = UI::DimValue::px(14.f);
	loadingBar_->bgColor   = { 0.16f, 0.18f, 0.22f, 1.f };
	loadingBar_->fillColor = { 0.18f, 0.62f, 0.95f, 1.f };
	loadingBar_->zOrder    = kZ + 1;
	loadingBar_->setProgress(0.f);

	// Dot-ring spinner below the bar. The rotating "arc" is faked by phase-shifting
	// each dot's alpha (no rotation field / arc texture needed).
	constexpr float kRadius = 26.f;
	const float spinnerCY = 176.f;
	for (int i = 0; i < static_cast<int>(spinnerDots_.size()); ++i) {
		const float ang = (static_cast<float>(i) / static_cast<float>(spinnerDots_.size())) * 6.2831853f;
		auto* dot = static_cast<UI::Button*>(loadingRoot_->addChild(std::make_unique<UI::Button>()));
		dot->name        = "spinnerDot";
		dot->anchor      = UI::Anchors::Center;
		dot->pivot       = UI::Pivots::Center;
		dot->offsetX     = UI::DimValue::px(std::sin(ang) * kRadius);
		dot->offsetY     = UI::DimValue::px(spinnerCY - std::cos(ang) * kRadius);
		dot->width       = UI::DimValue::px(7.f);
		dot->height      = UI::DimValue::px(7.f);
		dot->interactive = false;
		dot->bgColor     = { 0.85f, 0.90f, 1.f, 0.25f };
		dot->zOrder      = kZ + 1;
		spinnerDots_[i] = dot;
	}
}

// Toggle the loading overlay and, while visible, push real progress to the bar and
// animate the spinner. Called every frame from LobbyScene().
void Game::updateLoadingScreen(float deltaTimeSec, bool visible) {
	if (!loadingRoot_) return;
	loadingRoot_->visible = visible;
	if (!visible) return;

	if (loadingBar_) loadingBar_->setProgress(loadProgress01());

	// Rotate the bright "head" around the dot ring; trailing dots fade out.
	loadingSpinTime_ += deltaTimeSec;
	const int n = static_cast<int>(spinnerDots_.size());
	const float headF = std::fmod(loadingSpinTime_ * 1.6f, 1.f) * static_cast<float>(n);
	for (int i = 0; i < n; ++i) {
		if (!spinnerDots_[i]) continue;
		float dist = static_cast<float>(i) - headF;
		while (dist < 0.f) dist += static_cast<float>(n);   // forward angular distance from head
		const float t = dist / static_cast<float>(n);        // 0 at head .. ~1 just behind it
		const float alpha = 0.15f + 0.85f * (1.f - t);
		auto c = spinnerDots_[i]->bgColor;
		c.w = alpha;
		spinnerDots_[i]->bgColor = c;
	}
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
	if (!lobbyRoot_) {
		return;
	}

	const XMFLOAT4 primary     = { 0.082f, 0.365f, 0.545f, 1.f };
	const XMFLOAT4 primaryDark = { 0.055f, 0.286f, 0.435f, 1.f };
	// 대기실(반투명 스쿼드 스테이지) 색상
	const XMFLOAT4 textLight   = { 0.95f, 0.97f, 1.00f, 1.f };
	const XMFLOAT4 textMuted   = { 0.78f, 0.84f, 0.92f, 1.f };

	lobbyRoot_->visible = (scene_ == Scene::Lobby);

	const bool inMain = (lobbyState_ == LobbyState::MainMenu);
	const bool waitingRoom3D = !inMain && stageVisualReady_.load(std::memory_order_acquire);
	if (lobbySkyBg_)   lobbySkyBg_->visible   = !waitingRoom3D;
	if (lobbyBgImage_) lobbyBgImage_->visible = !waitingRoom3D;

	if (mainMenuRoot_)    mainMenuRoot_->visible    = inMain;
	if (waitingRoomRoot_) waitingRoomRoot_->visible = !inMain;

	if (inMain) {
		return;
	}

	// 방 코드
	if (roomCodeLabel_) {
		const std::wstring code(roomCode_.begin(), roomCode_.end());
		roomCodeLabel_->setText(code.empty() ? std::wstring(L"------") : code);
	}

	// 인원수
	if (playerCountLabel_) {
		playerCountLabel_->setText(
			std::to_wstring(lobbyPlayers_.size()) + L" / " + std::to_wstring(kMaxLobbyPlayers));
	}

	// 플레이어 슬롯 (스쿼드 스테이지)
	for (int i = 0; i < kMaxLobbyPlayers; ++i) {
		if (slotNumberLabels_[i]) {
			wchar_t num[4] = {};
			swprintf_s(num, L"%02d", i + 1);
			slotNumberLabels_[i]->setText(num);
		}

		const bool filled = (i < static_cast<int>(lobbyPlayers_.size()));
		if (filled) {
			const auto& player = lobbyPlayers_[i];
			const bool isSlotHost = (player.sessionId == hostId_);

			if (slotNameLabels_[i]) {
				slotNameLabels_[i]->setText(player.name);
				slotNameLabels_[i]->setTextColor(textLight.x, textLight.y, textLight.z, textLight.w);
			}
			if (slotHostBadgeLabels_[i]) {
				slotHostBadgeLabels_[i]->setText(isSlotHost ? L"\U0001F451 호스트" : L"");
			}
			for (auto* edge : slotNameBorders_[i]) {
				if (edge) edge->visible = isSlotHost;   // 호스트 이름에만 사각 테두리 표시
			}
		} else {
			if (slotNameLabels_[i]) {
				slotNameLabels_[i]->setText(L"대기 중");
				slotNameLabels_[i]->setTextColor(textMuted.x, textMuted.y, textMuted.z, textMuted.w);
			}
			if (slotHostBadgeLabels_[i]) slotHostBadgeLabels_[i]->setText(L"");
			for (auto* edge : slotNameBorders_[i]) {
				if (edge) edge->visible = false;
			}
		}
	}

	// 게임 시작 버튼(호스트) / 대기 메시지(비호스트) — 같은 자리에 겹쳐 두고 토글.
	// Start is allowed even while loading (req 2): keep the button active and labeled.
	if (startGameButton_) startGameButton_->visible = isHost_;
	if (waitMessageBg_)   waitMessageBg_->visible   = !isHost_;
	if (hostStatusLabel_) hostStatusLabel_->visible = !isHost_;
	if (isHost_ && startGameLabel_) {
		startGameLabel_->setText(L"게임 시작");
	}
	if (isHost_ && startGameButton_) {
		// 단색 폴백 경로(texNormal 미설정 시).
		startGameButton_->bgColor        = primary;
		startGameButton_->bgColorHovered = primaryDark;
		startGameButton_->bgColorPressed = primaryDark;

		// 텍스처 경로(stylePrimary로 texNormal 설정됨)는 bgColor*를 무시하고 texTint*만 본다.
		startGameButton_->texTint        = XMFLOAT4{ 1.f,   1.f,   1.f,   0.92f };
		startGameButton_->texTintHovered = XMFLOAT4{ 1.08f, 1.08f, 1.08f, 0.96f };
		startGameButton_->texTintPressed = XMFLOAT4{ 0.82f, 0.82f, 0.82f, 0.96f };
	}
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
		INet::ClientApp::addSendBuffer(PacketManager::makeCEnterPacket(handoffCode_));
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
		if (lobbySkyBg_)   lobbySkyBg_->visible = false;
		if (lobbyBgImage_) lobbyBgImage_->visible = false;

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
	updateLoadingScreen(std::chrono::duration<float>(deltaTime).count(), showLoading);

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
		chunkManager_.submitDrawEvents(gfx_);
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
	gfx_.addLobbyPortraitFrameData(PBRSkinnedPipeline::FrameData{ .globalAmbient = mu::Vec3(0.20f, 0.20f, 0.22f) });
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
		if (slotBays_[i]) {
			slotBays_[i]->visible = isFilled && (portraitTex != nullptr);
			if (slotBays_[i]->visible) {
				slotBays_[i]->texture     = portraitTex;
				slotBays_[i]->uvScaleBias = gfx_.lobbyPortraitCellUvScaleBias(static_cast<u32t>(i));
			}
		}
	}

	uiManager_.render(gfx_);
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
	pendingStart_ = false;
	waitingRoomWarmupFrames_ = 0;  // 대기실 재입장 시 다시 워밍업하도록 리셋
	if (loadingRoot_) loadingRoot_->visible = false;
	if (lobbyRoot_) {
		lobbyRoot_->visible = false;
	}

	// 인게임은 Deferred 경로로 렌더한다(로비에서 Forward로 바꿔둔 것을 복원).
	gfx_.setRenderPath(GFX::RenderPath::Deferred);

	// 대기실 전시 캐릭터 정리: animSystem 트랙/shared_ptr를 제거해 인게임 씬으로 누수되지 않게 한다.
	clearLobbyCharacters();

	setupStage();
	scene_ = Scene::InGame;

	// 플레이어/오브젝트 생성은 서버의 S_Enter 패킷이 담당한다.
	// (패킷은 InGameScene의 SleepEx(alertable)에서 처리되므로, 씬 전환 후 첫 프레임부터 수신된다.)
	// 여기서 별도로 플레이어를 만들면 S_Enter가 다시 setupPlayer를 호출해
	// 이전 플레이어의 물리 바디가 물리월드에 dangling으로 남아 크래시한다.
}

std::wstring Game::lobbyDisplayName(uint16 sessionId) const {
	if (sessionId == myId_) {
		return L"나";
	}
	return L"Player_" + std::to_wstring(sessionId);
}

void Game::lobbyCreateRoom() {
	// 방 생성 요청만 보낸다. 대기실 전환은 S_CreateRoom 수신(onLobbyCreated) 시 수행.
	INet::ClientApp::addSendBuffer(PacketManager::makeCCreateRoomPacket());
	INet::ClientApp::send();
	gSharedLog << "[Lobby] 방 생성 요청 전송\n";
}

void Game::lobbyJoinRoom(const std::string& code) {
	// 코드 정규화: 대문자 + 영숫자만 (TextInput에서 이미 필터되지만 방어적으로 한 번 더).
	std::string norm;
	for (char c : code) {
		if (c >= 'a' && c <= 'z') c = static_cast<char>(c - ('a' - 'A'));
		const bool isAlnum = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
		if (isAlnum) norm += c;
	}

	if (norm.size() != 6) {
		if (mainMenuMsgLabel_) mainMenuMsgLabel_->setText(L"6자리 코드를 입력하세요");
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
	myId_   = 0;
	lobbyPlayers_.clear();
	lobbyState_ = LobbyState::MainMenu;
	lobbyCameraTime_ = 0.f;
	pendingStart_ = false;
	waitingRoomWarmupFrames_ = 0;  // 대기실 재입장 시 다시 워밍업하도록 리셋
	if (loadingRoot_) loadingRoot_->visible = false;
	if (roomCodeInput_)    roomCodeInput_->clear();
	if (mainMenuMsgLabel_) mainMenuMsgLabel_->setText(L"");

	// 대기실 3D 배경 → 메인화면 복귀: Forward(UI-only) 경로 복원 + 키아트 bg 다시 표시.
	gfx_.setRenderPath(GFX::RenderPath::Forward);
	if (lobbyBgImage_) lobbyBgImage_->visible = true;

	// 전시 캐릭터 정리(animSystem 트랙 해제 포함) — 재입장 시 다시 생성된다.
	clearLobbyCharacters();

	refreshLobbyUI();
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

// --- LobbyServer 응답 핸들러 (메인 스레드 alertable 대기에서 호출) ---

void Game::onLobbyCreated(const std::string& code, uint16 myId) {
	roomCode_ = code;
	myId_     = myId;
	hostId_   = myId;       // 생성자가 곧 호스트
	isHost_   = true;
	lobbyPlayers_.clear();
	lobbyPlayers_.push_back({ myId, lobbyDisplayName(myId) });
	lobbyState_ = LobbyState::WaitingRoom;
	refreshLobbyUI();
	gSharedLog << "[Lobby] 방 생성됨: " << code << " (myId=" << myId << ")\n";
}

void Game::onLobbyJoined(bool success, uint16 hostId, uint16 myId, const std::string& code, const std::vector<uint16>& playerIds) {
	if (!success) {
		if (mainMenuMsgLabel_) mainMenuMsgLabel_->setText(L"방을 찾을 수 없습니다");
		gSharedLog << "[Lobby] 방 참가 실패\n";
		return;
	}

	myId_     = myId;
	hostId_   = hostId;
	roomCode_ = code;
	isHost_   = (myId == hostId);

	lobbyPlayers_.clear();
	for (uint16 id : playerIds) {
		lobbyPlayers_.push_back({ id, lobbyDisplayName(id) });
	}

	lobbyState_ = LobbyState::WaitingRoom;
	refreshLobbyUI();
	gSharedLog << "[Lobby] 방 참가 성공: " << code << " (myId=" << myId << ", host=" << hostId << ")\n";
}

void Game::onLobbyPlayerJoined(uint16 sessionId) {
	const bool exists = std::any_of(lobbyPlayers_.begin(), lobbyPlayers_.end(),
		[sessionId](const LobbyPlayer& p) { return p.sessionId == sessionId; });
	if (!exists) {
		lobbyPlayers_.push_back({ sessionId, lobbyDisplayName(sessionId) });
	}
	refreshLobbyUI();
	gSharedLog << "[Lobby] 플레이어 입장: " << sessionId << "\n";
}

void Game::onLobbyPlayerLeft(uint16 sessionId) {
	std::erase_if(lobbyPlayers_, [sessionId](const LobbyPlayer& p) { return p.sessionId == sessionId; });

	// 호스트가 떠났으면 서버 규칙과 동일하게 남은 목록의 front를 새 호스트로 본다.
	if (sessionId == hostId_ && !lobbyPlayers_.empty()) {
		hostId_ = lobbyPlayers_.front().sessionId;
		isHost_ = (myId_ == hostId_);
	}

	refreshLobbyUI();
	gSharedLog << "[Lobby] 플레이어 퇴장: " << sessionId << "\n";
}

void Game::onGameStart(const std::string& roomServerIp, uint16 roomServerPort, const std::string& lobbyCode) {
	// 이 함수는 로비 recv APC 안에서 실행되므로 여기서 소켓을 건드리지 않고, 핸드오프 요청만 적재한다.
	// 실제 재접속/씬 전환은 LobbyScene이 APC 밖(안전 지점)에서 에셋 로드 완료 후 수행한다.
	handoffIp_     = roomServerIp;
	handoffPort_   = roomServerPort;
	handoffCode_   = lobbyCode;
	pendingHandoff_ = true;
	gSharedLog << "[Lobby] 게임 시작 신호 수신: RoomServer " << roomServerIp << ":" << roomServerPort
		<< " (code=" << lobbyCode << ")\n";
}

// 윈도우 프로시저에서 특정한 메시지 처리를 위임받는다.
LRESULT Game::receiveWndMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
		DISPLAY_ERROR_GLE(rawInputResult != -1, true);

		if (rawInputSize > sRawInputBuffer.size()) {
			sRawInputBuffer.resize(rawInputSize);
		}

		// 입력 구조체 내용 수신
		rawInputResult = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT, sRawInputBuffer.data(), &rawInputSize, sizeof(RAWINPUTHEADER)
		);
		DISPLAY_ERROR_GLE(rawInputResult == rawInputSize, true);

		auto ri = reinterpret_cast<const RAWINPUT*>(sRawInputBuffer.data());
		if (ri->header.dwType == RIM_TYPEMOUSE) {
			// 마우스에 대한 입력 내용이 상대 좌표여야 한다.
			DISPLAY_ERROR_STR(!(ri->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE),
				"[Input Error] Game::receiveWndMsg: 마우스 입력 장치가 게임과 호환되지 않습니다.\n"
				"RAWMOUSE의 플래그 중 MOUSE_MOVE_ABSOLUTE가 활성화되어있습니다.",
				true
			);

			// 마우스 이동량 기록
			mouseDeltaX_ += ri->data.mouse.lLastX;
			mouseDeltaY_ += ri->data.mouse.lLastY;
		}
		return 0;
	}

	// Alt+Tab 등으로 윈도우가 포커스를 잃었다가 되찾은 경우,
	// 커서와 관련된 플래그들을 읽어 커서 캡처, 커서 숨기기 등을 다시 수행한다.
	case WM_SETFOCUS:
		if (cursorCaptureEnabled_) {
			captureCursor();
		}
		if (!cursorShowEnabled_) {
			hideCursor();
		}
		break;

	// Alt+Tab 등으로 윈도우가 포커스를 잃은 경우
	// 커서와 관련된 플래그들을 읽어 커서 캡처 해제, 커서 보이기 등을 수행한다.
	// 다른 윈도우로 전환되었는데 커서가 보이지 않거나 안 움직여지면 곤란할 것이다.
	case WM_KILLFOCUS:
		if (cursorCaptureEnabled_) {
			releaseCursor();
		}
		if (cursorShowEnabled_) {
			showCursor();
		}
		break;

	case WM_SIZE:
		return DefWindowProcA(hWnd, msg, wParam, lParam);

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

void Game::sendMouseMovePacket() {
	const auto forward = player_->forward();
	const auto yawRad = std::atan2(forward.x(), forward.z());

	auto sendBuffer = PacketManager::makeCMouseMovePacket(yawRad);
	INet::ClientApp::addSendBuffer(sendBuffer);
}

void Game::sendAttackPacket() {
	uint64 clientMs = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count());
	INet::ClientApp::addSendBuffer(PacketManager::makeCAttackPacket(clientMs));
}

void Game::sendSkillStartPacket(uint32 skillAssetId) {
	uint64 clientMs = static_cast<uint64>(
		static_cast<int64>(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count()));
	INet::ClientApp::addSendBuffer(PacketManager::makeCSkillStartPacket(skillAssetId, clientMs));
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

	processInputGame(deltaTime);

	// 로비/게임 공통 입력 처리
	// Enter 키를 누르면 커서 캡처 플래그를 활성화/비활성화한다.
	if ( (keyboardStateCurr_[VK_RETURN] & 0x80) && !(keyboardStatePrev_[VK_RETURN] & 0x80) ) {
		cursorCaptureEnabled_ = !cursorCaptureEnabled_;
		if (cursorCaptureEnabled_) {
			captureCursor();
		}
		else {
			releaseCursor();
		}
	}

	// Space 키를 누르면 커서 보이기 플래그를 활성화/비활성화한다.
	if ( (keyboardStateCurr_[VK_SPACE] & 0x80) && !(keyboardStatePrev_[VK_SPACE] & 0x80) ) {
		cursorShowEnabled_ = !cursorShowEnabled_;
		if (cursorShowEnabled_) {
			showCursor();
		}
		else {
			hideCursor();
		}
	}
}

void Game::processInputGame(Milliseconds deltaTime) {
	const auto prevForward = player_->forward();

	// 이동 가속도만 담당. 감속은 PhysicsWorld의 linearDamping(마찰)이 처리한다.
	// 속도 상한은 kPlayerMaxSpeed, 가속률은 kPlayerAccelRate (파일 상단 상수 참조).
	const auto moveXSign = !playerDead_ * ( (keyboardStateCurr_['D'] & 0x80) - (keyboardStateCurr_['A'] & 0x80) );
	const auto moveZSign = !playerDead_ * ( (keyboardStateCurr_['W'] & 0x80) - (keyboardStateCurr_['S'] & 0x80) );
	const bool rightMouseDragging = !uiManager_.needsCursor() && (keyboardStateCurr_[VK_RBUTTON] & 0x80);

	if (moveXSign || moveZSign) {
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
		const auto fullVel = player_->velocity();
		const auto accel   = mu::Vec3(moveDirection) * (kPlayerAccelRate * Seconds(deltaTime).count());
		float newX = fullVel.x() + accel.x();
		float newZ = fullVel.z() + accel.z();

		// x/z 속도만 클램프 (y는 건드리지 않음).
		const float hSpd2 = newX * newX + newZ * newZ;
		if (hSpd2 > kPlayerMaxSpeed * kPlayerMaxSpeed) {
			const float scale = kPlayerMaxSpeed / std::sqrt(hSpd2);
			newX *= scale;
			newZ *= scale;
		}
		player_->setVelocity(mu::Vec3(newX, fullVel.y(), newZ));
	}

	currVelocity_ = player_->velocity();

	// C key: toggle CSM cascade debug visualization
	if ( (keyboardStateCurr_['C'] & 0x80) && !(keyboardStatePrev_['C'] & 0x80) ) {
		gfx_.toggleCsmDebugVisualization();
	}

	// H key: toggle Hi-Z occlusion culling
	if ( (keyboardStateCurr_['H'] & 0x80) && !(keyboardStatePrev_['H'] & 0x80) ) {
		gfx_.setHiZCullEnabled(!gfx_.isHiZCullEnabled());
	}
	gfx_.setHiZCullEnabled(false);

	// G key: cycle GBuffer debug view (deferred path only)
	if ( (keyboardStateCurr_['G'] & 0x80) && !(keyboardStatePrev_['G'] & 0x80) ) {
		gfx_.cycleGBufferDebugMode();
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

	if (prevForward != currForward) {
		sendMouseMovePacket();
	}

	mouseDeltaX_ = 0;
	mouseDeltaY_ = 0;

	// Q key: start skill
	if (!playerDead_
		&& !uiManager_.needsCursor()
		&& (keyboardStateCurr_['Q'] & 0x80)
		&& !(keyboardStatePrev_['Q'] & 0x80))
	{
		const SkillAsset* asset = skillSystem_.findAsset("SwordSlash");
		if (asset && !skillSystem_.hasActiveSkill(player_->getId())) {
			skillSystem_.startSkill(asset->id, player_->getId(), skillCtx_);
			sendSkillStartPacket(asset->id);
		}
	}

	// 플레이어 공격: LButton 클릭 시 서버에 C_Attack 전송 + 로컬 이펙트 재생
	if (!playerDead_
		&& !uiManager_.needsCursor()	// ui가 커서를 필요로 하는 상태가 아니어야 한다. (UI 상호작용 중에 공격이 나가는 것을 방지)
		&& (keyboardStateCurr_[VK_LBUTTON] & 0x80)
		&& !(keyboardStatePrev_[VK_LBUTTON] & 0x80))
	{
		sendAttackPacket();
		holdEvent( eventList_, EvAttack( player_->getId() ) );

		const auto slashPos = player_->renderState().pos
		                    + player_->forward() * 1.f
		                    + mu::Vec3(0.f, 1.0f, 0.f);
		switch (currentEffect_) {
		case SwordEffect::SlashCombo: swordSlashComboEffect_.play(slashPos);                    break;
		case SwordEffect::Slash7:     swordSlash7Effect_.play(slashPos);                        break;
		case SwordEffect::Slash1:     swordSlash1Effect_.play(slashPos);                        break;
		case SwordEffect::SlashWave:  slashWaveEffect_.play(slashPos, player_->orient());       break;
		case SwordEffect::Spikes:     spikesAttackEffect_.play(slashPos);                       break;
		case SwordEffect::Piercing:            piercingEffect_.play( slashPos, player_->orient() );              break;
		case SwordEffect::PiercingSlash:       piercingSlashEffect_.play( slashPos, player_->orient() );         break;
		case SwordEffect::PiercingCircleSlash: piercingCircleSlashEffect_.play( slashPos, player_->orient() );   break;
		case SwordEffect::PiercingMulti: {
			const auto multiCenter = player_->renderState().pos
				+ player_->forward() * (kPiercingMultiRadius + 1.f)
				+ mu::Vec3( 0.f, 1.0f, 0.f );
			piercingMultiEffect_.play( multiCenter, player_->orient() );
			break;
		}
		case SwordEffect::CrystalsFrontAttack: {
			const auto crystalPos = player_->renderState().pos + player_->forward() * 1.f;
			const mu::Mat4x4 crystalOrient = mu::rotateYH( mu::Degree( -90.f ) ) * player_->orient().mat4();
			crystalsFrontAttackEffect_.play( crystalPos, crystalOrient, player_->forward() );
			break;
		}
		case SwordEffect::AoESlashGreen: {
			const auto aoePos = slashPos + player_->forward() * 5.5f;
			aoESlashGreenEffect_.play( aoePos, player_->orient(), player_->forward() );
			break;
		}
		case SwordEffect::Arrow: {
			const auto arrowOrigin = player_->renderState().pos
				+ mu::Vec3{ 0.f, 1.2f, 0.f }
				+ player_->forward() * 0.5f;
			arrowEffect_.play( arrowOrigin, player_->orient() );
			break;
		}
		case SwordEffect::ArrowVolley: {
			const auto volleyOrigin = player_->renderState().pos
				+ mu::Vec3{ 0.f, 1.2f, 0.f }
				+ player_->forward() * 0.6f;
			arrowVolleyMuzzleEffect_.play( volleyOrigin, player_->orient() );
			arrowVolleyEffect_.play( volleyOrigin, player_->orient() );
			break;
		}
		case SwordEffect::ArrowRain: {
			const auto muzzlePos = player_->renderState().pos + mu::Vec3{ 0.f, 1.2f, 0.f };
			auto rainCenter = player_->renderState().pos + player_->forward() * 6.5f;
			if ( !chunkManager_.empty() ) {
				const float groundY = chunkManager_.heightAtWorld( rainCenter.x(), rainCenter.z() );
				rainCenter = { rainCenter.x(), groundY, rainCenter.z() };
			}
			arrowRainMuzzleEffect_.play( muzzlePos, player_->orient() );
			arrowRainEffect_.play( rainCenter, player_->orient() );
			debugBVView_.pushCircle( rainCenter, kArrowRainRadius, 1500ms, { 1.f, 0.f, 0.f, 1.f } );
			break;
		}
		case SwordEffect::RedEnergyExplosion: {
			auto explosionPos = player_->renderState().pos + player_->forward() * 6.5f;
			if ( !chunkManager_.empty() ) {
				const float groundY = chunkManager_.heightAtWorld( explosionPos.x(), explosionPos.z() );
				explosionPos = { explosionPos.x(), groundY + 0.1f, explosionPos.z() };
			}
			else {
				explosionPos += mu::Vec3( 0.f, 0.1f, 0.f );
			}
			redEnergyExplosionEffect_.play( explosionPos, player_->orient(), player_->forward() );
			break;
		}
		case SwordEffect::CrystalsCrossFade: {
			crystalsCrossFadeEffect_.play( slashPos );
			break;
		}
		case SwordEffect::EnergyExplosionArrow: {
			const auto origin = player_->renderState().pos
				+ mu::Vec3{ 0.f, 1.2f, 0.f }
				+ player_->forward() * 0.5f;
			energyExplosionArrowEffect_.play( origin, player_->orient() );
			break;
		}
		case SwordEffect::TornadoShot: {
			if ( tornadoShotActive_ )
				tornadoShotEffect_.stop();
			const auto origin = player_->renderState().pos
				+ mu::Vec3{ 0.f, 0.8f, 0.f }
				+ player_->forward() * 0.5f;
			tornadoMuzzleEffect_.play( origin );
			tornadoShotEffect_.play( origin, player_->orient() );
			tornadoShotActive_  = true;
			tornadoShotPos_     = origin;
			tornadoShotDir_     = player_->forward();
			tornadoShotOrient_  = player_->orient();
			tornadoShotElapsed_ = 0s;
			break;
		}
		}
	}
}

void Game::cullObjects() {
	auto entities = std::vector< std::shared_ptr<Object> >();
	entities.reserve(otherPlayers_.size() + goblins_.size());
	std::ranges::copy(otherPlayers_, std::back_inserter(entities));
	std::ranges::copy(goblins_, std::back_inserter(entities));

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

void Game::applyHiZCulling() {
	if (!gfx_.isHiZCullEnabled()) {
		for (auto& p : otherPlayers_) {
			p->setHiZCulled(false);
			if (auto* blender = p->animBlender())
				blender->setCulled(p->isFrustumCulled());
		}
		for (auto& g : goblins_) {
			g->setHiZCulled(false);
			if (auto* blender = g->animBlender())
				blender->setCulled(g->isFrustumCulled());
		}
		return;
	}

	auto applyToEntity = [&](const std::shared_ptr<Object>& entt) {
		const bool hiZVisible = gfx_.getHiZObjectVisible(entt->renderObjectId());
		entt->setHiZCulled(!hiZVisible);
		if (auto* blender = entt->animBlender())
			blender->setCulled(entt->isFrustumCulled() || !hiZVisible);
	};
	for (auto& g : goblins_)
		applyToEntity(g);
	for (auto& p : otherPlayers_)
		applyToEntity(p);
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

}	// namespace Online
