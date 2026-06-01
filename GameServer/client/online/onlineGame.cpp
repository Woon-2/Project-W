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

	assetManager_.loadGFXAssets( gfx_, assetConfigs_ );
}

void Game::setupStage() {
	const auto path = std::filesystem::path("../resources/levels/level.bin");
	auto ifs = std::ifstream(path, std::ios::binary);
	DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadModelFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, true);

	readHeadTag(ifs, "Level");
	const auto nodeCnt = readInteger(ifs, "NodeCnt");

	importNode(ifs);

	readTailTag(ifs, "Level");

	// 지형 청크 스트리밍 매니저 초기화 (팔레트 + 인덱스 로드, 동기 baseline 로드).
	chunkManager_.init(gfx_, physicsWorld_, &threadPool_, "../resources/terrains/");

	skybox_.setModel( assetManager_.modelCube( ) );
	skybox_.setSkyboxMaterial( assetManager_.skyboxMaterial( ) );

	dirLight_.setOrient( mu::NQuat( mu::Degree( 0.f ), mu::Degree( 160.f ), mu::Degree( 0.f ) ) );
	dirLight_.color = mu::Vec3( 0.8f, 0.8f, 0.8f );
	dirLight_.intensity = 2.f;
	dirLight_.type = PBRPipeline::LightData::Type::DirectionalLight;
	dirLight_.isMainDirectionalLight = true;

	uiManager_.setScreenSize(
		static_cast<float>(gClientRect.right - gClientRect.left),
		static_cast<float>(gClientRect.bottom - gClientRect.top)
	);
	uiManager_.requestDebugResources(gfx_);
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

	playerHpBar_ = static_cast<UI::ProgressBar*>(
		uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
	);
	playerHpBar_->name    = "playerHpBar";
	playerHpBar_->anchor  = UI::Anchors::TopLeft;
	playerHpBar_->pivot   = UI::Pivots::TopLeft;
	playerHpBar_->width   = UI::DimValue::px(300.f);
	playerHpBar_->height  = UI::DimValue::px(18.f);
	playerHpBar_->offsetX = UI::DimValue::px(20.f);
	playerHpBar_->offsetY = UI::DimValue::px(20.f);
	playerHpBar_->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
	playerHpBar_->fillColor = { 1.0f, 0.0f, 0.0f, 1.00f };
	playerHpBar_->setProgress(1.f);

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

	auto loadUnityParticleConfig = [](const std::filesystem::path& jsonPath,
	                                  std::string_view relativePath) {
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
	player_->setHp(100);
	player_->setMaxHp(100);
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
	otherPlayer->setHp(100);
	otherPlayer->setMaxHp(100);
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
	otherPlayer->setHp(100);
	otherPlayer->setMaxHp(100);
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

	goblin->setHp(90);
	goblin->setMaxHp(90);
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

	if ( npc->animBlender() ) {
		npc->animBlender()->triggerAttack();
	}
}

void Game::onPlayerAttack( uint16 attackerId ) {
	auto it = idPlayerMap_.find( attackerId );
	if ( it != idPlayerMap_.end() && it->second->animBlender() ) {
		it->second->animBlender()->triggerAttack();
	}
}

void Game::applyHit( uint16 targetId, int32 newHp ) {
	if ( player_ && static_cast<uint16>(player_->getId()) == targetId ) {
		player_->setHp( newHp );

		if ( newHp <= 0 ) {
			playerDead_ = true;
			player_->setDead( true );
		}
		return;
	}
	if ( auto it = idPlayerMap_.find( targetId ); it != idPlayerMap_.end() ) {
		it->second->setHp( newHp );
		if ( newHp <= 0 && !it->second->isDead() ) {
			it->second->setDead( true );
		}
		return;
	}
	if ( auto it = idGoblinMap_.find( targetId ); it != idGoblinMap_.end() ) {
		it->second->setHp( newHp );
		if ( newHp <= 0 && !it->second->isDead() ) {
			it->second->setDead( true );
			it->second->setRagdollPendingActivation( true );
		}
		if ( auto barIt = goblinHpBars_.find( targetId ); barIt != goblinHpBars_.end() )
			barIt->second.hpBarVisibleSeconds = 5.f;
	}
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
	npc->setDead( false );
	if (npc->ragdoll().isActive())
		npc->ragdoll().deactivate(physicsWorld_);
	npc->setPos( DirectX::XMLoadFloat3( &spawnPos ) );
}

