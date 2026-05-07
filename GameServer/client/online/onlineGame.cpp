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

static constexpr int     kRenderSkipLagFrames = 4;
static constexpr int     kMaxPhysicsStepsPerFrame = 3;
static constexpr Seconds kMaxPhysicsDeltaTime{ 1.f / 60.f * kMaxPhysicsStepsPerFrame };
static constexpr int     kMaxPhysicsScaleK    = 4;
static constexpr int     kLagScaleUpFrames    = 2;
static constexpr int     kLagScaleDownFrames  = 100;

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
	assetManager_.loadAnimations();
}

void Game::setupStage() {
	const auto path = std::filesystem::path("../resources/levels/level.bin");
	auto ifs = std::ifstream(path, std::ios::binary);
	DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadModelFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, true);

	readHeadTag(ifs, "Level");
	const auto nodeCnt = readInteger(ifs, "NodeCnt");

	importNode(ifs);

	readTailTag(ifs, "Level");

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
	pLabel->setText(L"U: UI영역 표시\nEnter: 마우스 포인터 캡처\nSpace: 마우스 포인터 감추기\nWASD: 이동\nG: GBuffer 버퍼내용 순환(0=None, 1=Albedo, ..., 7=Depth)\nH: Hi-Z Cull ON/OFF\n좌클릭: 공격 ");
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
	effectDropdown_->setup({ "Slash Wave", "Slash Combo", "Slash 7", "Slash 1", "Spikes" });
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
		terrain_ = std::make_shared<TerrainObject>(std::move(object));
		importTerrain(ifs, *terrain_);
		terrain_->update(0ms, 1.f);
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

void Game::importTerrain(std::ifstream& ifs, TerrainObject& terrain) {
	const auto manifestPath = readText(ifs, "ManifestPath");
	terrain.setTerrainData(assetManager_.terrain());

	// 지형 물리 바디 설정: Static body (위치는 importNode WorldTRS에서 설정됨)
	terrain.body().setMotionType(MotionType::Static);

	// Hi-Z Occlusion Occluder 설정
	terrain.activateOcclusion(true);

	// TerrainCollider 등록 (BVH 불필요 — heightField 직접 조회)
	const TerrainData* td = assetManager_.terrain();
	if (td && !td->heightField.empty())
		physicsWorld_.registerTerrain(&terrain.body(), &td->heightField);
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
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.flameTex(), .additive = true };

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

	physicsWorld_.registerBody(&player_->body(),
		[p = player_.get()]() { p->rebuildBodyBVH(); });

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
	physicsWorld_.registerBody(&otherPlayer->body(),
		[p = otherPlayer.get()]() { p->rebuildBodyBVH(); });

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
	// physicsWorld_.registerBody(&otherPlayer->body(),
		// [p = otherPlayer.get()]() { p->rebuildBodyBVH(); });

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
	goblin->setHp(90);
	goblin->setMaxHp(90);
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

	if (auto it = otherPlayerHpBars_.find(playerId); it != otherPlayerHpBars_.end()) {
		uiManager_.root()->removeChild(it->second.hpBar);
		otherPlayerHpBars_.erase(it);
	}

	otherPlayers_.erase(itPlayer);
	idPlayerMap_.erase( playerId );
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
		}
	}
}

void Game::applyTimeSync( uint64 serverMs ) {
	serverClockOffset_ = static_cast<int64>(serverMs) -
		static_cast<int64>(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count());
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
	npc->setPos( DirectX::XMLoadFloat3( &spawnPos ) );
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

	camera_.update(deltaTime);
	dirLight_.update(deltaTime);
	dirLight_.updateCSMCascades(camera_.view(), camera_.proj(), assetConfigs_.cascade, assetConfigs_.shadowMap);

	// 애니메이션 업데이트
	animSystem_.update(0.01s);

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

		for (auto& [id, entry] : goblinHpBars_) {
			if (!entry.goblin || entry.goblin->hp() <= 0) {
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
		dustParticleSystem_.update(deltaTime);

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
	dustParticleSystem_.render(gfx_);

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

	if (terrain_) {
		terrain_->render(gfx_);
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
		static_cast<int64>(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count()) + serverClockOffset_);
	INet::ClientApp::addSendBuffer(PacketManager::makeCAttackPacket(clientMs));
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