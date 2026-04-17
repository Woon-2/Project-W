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

	//for (auto& cube : cubes_) {
	//	cube.enableBVRendering();
	//}

	player_->enableBVRendering();
	goblin_->enableBVRendering();
	anubis_->enableBVRendering();
	bat_->enableBVRendering();
	bomber_->enableBVRendering();
	demon_->enableBVRendering();
	dragon_->enableBVRendering();
	eyeball_->enableBVRendering();
	fishman_->enableBVRendering();
	gargoyle_->enableBVRendering();

	setParticle();

	// 전투 시스템에 참가자 등록
	// 플레이어: 공격 hitbox 및 데미지 설정 (AI 쿨타임은 사용하지 않음)
	combatSystem_.registerCombatant(player_.get(),   { {1.5f, 1.5f, 1.5f}, 1.0f, 30, 500ms  });
	// 몬스터: 종류별로 공격 범위·데미지·쿨타임 차등 적용
	combatSystem_.registerCombatant(goblin_.get(),   { {1.2f, 1.5f, 1.2f}, 0.8f, 15, 2000ms });
	combatSystem_.registerCombatant(anubis_.get(),   { {1.5f, 2.0f, 1.5f}, 1.0f, 25, 3000ms });
	combatSystem_.registerCombatant(bat_.get(),      { {0.8f, 0.8f, 0.8f}, 0.6f, 10, 1500ms });
	combatSystem_.registerCombatant(bomber_.get(),   { {1.5f, 1.5f, 1.5f}, 1.2f, 30, 4000ms });
	combatSystem_.registerCombatant(demon_.get(),    { {1.5f, 2.0f, 1.5f}, 1.0f, 20, 2500ms });
	combatSystem_.registerCombatant(dragon_.get(),   { {2.5f, 2.0f, 2.5f}, 1.5f, 40, 5000ms });
	combatSystem_.registerCombatant(eyeball_.get(),  { {1.2f, 1.2f, 1.2f}, 1.0f, 15, 2000ms });
	combatSystem_.registerCombatant(fishman_.get(),  { {1.2f, 1.8f, 1.2f}, 0.9f, 20, 2500ms });
	combatSystem_.registerCombatant(gargoyle_.get(), { {1.5f, 2.0f, 1.5f}, 1.0f, 25, 3000ms });

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
	pLabel->offsetX = UI::DimValue::px( -340.f );
	pLabel->offsetY = UI::DimValue::px( -220.f );
	pLabel->setTextHAlign(UI::TextHAlign::Center);
	pLabel->setTextVAlign(UI::TextVAlign::Center);
	pLabel->setText(L"U: UI영역 표시\nEnter: 마우스 포인터 캡처\nSpace: 마우스 포인터 감추기\nWASD: 이동\nC: Cascade Debug View\n좌클릭: 공격 ");
	pLabel->setFontSize(24.0f);
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
		monsterHpBars_.push_back({ monster, bar, yOffset });
	};

	registerBar(goblin_.get(),   2.5f);
	registerBar(anubis_.get(),   3.5f);
	registerBar(bat_.get(),      1.5f);
	registerBar(bomber_.get(),   3.0f);
	registerBar(demon_.get(),    4.0f);
	registerBar(dragon_.get(),   6.0f);
	registerBar(eyeball_.get(),  1.5f);
	registerBar(fishman_.get(),  3.0f);
	registerBar(gargoyle_.get(), 3.5f);
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
		}
	}
	else if (type == "GoblinSpawner") {
		goblin_ = std::make_shared<Goblin>(std::move(object));
		importGoblinSpawner(ifs, *goblin_);
		physicsWorld_.registerBody(&goblin_->body(),
			[p = goblin_.get()]() { p->rebuildBodyBVH(); });
	}
	else if (type == "AnubisSpawner") {
		anubis_ = std::make_shared<Anubis>(std::move(object));
		importAnubisSpawner(ifs, *anubis_);
		physicsWorld_.registerBody(&anubis_->body(),
			[p = anubis_.get()]() { p->rebuildBodyBVH(); });
	}
	else if (type == "BatSpawner") {
		bat_ = std::make_shared<Bat>(std::move(object));
		importBatSpawner(ifs, *bat_);
		physicsWorld_.registerBody(&bat_->body(),
			[p = bat_.get()]() { p->rebuildBodyBVH(); });
	}
	else if (type == "BomberSpawner") {
		bomber_ = std::make_shared<Bomber>(std::move(object));
		importBomberSpawner(ifs, *bomber_);
		physicsWorld_.registerBody(&bomber_->body(),
			[p = bomber_.get()]() { p->rebuildBodyBVH(); });
	}
	else if (type == "DemonSpawner") {
		demon_ = std::make_shared<Demon>(std::move(object));
		importDemonSpawner(ifs, *demon_);
		physicsWorld_.registerBody(&demon_->body(),
			[p = demon_.get()]() { p->rebuildBodyBVH(); });
	}
	else if (type == "DragonSpawner") {
		dragon_ = std::make_shared<Dragon>(std::move(object));
		importDragonSpawner(ifs, *dragon_);
		physicsWorld_.registerBody(&dragon_->body(),
			[p = dragon_.get()]() { p->rebuildBodyBVH(); });
	}
	else if (type == "EyeballSpawner") {
		eyeball_ = std::make_shared<Eyeball>(std::move(object));
		importEyeballSpawner(ifs, *eyeball_);
		physicsWorld_.registerBody(&eyeball_->body(),
			[p = eyeball_.get()]() { p->rebuildBodyBVH(); });
	}
	else if (type == "FishmanSpawner") {
		fishman_ = std::make_shared<Fishman>(std::move(object));
		importFishmanSpawner(ifs, *fishman_);
		physicsWorld_.registerBody(&fishman_->body(),
			[p = fishman_.get()]() { p->rebuildBodyBVH(); });
	}
	else if (type == "GargoyleSpawner") {
		gargoyle_ = std::make_shared<Gargoyle>(std::move(object));
		importGargoyleSpawner(ifs, *gargoyle_);
		physicsWorld_.registerBody(&gargoyle_->body(),
			[p = gargoyle_.get()]() { p->rebuildBodyBVH(); });
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
	// Prevent collision impulses from tipping/spinning the character.
	player.body().setAngularDamping(100.f);

	//Equipment rifle{};
	//rifle.socketType = Bone::SocketType::RightHand;
	//rifle.object = std::make_unique<Object>();
	//rifle.object->setModel(assetManager_.modelRifle());
	//rifle.object->setScale(mu::Vec3(1.f, 1.f, 1.f));

	//player.equip(std::move(rifle));
}

