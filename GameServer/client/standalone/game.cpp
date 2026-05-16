#include "pch.hpp"
#include "game.hpp"

#include "../errorHandling.hpp"
#include "../binaryImport.hpp"
#include "../timer.hpp"
#include "../particleImporter.hpp"
#include "../ui/widgets/Label.hpp"
#include "../ui/widgets/ProgressBar.hpp"

extern RECT gClientRect;

namespace StandAlone {

// ---------------------------------------------------------------------------
// Player movement parameters — tweak these to adjust game feel.
//
// kPlayerLinearDamping acts as ground friction: each physics step (60 Hz)
// velocity is multiplied by (1 - damping/60).
//
// For the player to sustain kPlayerMaxSpeed under constant input,
// kPlayerAccelRate must equal kPlayerMaxSpeed * kPlayerLinearDamping.
// (At equilibrium: added_vel_per_step == removed_vel_per_step.)
//
// Stop time from max speed (no input):
//   kPlayerLinearDamping = 10  → ~0.42 s
//   kPlayerLinearDamping = 12  → ~0.33 s  ← default
//   kPlayerLinearDamping = 20  → ~0.20 s  (very snappy)
// ---------------------------------------------------------------------------
static constexpr float kPlayerMaxSpeed      = 10.f;   // m/s
static constexpr float kPlayerLinearDamping = 12.f;   // adjust stop time
static constexpr float kPlayerAccelRate     = kPlayerMaxSpeed * kPlayerLinearDamping;

static constexpr int     kRenderSkipLagFrames = 4;
static constexpr int     kMaxPhysicsStepsPerFrame = 3;
static constexpr Seconds kMaxPhysicsDeltaTime{ 1.f / 60.f * kMaxPhysicsStepsPerFrame };
static constexpr int     kMaxPhysicsScaleK    = 4;   // physicUpdateInterval 최대 배율
static constexpr int     kLagScaleUpFrames    = 2;   // 연속 렉 N프레임 → 배율 1 증가
static constexpr int     kLagScaleDownFrames  = 100; // 연속 정상 N프레임 → 배율 1 감소

Game::Game() {
	// 스레드 풀 초기화
	std::cout << "----------[게임 초기화 설정]----------\n";
	std::cout << "스레드 풀에 사용할 스레드 수를 입력해 주세요.\n";
	std::cout << "컴퓨터의 물리 코어 수: " << numberOfPhysicalCores() << '\n';
	std::cout << "사용 가능한 물리 코어 수: " << numberOfPhysicalCores() - 1 << " (1개 - 메인 스레드)\n";
	std::cout << "스레드 수: ";

	std::size_t threadCnt{};
	threadCnt = 4u;
	// std::cin >> threadCnt;

	threadPool_.run(threadCnt);

	// GFX 객체 초기화
	gfx_.setupDXGI(D3D_FEATURE_LEVEL_12_1);
	gfx_.init();
	gfx_.createSwapChain();
	gfx_.setThreadPool(&threadPool_);


	assetManager_.loadGFXAssets(gfx_, assetConfigs_);
}

Game::~Game() {
	if (goblin_ && goblin_->ragdoll().isBuilt())
		goblin_->ragdoll().destroy(physicsWorld_);
}

void Game::setupStage() {
	const auto path = std::filesystem::path("../resources/levels/level.bin");
	auto ifs = std::ifstream(path, std::ios::binary);
	DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadModelFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, true);

	readHeadTag(ifs, "Level");
	const auto nodeCnt = readInteger(ifs, "NodeCnt");

	importNode(ifs);

	readTailTag(ifs, "Level");

	// 이후 레벨에서 사용하는 스카이박스 정보들을 읽어들일 수 있지만
	// 스카이박스 재질을 하나만 사용하므로 굳이 읽지 않는다.

	gSharedLog << "[Level Load] File I/O: 레벨 " << path << "로드 완료\n";

	dumpLog();

	skybox_.setModel(assetManager_.modelCube());
	skybox_.setSkyboxMaterial(assetManager_.skyboxMaterial());

	dirLight_.setOrient(mu::NQuat(mu::Degree(0.f), mu::Degree(160.f), mu::Degree(0.f)));
	dirLight_.color = mu::Vec3(0.8f, 0.8f, 0.8f);
	dirLight_.intensity = 2.f;
	dirLight_.type = PBRPipeline::LightData::Type::DirectionalLight;
	dirLight_.isMainDirectionalLight = true;

	camera_.setTargetObject( player_ );
	camera_.setOffsetFromTarget( mu::Vec3( 0.f, 1.8f, -2.5f ) );
	camera_.setOffsetTargetPivot( mu::Vec3(0.f, 1.f, 0.f));
	camera_.setPerspective( mu::Degree( 90.f ),
		static_cast<float>( gClientRect.right - gClientRect.left ) / ( gClientRect.bottom - gClientRect.top ),
		0.1f, 500.f
	);
	camera_.setPhysicsWorld(&physicsWorld_);