void Game::onSkillStart( uint16 ownerId, uint32 skillAssetId, uint16 elapsedMs ) {
	// Trigger attack animation on the remote player that cast the skill.
	if (auto it = idPlayerMap_.find(ownerId); it != idPlayerMap_.end()) {
		if (it->second->animBlender())
			it->second->animBlender()->triggerAttack();
	}

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

	animSystem_.updatePriorities(
		std::chrono::duration_cast<Seconds>(deltaTime),
		player_->pos()
	);

	// 지형 청크 스트리밍 틱 (카메라 갱신 전에 호출).
	chunkManager_.update(player_->pos(), deltaTime);

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

		if (playerHpBar_)
			playerHpBar_->setProgress(
				static_cast<float>(player_->hp()) / static_cast<float>(player_->maxHp())
			);

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

void Game::render() {
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

	uiManager_.render(gfx_);

	auto frameDataUI = UIPipeline::FrameData{
		.screenWidth = static_cast<float>( gClientRect.right - gClientRect.left ),
		.screenHeight = static_cast<float>( gClientRect.bottom - gClientRect.top )
	};
	gfx_.addFrameData(frameDataUI);

	gfx_.render();
	applyHiZCulling();
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

// 방에 들어가지 않은 상태일 때
// 방 입장 키 처리
void Game::processInputLobby(Milliseconds deltaTime) {
	if (keyboardStateCurr_['1'] & 0x80) {
		//sendEnterRoomPacket(1);
	}
	if (keyboardStateCurr_['2'] & 0x80) {
		//sendEnterRoomPacket(2);
	}
	if (keyboardStateCurr_['3'] & 0x80) {
		//sendEnterRoomPacket(3);
	}
	if (keyboardStateCurr_['4'] & 0x80) {
		//sendEnterRoomPacket(4);
	}
}

void Game::processInputGame(Milliseconds deltaTime) {
	// 이동 가속도만 담당. 감속은 PhysicsWorld의 linearDamping(마찰)이 처리한다.
	// 속도 상한은 kPlayerMaxSpeed, 가속률은 kPlayerAccelRate (파일 상단 상수 참조).
	const auto moveXSign = !playerDead_ * ( (keyboardStateCurr_['D'] & 0x80) - (keyboardStateCurr_['A'] & 0x80) );
	const auto moveZSign = !playerDead_ * ( (keyboardStateCurr_['W'] & 0x80) - (keyboardStateCurr_['S'] & 0x80) );

	if (moveXSign || moveZSign) {
		// 'W'/'S' 입력으로 판정된 Z 부호는 플레이어의 forward 벡터,
		// 'D'/'A' 입력으로 판정된 X 부호는 플레이어의 right 벡터와 곱해 속도의 방향을 정한다.
		const auto moveDirection = mu::NVec3(
			static_cast<float>(moveXSign) * player_->right() + static_cast<float>(moveZSign) * player_->forward()
		);

		// 입력 방향으로 가속. 최대 속력 초과분은 클램프한다.
		auto vel = player_->velocity() + mu::Vec3(moveDirection) * (kPlayerAccelRate * Seconds(deltaTime).count());
		if (vel.len2() > kPlayerMaxSpeed * kPlayerMaxSpeed) {
			vel = vel * (kPlayerMaxSpeed / vel.len());
		}
		player_->setVelocity(vel);
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

	// G key: cycle GBuffer debug view (deferred path only)
	if ( (keyboardStateCurr_['G'] & 0x80) && !(keyboardStatePrev_['G'] & 0x80) ) {
		gfx_.cycleGBufferDebugMode();
	}

	// 마우스 민감도를 기반으로 1인칭 카메라 모드와 3인칭 카메라 모드일 때
	// 각각의 플레이어 yaw, 카메라 pitch를 계산한다.
	// (pitch를 플레이어에 적용하게 되면, 플레이어가 고개를 들고 내리는 게 아니라 굴러버린다.)
	const auto mouseSensitivity = mu::pi * 2.f;

	const auto prevForward = player_->forward();

	const auto yaw = mu::Radian(mouseDeltaX_ * mouseSensitivity / static_cast<float>(gClientRect.right - gClientRect.left));
	auto yawRotation = mu::NQuat(mu::Radian(0.f), mu::Radian(0.f), yaw);

	cameraPitch_ = std::clamp(
		static_cast<float>(cameraPitch_) + mouseDeltaY_ * mouseSensitivity / static_cast<float>(gClientRect.bottom - gClientRect.top),
		-mu::pi * 0.16f,
		mu::pi * 0.3f
	);

	if (!playerDead_) {
		player_->setOrient(player_->orient() * yawRotation);
		camera_.setOffsetFromTargetPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, mu::Radian(0.f)) );
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
		if ( player_->animBlender() )
			player_->animBlender()->triggerAttack();

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