// Helper: configure a monster body as Dynamic with shared character properties.
static void setupMonsterBody(RigidBody& body, float mass) {
	body.setMotionType(MotionType::Dynamic);
	body.setMass(mass);
	body.setLinearDamping(20.f);
	body.setAngularDamping(100.f);  // prevent impulse-driven tipping/spinning
}

void Game::importGoblinSpawner(std::ifstream& ifs, Goblin& goblin) {
	goblin.setModel(assetManager_.modelGoblin());
	goblin.setAnimBlender(animSystem_, assetManager_);
	goblin.setHp(90);
	goblin.setMaxHp(90);
	goblin.setId(1);
	setupMonsterBody(goblin.body(), 40.f);
}

void Game::importAnubisSpawner(std::ifstream& ifs, Anubis& anubis) {
	anubis.setModel(assetManager_.modelAnubis());
	anubis.setAnimBlender(animSystem_, assetManager_);
	anubis.setHp(110);
	anubis.setMaxHp(110);
	anubis.setId(2);
	setupMonsterBody(anubis.body(), 90.f);
}

void Game::importBatSpawner(std::ifstream& ifs, Bat& bat) {
	bat.setModel(assetManager_.modelBat());
	bat.setAnimBlender(animSystem_, assetManager_);
	bat.setHp(40);
	bat.setMaxHp(40);
	bat.setId(3);
	setupMonsterBody(bat.body(), 20.f);
}

void Game::importBomberSpawner(std::ifstream& ifs, Bomber& bomber) {
	bomber.setModel(assetManager_.modelBomber());
	bomber.setAnimBlender(animSystem_, assetManager_);
	bomber.setHp(60);
	bomber.setMaxHp(60);
	bomber.setId(4);
	setupMonsterBody(bomber.body(), 60.f);
}