	//for (auto& cube : cubes_) {
	//	cube.enableBVRendering();
	//}

	player_->enableBVRendering();
	goblin_->enableBVRendering();

	setParticle();

	// 전투 시스템에 참가자 등록
	// 플레이어: 공격 hitbox 및 데미지 설정 (AI 쿨타임은 사용하지 않음)
	combatSystem_.registerCombatant(player_.get(),   { {1.5f, 1.5f, 1.5f}, 1.0f, 30, 500ms  });
	// 몬스터: 종류별로 공격 범위·데미지·쿨타임 차등 적용
	combatSystem_.registerCombatant(goblin_.get(),   { {1.2f, 1.5f, 1.2f}, 0.8f, 15, 2000ms });

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
	effectDropdown_->setup({ "Slash Wave", "Slash Combo", "Slash 7", "Slash 1", "Spikes", "Crystals Front Attack", "AoE Slash Green", "Red Energy Explosion", "Crystals Cross Fade", "Arrow" });
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

	// Hi-Z occlusion culling용 renderObjectId 할당
	{
		u32t nextRenderObjId = 0u;
		goblin_->setRenderObjectId(nextRenderObjId++);
		for (auto& g : goblins_)
			g->setRenderObjectId(nextRenderObjId++);
		gfx_.setMaxRenderObjectId(nextRenderObjId - 1u);
	}

	setupMonsterHpBars();
}