void Game::importDemonSpawner(std::ifstream& ifs, Demon& demon) {
	demon.setModel(assetManager_.modelDemon());
	demon.setAnimBlender(animSystem_, assetManager_);
	demon.setHp(140);
	demon.setMaxHp(140);
	demon.setId(5);
	setupMonsterBody(demon.body(), 100.f);
}

void Game::importDragonSpawner(std::ifstream& ifs, Dragon& dragon) {
	dragon.setModel(assetManager_.modelDragon());
	dragon.setAnimBlender(animSystem_, assetManager_);
	dragon.setHp(250);
	dragon.setMaxHp(250);
	dragon.setId(6);
	setupMonsterBody(dragon.body(), 200.f);
}

void Game::importEyeballSpawner(std::ifstream& ifs, Eyeball& eyeball) {
	eyeball.setModel(assetManager_.modelEyeball());
	eyeball.setAnimBlender(animSystem_, assetManager_);
	eyeball.setHp(120);
	eyeball.setMaxHp(120);
	eyeball.setId(7);
	setupMonsterBody(eyeball.body(), 30.f);
}

void Game::importFishmanSpawner(std::ifstream& ifs, Fishman& fishman) {
	fishman.setModel(assetManager_.modelFishman());
	fishman.setAnimBlender(animSystem_, assetManager_);
	fishman.setHp(80);
	fishman.setMaxHp(80);
	fishman.setId(8);
	setupMonsterBody(fishman.body(), 70.f);
}

void Game::importGargoyleSpawner(std::ifstream& ifs, Gargoyle& gargoyle) {
	gargoyle.setModel(assetManager_.modelGargoyle());
	gargoyle.setAnimBlender(animSystem_, assetManager_);
	gargoyle.setHp(160);
	gargoyle.setMaxHp(160);
	gargoyle.setId(9);
	setupMonsterBody(gargoyle.body(), 120.f);
}