void Game::setupMonsterHpBars() {
	auto registerBar = [&](Object* monster, float yOffset) {
		if (!monster) return;
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
		monsterHpBars_.emplace(monster->getId(), MonsterHpEntry{ monster, bar, yOffset });
	};

	registerBar(goblin_.get(),   2.5f);
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
		cfg.main.duration           = 0.f;   // no time limit

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
		cfg.main.looping = false;  // Re-triggered manually per slash.
		cfg.renderer.mat = assetManager_.smokeBlendCGMaterial();
		swordSlash1Effect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}

	// ── Sword Slash Combo: five Sword Slash 1 layers, tuned only by rotation/burst
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

		{
			auto cfg = baseCfg;
			cfg.main.startRotation3DMin = { 0.f, -2.44346094f, -0.62831855f };
			cfg.main.startRotation3DMax = cfg.main.startRotation3DMin;

			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.10f,
					.countMin = 1,
					.countMax = 1,
					.cycleCount = 1,
					.repeatInterval = 0.01f,
					.probability = 1.f,
				}
			};

			swordSlashComboEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
		}

		{
			auto cfg = baseCfg;
			cfg.main.startRotation3DMin = { 0.f, -2.44346094f, 0.31415927f };
			cfg.main.startRotation3DMax = cfg.main.startRotation3DMin;

			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.45f,
					.countMin = 1,
					.countMax = 1,
					.cycleCount = 1,
					.repeatInterval = 0.01f,
					.probability = 1.f,
				}
			};

			swordSlashComboEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
		}

		{
			auto cfg = baseCfg;
			cfg.main.startRotation3DMin = { 0.f, -2.44346094f, -0.20943952f };
			cfg.main.startRotation3DMax = cfg.main.startRotation3DMin;

			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.6f,
					.countMin = 1,
					.countMax = 1,
					.cycleCount = 1,
					.repeatInterval = 0.01f,
					.probability = 1.f,
				}
			};

			swordSlashComboEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
		}

		{
			auto cfg = baseCfg;
			cfg.main.startRotation3DMin = { 0.f, 2.44346094f, 0.59341192f };
			cfg.main.startRotation3DMax = cfg.main.startRotation3DMin;

			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 1.15f,
					.countMin = 1,
					.countMax = 1,
					.cycleCount = 1,
					.repeatInterval = 0.01f,
					.probability = 1.f,
				}
			};

			swordSlashComboEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
		}

		{
			auto cfg = baseCfg;
			cfg.main.startRotation3DMin = { 0.f, 0.0f, 0.f };
			cfg.main.startRotation3DMax = cfg.main.startRotation3DMin;

			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 1.15f,
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

	// ── Sword Slash 7 / Slash (mesh particle, manual emit) ───────────────────
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

	// ── Sword Slash 7 / Slashes (mesh particle, manual emit) ─────────────────
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

	// ── Spikes Attack effect ───────────────────────────────────────────────
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
				{ 0.0f, { 1.f, 1.f, 1.f, 1.f } },
				{ 0.78f, { 1.f, 1.f, 1.f, 1.f } },
				{ 1.0f, { 1.f, 1.f, 1.f, 0.f } },
			}
		};
		spikesAttackEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── Crystals Front Attack effect ──────────────────────────────────────────
	{
		const std::filesystem::path crystalsJson =
			"../resources/effects/Crystals front attack_ParticleSystems.json";

		// parent: 7-burst trigger particles (invisible, just fires sub-emitter birth events)
		{
			auto cfg = loadUnityParticleConfig(crystalsJson, "Crystals front attack");
			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.mat  = ps::MatUnlit{ .mainTex = nullptr, .additive = true };
			cfg.main.looping  = false;
			// importer does not parse subEmitters yet — set manually
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
				.additive = false,
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
		cfg.renderer.mat      = ps::MatUnlit{ .mainTex = assetManager_.circle2Tex(), .additive = true };
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
		cfg.renderer.mat        = ps::MatUnlit{ .mainTex = assetManager_.slashTex(), .additive = true };
		aoESlashGreenEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/AoE slash green_ParticleSystems.json",
			"AoE slash green/RotatePart"
		);
		cfg.main.looping    = false;
		cfg.renderer.mode   = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat    = ps::MatUnlit{ .mainTex = assetManager_.slashTex(), .additive = true };
		aoESlashGreenEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}

	// ── Slash Wave effect ─────────────────────────────────────────────────────
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

	// ── 발 본 인덱스 탐색 (흙먼지 VFX용) ────────────────────────────────────
	const auto& playerSkeleton = player_->model()->skeleton;
	if ( playerSkeleton.bones ) {
		for ( const auto& bone : *playerSkeleton.bones ) {
			if ( bone.name == "foot_l" )       footBoneIdxLeft_  = bone.boneIdx;
			else if ( bone.name == "foot_r" )  footBoneIdxRight_ = bone.boneIdx;
		}
	}
	if ( footBoneIdxLeft_ < 0 || footBoneIdxRight_ < 0 ) {
		gSharedLog << "[Dust VFX] Warning: foot bones not found. Dumping all bone names:\n";
		if ( playerSkeleton.bones ) {
			for ( const auto& bone : *playerSkeleton.bones )
				gSharedLog << "  bone[" << bone.boneIdx << "] = \"" << bone.name << "\"\n";
		}
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
		cfg.renderer.mat         = ps::MatUnlit{ .mainTex = assetManager_.stoneTex(), .additive = true };
		redEnergyExplosionEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Trails"
		);
		cfg.main.looping         = false;
		cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat         = ps::MatUnlit{ .mainTex = nullptr, .additive = false };

		cfg.trail.enabled              = true;
		cfg.trail.material.mainTex     = assetManager_.trail67Tex();
		cfg.trail.material.additive    = true;
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
		cfg.renderer.mat                = ps::MatUnlit{ .mainTex = assetManager_.circleTex(), .additive = false };
		redEnergyExplosionEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/ShockWaveIn"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.circleTex(), .additive = false };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/ShockWave"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.circleTex(), .additive = false };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Smoke"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.smoke26Tex(), .additive = false };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Flash"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::StretchedBillboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.flashTex(), .additive = false };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}
	// Crystal Cross Fade effect: parent trigger + Crystals sub-emitter only.
	{
		const std::filesystem::path crystalsCrossFadeJson =
			"../resources/effects/Crystals crossfade 2_ParticleSystems.json";

		{
			auto cfg = loadUnityParticleConfig(crystalsCrossFadeJson, "Crystals crossfade 2");
			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.mat  = ps::MatUnlit{ .mainTex = nullptr, .additive = true };
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
				.additive = false,
				.color = { 1.15271747f, 1.1794312f, 1.41421354f, 1.f }
			};
			crystalsCrossFadeEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);  // idx 1
		}

		crystalsCrossFadeEffect_.bindSubEmitter(0, 0, 1);
	// ── Arrow Effect (Muzzle → mesh flight → Hit) ───────────────────────────
	{
		// System 0: Arrow mesh (parent) — flies in player's forward direction
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin        = 0.4f;
			cfg.main.lifetimeMax        = 0.4f;
			cfg.main.speedMin           = 40.f;
			cfg.main.speedMax           = 40.f;
			cfg.main.startSizeMin       = 0.3f;
			cfg.main.startSizeMax       = 0.3f;
			cfg.main.startColor         = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.gravityModifierMin = 0.f;
			cfg.main.gravityModifierMax = 0.f;
			cfg.main.looping            = false;
			cfg.main.duration           = 0.f;

			cfg.emission.emitRate = 0.f;

			cfg.shape.type      = ps::ShapeModule::Type::Point;
			cfg.shape.position  = { 0.f, 0.f, 0.f };
			cfg.shape.direction = { 0.f, 0.f, 1.f };  // local +Z, rotated by player orient at play()

			cfg.renderer.mode        = ps::RendererModule::Mode::Mesh;
			cfg.renderer.renderOrder = 2;
			cfg.renderer.mat         = ps::MatUnlit{ .mainTex = assetManager_.arrowTex(), .additive = false };
			cfg.renderer.pMesh       = assetManager_.meshArrow();
			cfg.renderer.pSubMesh    = assetManager_.meshArrow()->subMeshes.empty()
			                           ? nullptr : &assetManager_.meshArrow()->subMeshes[0];

			// Sub-emitter index 0 = Birth (muzzle), index 1 = Death (hit)
			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = {
				ps::SubEmittersModule::SubEmitter{
					.event           = ps::SubEmittersModule::Event::Birth,
					.emitProbability = 1.f,
					.emitCount       = 1,
					.inheritVelocity = false,
					.inheritColor    = false,
					.inheritSize     = false,
				},
				ps::SubEmittersModule::SubEmitter{
					.event           = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f,
					.emitCount       = 1,
					.inheritVelocity = false,
					.inheritColor    = false,
					.inheritSize     = false,
				},
			};

			arrowEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit, 32);
		}

		// System 1: ArrowMuzzle — spawned at arrow birth position
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin    = 0.f;
			cfg.main.speedMax    = 0.f;
			cfg.main.startSizeMin = 2.0f;
			cfg.main.startSizeMax = 2.0f;
			cfg.main.startColor  = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping     = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts   = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type     = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowMuzzleTex(), .additive = true };

			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;

			arrowEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit, 16);
		}

		// System 2: ArrowHit — spawned at arrow death position
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin  = 0.4f;
			cfg.main.lifetimeMax  = 0.4f;
			cfg.main.speedMin     = 0.f;
			cfg.main.speedMax     = 0.f;
			cfg.main.startSizeMin = 2.5f;
			cfg.main.startSizeMax = 2.5f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping      = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts   = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type     = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowHitTex(), .additive = true };

			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;

			arrowEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit, 16);
		}

		// System 0 Birth → System 1 (muzzle), System 0 Death → System 2 (hit)
		arrowEffect_.bindSubEmitter(0, 0, 1);
		arrowEffect_.bindSubEmitter(0, 1, 2);
	}
}