void Game::importTerrain(std::ifstream& ifs, TerrainObject& terrain) {
	const auto manifestPath = readText(ifs, "ManifestPath");
	terrain.setTerrainData(assetManager_.terrain());

	// 지형 물리 바디 설정: Static body (위치는 importNode WorldTRS에서 설정됨)
	terrain.body().setMotionType(MotionType::Static);

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
			}
			else if (static_cast<EvHit*>(pEv)->targetId == anubis_->getId()) {
				anubis_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, anubis_.get());
				if (anubis_->hp() == 0) {
					holdEvent( eventList_, EvDeath(anubis_->getId()) );
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == bat_->getId()) {
				bat_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bat_.get());
				if (bat_->hp() == 0) {
					holdEvent(eventList_, EvDeath(bat_->getId()));
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == bomber_->getId()) {
				bomber_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bomber_.get());
				if (bomber_->hp() == 0) {
					holdEvent(eventList_, EvDeath(bomber_->getId()));
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == demon_->getId()) {
				demon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, demon_.get());
				if (demon_->hp() == 0) {
					holdEvent(eventList_, EvDeath(demon_->getId()));
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == dragon_->getId()) {
				dragon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, dragon_.get());
				if (dragon_->hp() == 0) {
					holdEvent(eventList_, EvDeath(dragon_->getId()));
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == eyeball_->getId()) {
				eyeball_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, eyeball_.get());
				if (eyeball_->hp() == 0) {
					holdEvent(eventList_, EvDeath(eyeball_->getId()));
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == fishman_->getId()) {
				fishman_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, fishman_.get());
				if (fishman_->hp() == 0) {
					holdEvent( eventList_, EvDeath(fishman_->getId()) );
				}
			}
			else if (static_cast<EvHit*>(pEv)->targetId == gargoyle_->getId()) {
				gargoyle_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, gargoyle_.get());
				if (gargoyle_->hp() == 0) {
					holdEvent(eventList_, EvDeath(gargoyle_->getId()));
				}
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
			else if (attack->attackerId == anubis_->getId()) {
				anubis_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, anubis_.get());
			}
			else if (attack->attackerId == bat_->getId()) {
				bat_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bat_.get());
			}
			else if (attack->attackerId == bomber_->getId()) {
				bomber_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bomber_.get());
			}
			else if (attack->attackerId == demon_->getId()) {
				demon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, demon_.get());
			}
			else if (attack->attackerId == dragon_->getId()) {
				dragon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, dragon_.get());
			}
			else if (attack->attackerId == eyeball_->getId()) {
				eyeball_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, eyeball_.get());
			}
			else if (attack->attackerId == fishman_->getId()) {
				fishman_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, fishman_.get());
			}
			else if (attack->attackerId == gargoyle_->getId()) {
				gargoyle_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, gargoyle_.get());
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
			else if (death->victimId == anubis_->getId()) {
				anubis_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, anubis_.get());
			}
			else if (death->victimId == bat_->getId()) {
				bat_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bat_.get());
			}
			else if (death->victimId == bomber_->getId()) {
				bomber_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, bomber_.get());
			}
			else if (death->victimId == demon_->getId()) {
				demon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, demon_.get());
			}
			else if (death->victimId == dragon_->getId()) {
				dragon_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, dragon_.get());
			}
			else if (death->victimId == eyeball_->getId()) {
				eyeball_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, eyeball_.get());
			}
			else if (death->victimId == fishman_->getId()) {
				fishman_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, fishman_.get());
			}
			else if (death->victimId == gargoyle_->getId()) {
				gargoyle_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, gargoyle_.get());
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
	physicUpdateAcc_ += deltaTime;

	if (physicUpdateAcc_ >= physicUpdateInterval) {
		while (physicUpdateAcc_ >= physicUpdateInterval) {
			physicsWorld_.step(physicUpdateInterval);
			physicUpdateAcc_ -= physicUpdateInterval;
		}
	}

	// 객체별 업데이트 루틴
	//
	// 물리량 갱신 주기에 대해,
	// 마지막 물리량 갱신으로부터 얼마나 지났는지의 비율로
	// RenderState 갱신을 위한 PhysicState 보간 계수를 설정한다.
	// 게임 객체의 update 함수에 전달된다.
	const auto tPhysicInterpolation = physicUpdateAcc_ / physicUpdateInterval;

	player_->update(deltaTime, tPhysicInterpolation);
	goblin_->update(deltaTime, tPhysicInterpolation);
	anubis_->update(deltaTime, tPhysicInterpolation);
	bat_->update(deltaTime, tPhysicInterpolation);
	bomber_->update(deltaTime, tPhysicInterpolation);
	demon_->update(deltaTime, tPhysicInterpolation);
	dragon_->update(deltaTime, tPhysicInterpolation);
	eyeball_->update(deltaTime, tPhysicInterpolation);
	fishman_->update(deltaTime, tPhysicInterpolation);
	gargoyle_->update(deltaTime, tPhysicInterpolation);
	camera_.update();
	dirLight_.update(deltaTime);
	dirLight_.updateCSMCascades(camera_.view(), camera_.proj(), assetConfigs_.cascade, assetConfigs_.shadowMap);

	// 몬스터 HP바 위치 갱신 (layout() 이전에 offset 갱신해야 반영됨)
	{
		constexpr float kBarHalfWidth = 40.f;
		for (auto& entry : monsterHpBars_) {
			if (!entry.monster || entry.monster->hp() <= 0) {
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

	uiManager_.layout();
	uiManager_.update( std::chrono::duration<float>(deltaTime).count(), gfx_, gfx_.defaultFont() );

	// 애니메이션 업데이트
	animSystem_.update(0.016s);

	// 발 흙먼지 방출
	if (footBoneIdxLeft_ >= 0 && footBoneIdxRight_ >= 0
		&& player_->renderState().animBlender) {
		auto* animBlender = static_cast<AnimBlenderPlayer*>(
			player_->renderState().animBlender.get());

		if (animBlender->isRunning()) {
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
	dustParticleSystem_.update( deltaTime );

	// UI 동기화
	if (playerHpBar_)
		playerHpBar_->setProgress(player_->hp() / 100.f);

	clearEvents(eventList_);
}

void Game::render() {
	debugBVView_.render(gfx_);
	player_->render(gfx_);
	goblin_->render(gfx_);
	anubis_->render(gfx_);
	bat_->render(gfx_);
	bomber_->render(gfx_);
	demon_->render(gfx_);
	dragon_->render(gfx_);
	eyeball_->render(gfx_);
	fishman_->render(gfx_);
	gargoyle_->render(gfx_);
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
	dustParticleSystem_.render( gfx_ );

	uiManager_.render( gfx_ );

	auto frameDataPBR = PBRPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData( frameDataPBR );
	auto frameDataPBRSkinned = PBRSkinnedPipeline::FrameData{
		.globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f )
	};
	gfx_.addFrameData( frameDataPBRSkinned );

	if (terrain_) {
		terrain_->render(gfx_);
		gfx_.addFrameData(TerrainPipeline::FrameData{
			.globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f)
		});
	}

	gfx_.render();
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

	// 카메라 1인칭 모드 설정
	if ( keyboardStateCurr_['1'] & 0x80 ) {
		camera_.setOffsetFromTargetPreRotation( mu::NQuat{} );
		camera_.setOffsetFromTarget( mu::Vec3( 0.f, 2.f, 0.5f ) );
		camera_.setOffsetTargetPivot( mu::Vec3(0.f, 2.f, 8.f));
		cameraMode_ = CameraMode::FirstPerson;
	} 
	// 카메라 3인칭 모드 설정
	if ( keyboardStateCurr_['3'] & 0x80 ) {
		camera_.setXXPreRotation( mu::NQuat{} );
		camera_.setOffsetFromTarget( mu::Vec3( 0.f, 1.8f, -2.5f ) );
		camera_.setOffsetTargetPivot( mu::Vec3(0.f, 1.f, 0.f));
		cameraMode_ = CameraMode::ThirdPerson;
	}

	// 플레이어 공격: LButton 클릭 시 forward 방향 hitbox와 몬스터 AABB 교차 검사
	if ( !playerDead_
		&& (keyboardStateCurr_[VK_LBUTTON] & 0x80)
		&& !(keyboardStatePrev_[VK_LBUTTON] & 0x80)
	) {
		combatSystem_.onPlayerAttack(player_->getId(), eventList_);
		// 공격 발동 시 플레이어 attack hitbox를 1500ms 동안 live 추적으로 렌더링
		if (auto spec = combatSystem_.queryAttackSpec(player_->getId())) {
			debugBVView_.pushLive(spec->obj, spec->halfExtent, spec->offsetFwd, 1500ms);
		}

		// 검기 이펙트 emit: SlashPath가 플레이어 forward 방향으로 날아가도록 회전을 함께 전달한다.
		const auto slashPos = player_->renderState().pos
		                    + player_->forward() * 1.f
		                    + mu::Vec3(0.f, 1.0f, 0.f);
		// swordSlashComboEffect_.play( slashPos );
		// swordSlash7Effect_.play( slashPos );
		// swordSlash1Effect_.play(slashPos);
		// slashWaveEffect_.play( slashPos, player_->orient() );
		// spikesAttackEffect_.play( slashPos );
	}

	// 마우스 민감도를 기반으로 1인칭 카메라 모드와 3인칭 카메라 모드일 때
	// 각각의 플레이어 yaw, 카메라 pitch를 계산한다.
	// (pitch를 플레이어에 적용하게 되면, 플레이어가 고개를 들고 내리는 게 아니라 굴러버린다.)
    const auto mouseSensitivity = mu::pi * 2.f;

	switch (cameraMode_) {
	case CameraMode::ThirdPerson: {
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
		break;
	}
	case CameraMode::FirstPerson: {
		const auto yaw = mu::Radian(mouseDeltaX_ * mouseSensitivity / static_cast<float>(gClientRect.right - gClientRect.left));
		auto yawRotation = mu::NQuat(mu::Radian(0.f), mu::Radian(0.f), yaw);

		if (!playerDead_) {
			player_->setOrient(player_->orient() * yawRotation);
		}

		cameraPitch_ = std::clamp(
			static_cast<float>(cameraPitch_) + mouseDeltaY_ * mouseSensitivity / static_cast<float>(gClientRect.bottom - gClientRect.top),
			-mu::pi * 0.16f,
			mu::pi * 0.3f
		);

		if (!playerDead_) {
			player_->setOrient(player_->orient() * yawRotation);
			camera_.setXXPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, mu::Radian(0.f)) );
		}
		else {
			cameraYaw_ += yaw;
			camera_.setXXPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, cameraYaw_) );
		}

		mouseDeltaX_ = 0;
		mouseDeltaY_ = 0;
		break;
	}
	}

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