// Helper: configure a monster body as Dynamic with shared character properties.
static void setupMonsterBody(RigidBody& body, float mass) {
	body.setMotionType(MotionType::Dynamic);
	body.setMass(mass);
	body.setLinearDamping(20.f);
	body.setAngularDamping(25.f);
	body.setUprightStiffness(4000.f);
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

	if (type == "Cube") {
		auto tmp = std::make_shared<Cube>(std::move(object));
		tmp->setModel(assetManager_.modelCube());
		importCube(ifs, *tmp);
	}
	else if (type == "PlayerStart") {
		if (!playerSpawned_) {
			player_ = std::make_shared<Player>(std::move(object));
			playerSpawned_ = true;
			importPlayerStart(ifs, *player_);
			physicsWorld_.registerBody(&player_->body(),
				[p = player_.get()]() { p->rebuildBodyBVH(); });
			player_->body().setUserData(player_.get());
		}
	}
	else if (type == "GoblinSpawner") {
		const float baseX = object.pos().x();
		const float baseY = object.pos().y();
		const float baseZ = object.pos().z();

		goblin_ = std::make_shared<Goblin>(std::move(object));
		importGoblinSpawner(ifs, *goblin_);
		physicsWorld_.registerBody(&goblin_->body(),
			[p = goblin_.get()]() { p->rebuildBodyBVH(); });
		goblin_->body().setUserData(goblin_.get());

		auto urd = std::uniform_real_distribution<float>(-80.f, 80.f);

		for (std::size_t i = 0; i < 1u; ++i) {
			auto& g = goblins_.emplace_back( std::make_shared<Goblin>() );
			g->setPos( mu::Vec3( DirectX::XMLoadFloat3(&worldT) )
				+ mu::Vec3( urd(gRandomEngine), urd(gRandomEngine) + 80.f, urd(gRandomEngine) )
			);
			g->setOrient(DirectX::XMLoadFloat4(&worldR));
			g->setScale(DirectX::XMLoadFloat3(&worldS));

			g->setModel(assetManager_.modelGoblin());
			g->setAnimBlender(animSystem_, assetManager_);
			g->setHp(90);
			g->setMaxHp(90);
			g->setId(1);
			setupMonsterBody(g->body(), 40.f);
			g->enableBVRendering();

			physicsWorld_.registerBody(&g->body(),
				[p = g.get()]() { p->rebuildBodyBVH(); }
			);
			g->body().setUserData(g.get());
		}
	}
	else if (type == "Terrain") {
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

void Game::importCube(std::ifstream& ifs, Cube& cube) {
	const auto meshName = readText(ifs, "Mesh");
	const auto materialSetName = readText(ifs, "MaterialSet");
	const auto materialSetIdx = readInteger(ifs, "MaterialSetIndex");

	cube.setMaterialSetIdx(materialSetIdx);
}

void Game::importPlayerStart(std::ifstream& ifs, Player& player) {
	player.setModel(assetManager_.modelPlayer());
	player.setAnimBlender(animSystem_, assetManager_);
	player.setHp(100);
	player.setMaxHp(100);

	player.body().setMotionType(MotionType::Dynamic);
	player.body().setMass(80.f);
	player.body().setLinearDamping(kPlayerLinearDamping);
	player.body().setAngularDamping(25.f);
	player.body().setUprightStiffness(40000.f);

	//Equipment rifle{};
	//rifle.socketType = Bone::SocketType::RightHand;
	//rifle.object = std::make_unique<Object>();
	//rifle.object->setModel(assetManager_.modelRifle());
	//rifle.object->setScale(mu::Vec3(1.f, 1.f, 1.f));

	//player.equip(std::move(rifle));
}

void Game::importGoblinSpawner(std::ifstream& ifs, Goblin& goblin) {
	goblin.setModel(assetManager_.modelGoblin());
	goblin.setAnimBlender(animSystem_, assetManager_);
	goblin.setHp(90);
	goblin.setMaxHp(90);
	goblin.setId(1);
	setupMonsterBody(goblin.body(), 40.f);

	if (goblin.model() && goblin.model()->ragdollDef) {
		goblin.ragdoll().build(
			goblin.model()->skeleton,
			*goblin.model()->ragdollDef,
			physicsWorld_
		);
	}
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

// 게임의 업데이트는 다음 순서대로 이루어진다.
// 입력 처리
// 이벤트 처리
// 물리 업데이트 루틴
// 객체별 업데이트 루틴
// 애니메이션 업데이트
void Game::update(Milliseconds deltaTime) {
	// 입력 처리
	processInput(deltaTime);

	// debug BV 갱신 (TTL 감소 + 소멸 조건 평가)
	debugBVView_.update(deltaTime);

	// 몬스터 AI 공격 처리
	// 쿨타임 감소 후 플레이어와 AABB 교차 시 EvAttack + EvHit 발생
	if (!playerDead_) {
		combatSystem_.update(deltaTime, player_->getId(), eventList_);
	}

	// 이벤트 처리
	for (auto pEvRaw : eventList_) {
		auto pEv = reinterpret_cast<BasicEvent*>(pEvRaw);
		switch (pEv->type) {
		case EventType::Hit:
			if (static_cast<EvHit*>(pEv)->targetId == goblin_->getId()) {
				goblin_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, goblin_.get());
				if (goblin_->hp() == 0) {
					holdEvent( eventList_, EvDeath(goblin_->getId()) );
				}
				if (auto it = monsterHpBars_.find(goblin_->getId()); it != monsterHpBars_.end())
					it->second.hpBarVisibleSeconds = 5.f;
			}
			else if (static_cast<EvHit*>(pEv)->targetId == player_->getId()) {
				player_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, player_.get());
				if (player_->hp() <= 0 && !playerDead_) {
					playerDead_ = true;
					holdEvent(eventList_, EvDeath(player_->getId()));
				}
			}
			break;

		case EventType::Attack: {
			auto* attack = static_cast<EvAttack*>(pEv);
			if (attack->attackerId == goblin_->getId()) {
				goblin_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, goblin_.get());
			}

			// 공격 발동 시 attack hitbox를 1500ms 동안 live 추적으로 렌더링
			if (auto spec = combatSystem_.queryAttackSpec(attack->attackerId)) {
				debugBVView_.pushLive(spec->obj, spec->halfExtent, spec->offsetFwd, 1500ms);
			}
		}
		break;

		case EventType::Death: {
			auto* death = static_cast<EvDeath*>(pEv);

			if (death->victimId == goblin_->getId()) {
				goblin_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, goblin_.get());
			}
			else if (death->victimId == player_->getId()) {
				player_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, player_.get());
			}

			break;
		}

		default:
			break;
		}
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

	// BV 충돌 색상 업데이트: 기본=초록, Terrain-Object=빨강, Object-Object=파랑
	if (physicsStepsDone > 0) {
		static const mu::Vec4 kColDefault{ 0.f, 1.f, 0.f, 1.f };
		static const mu::Vec4 kColTerrain{ 1.f, 0.f, 0.f, 1.f };
		static const mu::Vec4 kColObjObj { 0.f, 0.f, 1.f, 1.f };

		if (player_) player_->setBVColor(kColDefault);
		if (goblin_) goblin_->setBVColor(kColDefault);
		for (auto& g : goblins_) g->setBVColor(kColDefault);

		// 1패스: Terrain-Object (빨강)
		physicsWorld_.forEachContact([&](const ContactConstraint& cc) {
			auto* objA = static_cast<Object*>(cc.bodyA->userData());
			auto* objB = static_cast<Object*>(cc.bodyB->userData());
			if (objA && !objB) objA->setBVColor(kColTerrain);
			if (objB && !objA) objB->setBVColor(kColTerrain);
		});
		// 2패스: Object-Object (파랑, 빨강보다 우선)
		physicsWorld_.forEachContact([&](const ContactConstraint& cc) {
			auto* objA = static_cast<Object*>(cc.bodyA->userData());
			auto* objB = static_cast<Object*>(cc.bodyB->userData());
			if (objA && objB) {
				objA->setBVColor(kColObjObj);
				objB->setBVColor(kColObjObj);
			}
		});
	}

	// 객체별 업데이트 루틴
	//
	// 물리량 갱신 주기에 대해,
	// 마지막 물리량 갱신으로부터 얼마나 지났는지의 비율로
	// RenderState 갱신을 위한 PhysicState 보간 계수를 설정한다.
	// 게임 객체의 update 함수에 전달된다.
	const auto tPhysicInterpolation = physicUpdateAcc_ / effectiveInterval;

	player_->update(deltaTime, tPhysicInterpolation);
	goblin_->update(deltaTime, tPhysicInterpolation);
	for (auto& g : goblins_) {
		g->update(deltaTime, tPhysicInterpolation);
	}
	
	animSystem_.updatePriorities(
		std::chrono::duration_cast<Seconds>(deltaTime),
		player_->pos()
	);

	camera_.update(deltaTime);
	dirLight_.update(deltaTime);
	dirLight_.updateCSMCascades(camera_.view(), camera_.proj(), assetConfigs_.cascade, assetConfigs_.shadowMap);

	// 몬스터 HP바 위치 갱신 (layout() 이전에 offset 갱신해야 반영됨)
	{
		constexpr float kBarHalfWidth = 40.f;
		const float dtSec = std::chrono::duration<float>(deltaTime).count();
		for (auto& [id, entry] : monsterHpBars_) {
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
				entry.hpBar->offsetX = UI::DimValue::px(sx - kBarHalfWidth);
				entry.hpBar->offsetY = UI::DimValue::px(sy);
				entry.hpBar->setProgress(
					static_cast<float>(entry.monster->hp()) /
					static_cast<float>(entry.monster->maxHp())
				);
			}
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
	uiManager_.update( std::chrono::duration<float>(deltaTime).count(), gfx_, gfx_.defaultFont() );

	// 애니메이션 업데이트
	animSystem_.update(0.01s);

	// Ragdoll 활성화: 이번 프레임 사망 → finalXformData 확정 후 seed + activate
	// Ragdoll 동기화: 활성 ragdoll body 위치를 finalXformData에 덮어씀
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
			rd.activate();
			physicsWorld_.unregisterBody(&g.body());
		};

		auto syncRagdollToAnim = [&](Goblin& g) {
			Ragdoll& rd = g.ragdoll();
			if (!rd.isActive() || !g.animBlender() || !g.model()) return;
			rd.syncToFinalXforms(
				g.animBlender()->finalXformData(),
				g.model()->skeleton,
				g.renderState().world
			);
		};

		activateRagdollIfPending(*goblin_);
		syncRagdollToAnim(*goblin_);
	}

	// 발 흙먼지 방출
	if (footBoneIdxLeft_ >= 0 && footBoneIdxRight_ >= 0
		&& player_->renderState().animBlender) {
		auto* animBlender = static_cast<AnimBlenderPlayer*>(
			player_->renderState().animBlender.get());

		const auto vel = player_->velocity();
		const float hSpeed2 = vel.x() * vel.x() + vel.z() * vel.z();
		constexpr float kDustMinSpeed = 1.0f;

		if (animBlender->isRunning() && hSpeed2 >= kDustMinSpeed * kDustMinSpeed) {
			const auto duration = animBlender->runDuration();
			const auto currTime = animBlender->runAnimTime();
			const float currPhase = currTime / duration;
			const float prevPhase = prevAnimTimeRun_ / duration;

			constexpr float kLeftFootContact  = 0.0f;
			constexpr float kRightFootContact = 0.5f;

			auto crossedPhase = [&](float phase) -> bool {
				if (currPhase >= prevPhase) {
					return prevPhase < phase && currPhase >= phase;
				} else {
					return prevPhase < phase || currPhase >= phase;
				}
			};

			const auto& skeleton = player_->model()->skeleton;
			const auto& boneXforms = animBlender->finalXformData();
			const auto& world = player_->renderState().world;

			auto getBoneWorldPos = [&](int boneIdx) -> mu::Vec3 {
				const auto& bone = skeleton.bones->at(boneIdx);
				const mu::Mat4x4 boneToWorld = bone.toDress
					* boneXforms[boneIdx] * world;
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

	// 파티클
	flameParticleSystem_.update( deltaTime );
	smokeParticleSystem_.update( deltaTime );
	swordSlash1Effect_.update( deltaTime );
	swordSlash7Effect_.update( deltaTime );
	swordSlashComboEffect_.update( deltaTime );
	slashWaveEffect_.update( deltaTime );
	spikesAttackEffect_.update( deltaTime );
	crystalsFrontAttackEffect_.update( deltaTime );
	aoESlashGreenEffect_.update( deltaTime );
	dustParticleSystem_.update( deltaTime );
	redEnergyExplosionEffect_.update( deltaTime );
	crystalsCrossFadeEffect_.update( deltaTime );
	arrowEffect_.update( deltaTime );

	// UI 동기화
	if (playerHpBar_)
		playerHpBar_->setProgress(player_->hp() / 100.f);

	clearEvents(eventList_);
}

void Game::render() {
	if (skipNextRender_) {
		skipNextRender_ = false;
		return;
	}

	cullObjects();

	debugBVView_.render(gfx_);
	player_->render(gfx_);
	goblin_->render(gfx_);
	for (auto& g : goblins_) {
		g->render(gfx_);
	}
	skybox_.render(gfx_);
	camera_.updateGFX(gfx_);
	dirLight_.render(gfx_);

	flameParticleSystem_.render( gfx_ );
	smokeParticleSystem_.render( gfx_ );
	swordSlash1Effect_.render( gfx_ );
	swordSlash7Effect_.render( gfx_ );
	swordSlashComboEffect_.render( gfx_ );
	slashWaveEffect_.render( gfx_ );
	spikesAttackEffect_.render( gfx_ );
	crystalsFrontAttackEffect_.render( gfx_ );
	aoESlashGreenEffect_.render( gfx_ );
	dustParticleSystem_.render( gfx_ );
	redEnergyExplosionEffect_.render( gfx_ );
	crystalsCrossFadeEffect_.render( gfx_ );
	arrowEffect_.render( gfx_ );

	uiManager_.render( gfx_ );

	// FrameData와 gfx_.render()는 렉 상황에도 항상 호출한다 (DX12 swapchain 동기화 유지).
	gfx_.addFrameData( PBRPipeline::FrameData{ .globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f ) } );
	gfx_.addFrameData( PBRSkinnedPipeline::FrameData{ .globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f ) } );
	gfx_.addFrameData( PBRDeferredPipeline::FrameData{ .globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f ) } );
	gfx_.addFrameData( PBRDeferredSkinnedPipeline::FrameData{ .globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f ) } );

	if (terrain_) {
		terrain_->render(gfx_);
		gfx_.addFrameData(TerrainPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
		gfx_.addFrameData(TerrainDeferredPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
	}

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
		rawInputResult = GetRawInputData( reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT, nullptr, &rawInputSize, sizeof(RAWINPUTHEADER)
		);
		DISPLAY_ERROR_GLE(rawInputResult != -1, true);

		if (rawInputSize > sRawInputBuffer.size()) {
			sRawInputBuffer.resize(rawInputSize);
		}

		// 입력 구조체 내용 수신
		rawInputResult = GetRawInputData( reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT, sRawInputBuffer.data(), &rawInputSize, sizeof(RAWINPUTHEADER)
		);
		DISPLAY_ERROR_GLE(rawInputResult == rawInputSize, true);

		auto ri = reinterpret_cast<const RAWINPUT*>(sRawInputBuffer.data());
		if (ri->header.dwType == RIM_TYPEMOUSE) {
			// 마우스에 대한 입력 내용이 상대 좌표여야 한다.
			DISPLAY_ERROR_STR( !(ri->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE),
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
		break;

	default:
		uiManager_.onWndMsg(msg, wParam, lParam);
		break;
	}

	return DefWindowProcA(hWnd, msg, wParam, lParam);
};

void Game::processInput(Milliseconds deltaTime) {
	if (GetForegroundWindow() != ghWnd) {
		return;
	}

	keyboardStatePrev_ = keyboardStateCurr_;
	DISPLAY_ERROR_GLE( GetKeyboardState(keyboardStateCurr_.data()), false );

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

		// x/z 방향으로만 가속. y(중력)는 물리 엔진이 담당.
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

	// 플레이어 공격: LButton 클릭 시 forward 방향 hitbox와 몬스터 AABB 교차 검사
	if ( !playerDead_
		&& !uiManager_.needsCursor()
		&& (keyboardStateCurr_[VK_LBUTTON] & 0x80)
		&& !(keyboardStatePrev_[VK_LBUTTON] & 0x80)
		) {
		combatSystem_.onPlayerAttack( player_->getId(), eventList_ );
		// 공격 발동 시 플레이어 attack hitbox를 1500ms 동안 live 추적으로 렌더링
		if ( auto spec = combatSystem_.queryAttackSpec( player_->getId() ) ) {
			debugBVView_.pushLive( spec->obj, spec->halfExtent, spec->offsetFwd, 1500ms );
		}

		// 검기 이펙트 emit: SlashPath가 플레이어 forward 방향으로 날아가도록 회전을 함께 전달한다.
		const auto slashPos = player_->renderState().pos
			+ player_->forward() * 1.f
			+ mu::Vec3( 0.f, 1.0f, 0.f );
		switch ( currentEffect_ ) {
		case SwordEffect::SlashCombo: swordSlashComboEffect_.play( slashPos ); break;
		case SwordEffect::Slash7:     swordSlash7Effect_.play( slashPos );     break;
		case SwordEffect::Slash1:     swordSlash1Effect_.play( slashPos );     break;
		case SwordEffect::SlashWave:  slashWaveEffect_.play( slashPos, player_->orient() );       break;
		case SwordEffect::Spikes:              spikesAttackEffect_.play( slashPos );          break;
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
		case SwordEffect::RedEnergyExplosion: {
			auto explosionPos = player_->renderState().pos + player_->forward() * 6.5f;
			if ( terrain_ && !assetManager_.terrain()->heightField.empty() ) {
				const auto terrainPos = terrain_->renderState().pos;
				const float localX = explosionPos.x() - terrainPos.x();
				const float localZ = explosionPos.z() - terrainPos.z();
				const float groundY = terrainPos.y()
					+ assetManager_.terrain()->heightField.getHeightAt( localX, localZ );
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
		}
	}

	// 마우스 민감도를 기반으로 플레이어 yaw, 카메라 pitch를 계산한다.
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
		player_->setOrient(player_->orient() * yawRotation);
		camera_.setOffsetFromTargetPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, mu::Radian(0.f)) );
	}
	else {
		cameraYaw_ += yaw;
		camera_.setOffsetFromTargetPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, cameraYaw_) );
	}

	mouseDeltaX_ = 0;
	mouseDeltaY_ = 0;

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

	// U key: toggle UI debug overlay (colored bounding boxes)
	if ( (keyboardStateCurr_['U'] & 0x80) && !(keyboardStatePrev_['U'] & 0x80) ) {
		uiManager_.toggleDebugMode();
	}

	// F key: emit particles for testing
	if ( (keyboardStateCurr_['F'] & 0x80) && !(keyboardStatePrev_['F'] & 0x80) ) {
		const auto slashPos = player_->renderState().pos
			+ player_->forward() * 1.f
			+ mu::Vec3( 0.f, 1.0f, 0.f );
		swordSlash1Effect_.play(slashPos);
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

void Game::cullObjects() {
	auto entities = std::vector< std::shared_ptr<Object> >{};
	entities.reserve(1u + goblins_.size());

	entities.push_back(goblin_);
	for (auto& g : goblins_) {
		entities.push_back(g);
	}

	// Gribb-Hartmann: extract 6 frustum planes from view-proj matrix.
	// DirectXMath row-vector convention: v' = v * M, so P_clip[j] = dot(P, col(j)).
	// Plane equation: dot(plane.xyz, P) + plane.w >= 0 means inside.
	const auto vp = camera_.view() * camera_.proj();
	const auto c0 = vp.col(0);
	const auto c1 = vp.col(1);
	const auto c2 = vp.col(2);
	const auto c3 = vp.col(3);

	const mu::Vec4 planes[6] = {
		c3 + c0,   // left
		c3 - c0,   // right
		c3 + c1,   // bottom
		c3 - c1,   // top
		c2,        // near
		c3 - c2,   // far
	};

	for (auto& entt : entities) {
		auto& rootShape = entt->body().worldBVH().nodes[0].shape;
		bool culled = false;

		if (std::holds_alternative<AABB>(rootShape)) {
			const auto& aabb = std::get<AABB>(rootShape);
			const mu::Vec3 c = aabb.center;
			const mu::Vec3 h = aabb.size * 0.5f;

			for (const auto& pl : planes) {
				// Projection radius of AABB onto plane normal
				const float e = std::abs(pl.x()) * h.x()
							  + std::abs(pl.y()) * h.y()
							  + std::abs(pl.z()) * h.z();
				const float dist = pl.x() * c.x() + pl.y() * c.y()
								 + pl.z() * c.z() + pl.w();
				if (dist + e < 0.f) { culled = true; break; }
			}
		} else {
			const auto& obb = std::get<OBB>(rootShape);
			const mu::Vec3 c  = obb.center;
			const mu::Vec3 ax = obb.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
			const mu::Vec3 ay = obb.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
			const mu::Vec3 az = obb.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));

			for (const auto& pl : planes) {
				const float e =
					std::abs(pl.x()*ax.x() + pl.y()*ax.y() + pl.z()*ax.z()) * obb.halfExtents.x()
				  + std::abs(pl.x()*ay.x() + pl.y()*ay.y() + pl.z()*ay.z()) * obb.halfExtents.y()
				  + std::abs(pl.x()*az.x() + pl.y()*az.y() + pl.z()*az.z()) * obb.halfExtents.z();
				const float dist = pl.x()*c.x() + pl.y()*c.y() + pl.z()*c.z() + pl.w();
				if (dist + e < 0.f) { culled = true; break; }
			}
		}

		entt->setFrustumCulled(culled);
	}
}

void Game::applyHiZCulling() {
	if (!gfx_.isHiZCullEnabled()) {
		for (auto& entt : { std::static_pointer_cast<Object>(goblin_) }) {
			entt->setHiZCulled(false);
			if (auto* blender = entt->animBlender())
				blender->setCulled(entt->isFrustumCulled());
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
	applyToEntity(goblin_);
	for (auto& g : goblins_)
		applyToEntity(g);
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

}	// namespace StandAlone
