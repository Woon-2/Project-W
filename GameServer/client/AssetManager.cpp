#include "pch.hpp"
#include "AssetManager.hpp"

namespace {

ps::MatSwordSlash makeDefaultSwordSlashMaterial() {
	return ps::MatSwordSlash{
		.speedMainTexUV    = { 0.f, 0.5f },
		.speedDissolveUV   = { 0.f, 0.f },
		.speedFlow         = { 0.f, 0.f },
		.mainTexST         = { 1.f, 1.f, 0.f, 0.f },
		.emissionTexST     = { 1.f, 1.f, 0.f, 0.f },
		.dissolveTexST     = { 1.f, 1.f, 0.f, 0.f },
		.flowTexST         = { 1.f, 1.f, 0.f, 0.f },
		.flowPower         = 0.f,
		.emission          = 4.79f,
		.desaturation      = 0.f,
		.remap             = { -0.3f, 1.f },
		.addColor          = { 0.0113418f, 0.f, 0.1981132f, 1.f },
		.opacity           = 0.1f,
		.useSmoothDissolve = true,
	};
}

}  // namespace

const Texture* AssetManager::playerWeaponIcon(PlayerWeaponType weaponType) const {
	switch (weaponType) {
	case PlayerWeaponType::Katana:
		return &katana_;
	case PlayerWeaponType::SpearHook:
		return &spearHook_;
	case PlayerWeaponType::CrystalWand:
		return &crystalWand_;
	case PlayerWeaponType::HeavyArrow:
		return &heavyArrow_;
	default:
		return &katana_;
	}
}

const Texture* AssetManager::skillIconByAssetName(std::string_view n) const {
	if (n == "SlashWave")            return &skillIconSlashWave_;
	if (n == "Slash7")               return &skillIconSlash7_;
	if (n == "SlashCombo")           return &skillIconSlashCombo_;
	if (n == "ArrowVolley")          return &skillIconArrowVolley_;
	if (n == "ArrowRain")            return &skillIconArrowRain_;
	if (n == "EnergyExplosionArrow") return &skillIconEnergyExplosionArrow_;
	if (n == "CrystalsFrontAttack")  return &skillIconCrystalsFront_;
	if (n == "CrystalsCrossFade")    return &skillIconCrystalsCross_;
	if (n == "RedEnergyExplosion")   return &skillIconRedEnergy_;
	if (n == "PiercingSlash")        return &skillIconPiercingSlash_;
	if (n == "PiercingCircleSlash")  return &skillIconPiercingCircle_;
	if (n == "PiercingMulti")        return &skillIconPiercingMulti_;
	return nullptr;
}

void AssetManager::loadGFXAssets(GFX& gfx, const AssetConfigs& configs) {
	// Full load = Phase 1 (lobby visual) then Phase 2 (remaining in-game).
	loadLobbyVisualAssets(gfx, configs);
	loadRemainingInGameAssets(gfx, configs);
}

void AssetManager::loadLobbyVisualAssets(GFX& gfx, const AssetConfigs& configs) {
	// Minimum set needed to render the waiting-room 3D: cube (skybox mesh),
	// player model, skybox material, and the full player animation set.
	// (Loading only idle would require changing AnimBlenderPlayer::init(), which
	//  registers the whole playerAnimations() set — so load them all here.)
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/cube/cube.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelCube_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/player/player.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelPlayer_
	} );

	gfx.addRequestSkyboxLoad( RequestSkyboxLoad{
		.skyboxPath = "../resources/skyboxes/skybox.bin",
		.pDest = &skyboxMaterial_
	} );

	auto tmpPlayerAnims = loadAnimClipsFromFile("../resources/animations/playerAnimations.anim", gfx);
	playerAnimations_.reserve(tmpPlayerAnims.size());

	gfx.loadRequestedAssets();

	for (auto& clip : tmpPlayerAnims) {
		playerAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

	// Player baked-anim ids (goblin vector is still empty here — handled in Phase 2).
	setupBakedAnimationIds();
}

void AssetManager::loadRemainingInGameAssets(GFX& gfx, const AssetConfigs& configs) {
	// Remaining assets needed only in-game.
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/goblin/goblin.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelGoblin_
	} );
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/goblin/Hobgoblin.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelHobgoblin_
	} );
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/snake/snake.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelSnake_
	} );
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/mushroom/mushroom.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelMushroom_
	} );

	// Monster skill-caster model loads.
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/bomber/bomber.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelBomber_
	} );
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/birdy/birdy.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelBirdy_
	} );
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/slime/slime.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelSlime_
	} );
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/treant/treant.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelTreant_
	} );
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/boss/boss.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelBoss_
	} );

	// Mid-boss variant models (share Treant/Birdy rigs; rendered for Grandbaum/Isys bosses).
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/treant/Grandbaum.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelGrandbaum_
	} );
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/birdy/Isys.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelIsys_
	} );

	//gfx.addRequestTextureLoad( RequestTextureLoad{
	//	.name = "PlayerHpLine",
	//	.texturePath = "../resources/UI/player_hp_line.dds",
	//	.pDest = &playerHpLine_,
	//	.pTexHashMap = &texHashMap_,
	//	.needsUploadInfo = false
	//} );

	//gfx.addRequestTextureLoad( RequestTextureLoad{
	//	.name = "PlayerHpFrame",
	//	.texturePath = "../resources/UI/player_hp_frame.dds",
	//	.pDest = &playerHpFrame_,
	//	.pTexHashMap = &texHashMap_,
	//	.needsUploadInfo = false
	//} );

	// Terrain is now streamed by TerrainChunkManager (chunks_index.bin), not loaded here.
	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name           = "FlameTex",
		.texturePath    = "../resources/Sprites/Flame.dds",
		.pDest          = &flameTex_,
		.pTexHashMap    = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name           = "SmokeTex",
		.texturePath    = "../resources/Sprites/A_Smoke_2.dds",
		.pDest          = &smokeTex_,
		.pTexHashMap    = &texHashMap_,
		.needsUploadInfo = false
	} );

	// Combat feedback UI: damage-number digit atlas + kill-count skull icon.
	// Clamp sampler avoids UV bleed across atlas cell borders.
	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "PlayerHpHeart",
		.texturePath     = "../resources/UI/hearts.dds",
		.pDest           = &playerHpHeart_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "katana",
		.texturePath     = "../resources/UI/katana.dds",
		.pDest           = &katana_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "spear-hook",
		.texturePath     = "../resources/UI/spear-hook.dds",
		.pDest           = &spearHook_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "crystal-wand",
		.texturePath     = "../resources/UI/crystal-wand.dds",
		.pDest           = &crystalWand_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "heavy-arrow",
		.texturePath     = "../resources/UI/heavy-arrow.dds",
		.pDest           = &heavyArrow_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "UiLeftButton",
		.texturePath     = "../resources/UI/ui_left_button.dds",
		.pDest           = &uiLeftButton_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "DamageDigits",
		.texturePath     = "../resources/UI/damage_digits.dds",
		.pDest           = &digitAtlasTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "KillIcon",
		.texturePath     = "../resources/UI/icon_kill.dds",
		.pDest           = &killIconTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	// Skill dial icons (12) — one per selectable weapon skill.
	auto loadSkillIcon = [&]( const char* name, const char* file, Texture* dest ) {
		gfx.addRequestTextureLoad( RequestTextureLoad{
			.name            = name,
			.texturePath     = file,
			.pDest           = dest,
			.pTexHashMap     = &texHashMap_,
			.needsUploadInfo = false,
			.sampler         = Samplers::BilinearClamp
		} );
	};
	loadSkillIcon( "SkillIconSlashWave",           "../resources/UI/slash_wave.dds",            &skillIconSlashWave_ );
	loadSkillIcon( "SkillIconSlash7",              "../resources/UI/slash_7.dds",               &skillIconSlash7_ );
	loadSkillIcon( "SkillIconSlashCombo",          "../resources/UI/slash_combo.dds",           &skillIconSlashCombo_ );
	loadSkillIcon( "SkillIconArrowVolley",         "../resources/UI/arrow_volley.dds",          &skillIconArrowVolley_ );
	loadSkillIcon( "SkillIconArrowRain",           "../resources/UI/arrow_rain.dds",            &skillIconArrowRain_ );
	loadSkillIcon( "SkillIconEnergyExplosionArrow","../resources/UI/energy_explosion_arrow.dds",&skillIconEnergyExplosionArrow_ );
	loadSkillIcon( "SkillIconCrystalsFront",       "../resources/UI/crystals_front_attack.dds", &skillIconCrystalsFront_ );
	loadSkillIcon( "SkillIconCrystalsCross",       "../resources/UI/crystals_cross_fade.dds",   &skillIconCrystalsCross_ );
	loadSkillIcon( "SkillIconRedEnergy",           "../resources/UI/red_energy_explosion.dds",  &skillIconRedEnergy_ );
	loadSkillIcon( "SkillIconPiercingSlash",       "../resources/UI/piercing_slash.dds",        &skillIconPiercingSlash_ );
	loadSkillIcon( "SkillIconPiercingCircle",      "../resources/UI/piercing_circle_slash.dds", &skillIconPiercingCircle_ );
	loadSkillIcon( "SkillIconPiercingMulti",       "../resources/UI/piercing_multi.dds",        &skillIconPiercingMulti_ );

	gfx.addRequestMeshBinLoad( RequestMeshBinLoad{
		.meshPath    = "../resources/effects/Slash3.meshbin",
		.pTexHashMap = &texHashMap_,
		.pDestMesh   = &meshSlash3_,
		.pDestTex    = nullptr
	} );

	gfx.addRequestMeshBinLoad( RequestMeshBinLoad{
		.meshPath    = "../resources/effects/HalfTrail.meshbin",
		.pTexHashMap = &texHashMap_,
		.pDestMesh   = &meshHalfTrail_,
		.pDestTex    = nullptr
	} );

	gfx.addRequestMeshBinLoad( RequestMeshBinLoad{
		.meshPath    = "../resources/effects/SlashWave.meshbin",
		.pTexHashMap = &texHashMap_,
		.pDestMesh   = &meshSlashWave_,
		.pDestTex    = nullptr
	} );

	gfx.addRequestMeshBinLoad( RequestMeshBinLoad{
		.meshPath    = "../resources/effects/IceSpikes2.meshbin",
		.pTexHashMap = &texHashMap_,
		.pDestMesh   = &meshIceSpikes2_,
		.pDestTex    = nullptr
	} );

	gfx.addRequestMeshBinLoad( RequestMeshBinLoad{
		.meshPath    = "../resources/effects/Arrow.meshbin",
		.pTexHashMap = &texHashMap_,
		.pDestMesh   = &meshArrow_,
		.pDestTex    = nullptr
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Arrow",
		.texturePath     = "../resources/Textures/Arrow.dds",
		.pDest           = &arrowTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearWrap
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Smoke24",
		.texturePath     = "../resources/Textures/Smoke24.dds",
		.pDest           = &smoke24Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Smoke26",
		.texturePath     = "../resources/Textures/Smoke26.dds",
		.pDest           = &smoke26Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Flash",
		.texturePath     = "../resources/Textures/Flash.dds",
		.pDest           = &flashTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Smoke12",
		.texturePath     = "../resources/Textures/Smoke12.dds",
		.pDest           = &smoke12Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::TrilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Crater62",
		.texturePath     = "../resources/Textures/Crater62.dds",
		.pDest           = &crater62Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::TrilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Noise43b",
		.texturePath     = "../resources/Textures/Noise43b.dds",
		.pDest           = &noise43bTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Fire13",
		.texturePath     = "../resources/Textures/Fire13.dds",
		.pDest           = &fire13Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Trail62For",
		.texturePath     = "../resources/Textures/Trail62For.dds",
		.pDest           = &trail62ForTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Trail62",
		.texturePath     = "../resources/Textures/Trail62.dds",
		.pDest           = &trail62Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Trail67",
		.texturePath     = "../resources/Textures/Trail67.dds",
		.pDest           = &trail67Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Circle",
		.texturePath     = "../resources/Textures/Circle.dds",
		.pDest           = &circleTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "MagicCircle",
		.texturePath     = "../resources/Textures/magic_circle.dds",
		.pDest           = &magicCircleTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Stone",
		.texturePath     = "../resources/Textures/Stone.dds",
		.pDest           = &stoneTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Gradient3t",
		.texturePath     = "../resources/Textures/Gradient3t.dds",
		.pDest           = &gradient3tTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Gradient4.3",
		.texturePath     = "../resources/Textures/Gradient4.3.dds",
		.pDest           = &gradient4_3Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Waves21",
		.texturePath     = "../resources/Textures/Waves21.dds",
		.pDest           = &waves21Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearWrap
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Noise62",
		.texturePath     = "../resources/Textures/Noise62.dds",
		.pDest           = &noise62Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearWrap
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Noise1",
		.texturePath     = "../resources/Textures/Noise1.dds",
		.pDest           = &noise1Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearWrap
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "CrystalFree1",
		.texturePath     = "../resources/Textures/CrystalFree1.dds",
		.pDest           = &crystalFree1Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Circle2",
		.texturePath     = "../resources/Textures/Circle2.dds",
		.pDest           = &circle2Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "Slash",
		.texturePath     = "../resources/Textures/Slash.dds",
		.pDest           = &slashTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "ArrowHit",
		.texturePath     = "../resources/Textures/ArrowHit_8x4.dds",
		.pDest           = &arrowHitTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "ArrowMuzzle",
		.texturePath     = "../resources/Textures/ArrowMuzzle_8x4.dds",
		.pDest           = &arrowMuzzleTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name = "EnergyExplosionHit",
		.texturePath = "../resources/Textures/EnergyExplosionHit_8x6_Core.dds",
		.pDest = &energyExplosionHitTex_,
		.pTexHashMap = &texHashMap_,
		.needsUploadInfo = false,
		.sampler = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name = "EnergyExplosionHitWhiteBG",
		.texturePath = "../resources/Textures/EnergyExplosionHit_8x6_WightBG.dds",
		.pDest = &energyExplosionHitWhiteBGTex_,
		.pTexHashMap = &texHashMap_,
		.needsUploadInfo = false,
		.sampler = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name = "EnergyExplosionCharge",
		.texturePath = "../resources/Textures/EnergyExplosionCharge_8x6.dds",
		.pDest = &energyExplosionChargeTex_,
		.pTexHashMap = &texHashMap_,
		.needsUploadInfo = false,
		.sampler = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name = "EnergyExplosionArrow",
		.texturePath = "../resources/Textures/ArrowEnergyExplosion.dds",
		.pDest = &energyExplosionArrowTex_,
		.pTexHashMap = &texHashMap_,
		.needsUploadInfo = false,
		.sampler = Samplers::BilinearClamp
	} );

	gfx.addRequestMeshBinLoad( RequestMeshBinLoad{
		.meshPath    = "../resources/effects/Ring.meshbin",
		.pTexHashMap = &texHashMap_,
		.pDestMesh   = &meshRing_,
		.pDestTex    = nullptr
	} );

	gfx.addRequestMeshBinLoad( RequestMeshBinLoad{
		.meshPath    = "../resources/effects/SM_VFX_Projectile_02.meshbin",
		.pTexHashMap = &texHashMap_,
		.pDestMesh   = &meshVfxProjectile02_,
		.pDestTex    = nullptr
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "PiercingFire",
		.texturePath     = "../resources/Textures/T_VFX_Piercing_Fire.dds",
		.pDest           = &piercingFireTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "VfxNoise01",
		.texturePath     = "../resources/Textures/T_VFX_Noises_01.dds",
		.pDest           = &vfxNoise01Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "VfxNoise02",
		.texturePath     = "../resources/Textures/T_VFX_Noises_02.dds",
		.pDest           = &vfxNoise02Tex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "PiercingGradientMask",
		.texturePath     = "../resources/Textures/T_VFX_Piercing_Generic_Gradient_Mask_01.dds",
		.pDest           = &piercingGradientMaskTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "SlashFire",
		.texturePath     = "../resources/Textures/T_VFX_Slash_Fire.dds",
		.pDest           = &slashFireTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "SlashMask",
		.texturePath     = "../resources/Textures/T_VFX_Slash_Mask_01.dds",
		.pDest           = &slashMaskTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "GradientMirrorVertical",
		.texturePath     = "../resources/Textures/T_VFX_Linear_Gradient_Mirror_Vertical_01.dds",
		.pDest           = &gradientMirrorVerticalTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "SlashGeneric",
		.texturePath     = "../resources/Textures/T_VFX_Slash_Generic.dds",
		.pDest           = &slashGenericTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "SlashCircleMask",
		.texturePath     = "../resources/Textures/T_VFX_Slash_Circle_Mask_01.dds",
		.pDest           = &slashCircleMaskTex_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false,
		.sampler         = Samplers::BilinearClamp
	} );

	gfx.addRequestMeshBinLoad( RequestMeshBinLoad{
		.meshPath    = "../resources/effects/SM_VFX_Slash_01_HD.meshbin",
		.pTexHashMap = &texHashMap_,
		.pDestMesh   = &meshVfxSlash01HD_,
		.pDestTex    = nullptr
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "TexWindRing",
		.texturePath     = "../resources/textures/Tex_WindRing.dds",
		.pDest           = &texWindRing_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "TexDotParticle",
		.texturePath     = "../resources/textures/Tex_DotParticle.dds",
		.pDest           = &texDotParticle_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "WindBulletMuzzle",
		.texturePath     = "../resources/Textures/WindBulletMuzzle_8x6.dds",
		.pDest           = &texWindBulletMuzzle_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name            = "WindBulletHit",
		.texturePath     = "../resources/Textures/WindBulletHit_8x6.dds",
		.pDest           = &texWindBulletHit_,
		.pTexHashMap     = &texHashMap_,
		.needsUploadInfo = false
	} );

	swordSlashMaterial_ = makeDefaultSwordSlashMaterial();
	if (loadSwordSlashMaterialMetadata("../resources/effects/SwordSlashMat.json", swordSlashMaterial_)) {
		gSharedLog << "[SwordSlash Material] File I/O: ../resources/effects/SwordSlashMat.json 로드 완료\n";
	} else {
		gSharedLog << "[SwordSlash Material] Warning: ../resources/effects/SwordSlashMat.json 로드 실패. 기본값을 사용합니다.\n";
	}

	swordSlash2Material_ = makeDefaultSwordSlashMaterial();
	if (loadSwordSlashMaterialMetadata("../resources/effects/SwordSlash2.json", swordSlash2Material_)) {
		gSharedLog << "[SwordSlash2 Material] File I/O: ../resources/effects/SwordSlash2.json 로드 완료\n";
	} else {
		gSharedLog << "[SwordSlash2 Material] Warning: ../resources/effects/SwordSlash2.json 로드 실패. 기본값을 사용합니다.\n";
	}

	auto tmpGoblinAnims   = loadAnimClipsFromFile("../resources/animations/goblinAnimations.anim", gfx);
	auto tmpSnakeAnims    = loadAnimClipsFromFile("../resources/animations/snakeAnimations.anim", gfx);
	auto tmpMushroomAnims = loadAnimClipsFromFile("../resources/animations/mushroomAnimations.anim", gfx);
	goblinAnimations_.reserve(tmpGoblinAnims.size());
	snakeAnimations_.reserve(tmpSnakeAnims.size());
	mushroomAnimations_.reserve(tmpMushroomAnims.size());

	// Monster skill-caster animation loads (part 1/2: load + reserve; tmp* used in part 2).
	auto tmpBomberAnims = loadAnimClipsFromFile("../resources/animations/bomberAnimations.anim", gfx);
	bomberAnimations_.reserve(tmpBomberAnims.size());
	auto tmpBirdyAnims = loadAnimClipsFromFile("../resources/animations/birdyAnimations.anim", gfx);
	birdyAnimations_.reserve(tmpBirdyAnims.size());
	auto tmpSlimeAnims = loadAnimClipsFromFile("../resources/animations/slimeAnimations.anim", gfx);
	slimeAnimations_.reserve(tmpSlimeAnims.size());
	auto tmpTreantAnims = loadAnimClipsFromFile("../resources/animations/treantAnimations.anim", gfx);
	treantAnimations_.reserve(tmpTreantAnims.size());
	auto tmpBossAnims = loadAnimClipsFromFile("../resources/boss/bossAnimations.anim", gfx);
	bossAnimations_.reserve(tmpBossAnims.size());

	// 공용 리소스(gfx.initSharedResources)는 호출부에서 미리 초기화되어 있어야 한다.
	// 여기서는 요청된 리소스만 로드한다. (백그라운드 스레드에서 호출 가능)
	gfx.loadRequestedAssets();

	for (auto& clip : tmpGoblinAnims)
		goblinAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	for (auto& clip : tmpSnakeAnims)
		snakeAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	for (auto& clip : tmpMushroomAnims)
		mushroomAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );

	// Monster skill-caster animation loads (part 2/2: move into members).
	for (auto& clip : tmpBomberAnims)
		bomberAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	for (auto& clip : tmpBirdyAnims)
		birdyAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	for (auto& clip : tmpSlimeAnims)
		slimeAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	for (auto& clip : tmpTreantAnims)
		treantAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	for (auto& clip : tmpBossAnims)
		bossAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );

	// Re-runs player ids (harmless) and assigns goblin baked-anim ids.
	setupBakedAnimationIds();

	swordSlashMaterial_.mainTex = &smoke12Tex_;
	swordSlashMaterial_.emissionTex = &crater62Tex_;
	swordSlashMaterial_.dissolveTex = &noise43bTex_;
	swordSlashMaterial_.flowTex = nullptr;

	swordSlash2Material_.mainTex = &smoke12Tex_;
	swordSlash2Material_.emissionTex = &fire13Tex_;
	swordSlash2Material_.dissolveTex = &noise43bTex_;
	swordSlash2Material_.flowTex = nullptr;

	smokeBlendCGMaterial_ = ps::MatSmokeBlendCG{};
	smokeBlendCGMaterial_.mainTex = &smoke24Tex_;

	// TwoSides22 material — textures loaded from TwoSides22.json property names:
	//   _MainTex=Trail62For, _Mask=Trail62, _Noise=Noise (reuse Noise43b)
	twoSidesMaterial_ = ps::MatTwoSides{};
	twoSidesMaterial_.mainTex           = &trail62ForTex_;
	twoSidesMaterial_.maskTex           = &trail62Tex_;
	twoSidesMaterial_.noiseTex          = &noise43bTex_;
	twoSidesMaterial_.noiseTexST        = { 0.1f, 2.0f, 0.f, 0.f };
	twoSidesMaterial_.texSpeed          = { 0.f, 0.f, 0.f, 0.f };
	twoSidesMaterial_.emission          = 3.0f;
	twoSidesMaterial_.opacity           = 1.0f;
	twoSidesMaterial_.useFresnel        = 0.0f;
	twoSidesMaterial_.fresnelPower      = 2.0f;
	twoSidesMaterial_.frontFacesColor   = { 1.f, 1.f, 1.f, 1.f };
	twoSidesMaterial_.backFacesColor    = { 0.6415f, 0.6415f, 0.6415f, 1.f };
	twoSidesMaterial_.fresnelColor      = { 1.f, 1.f, 1.f, 1.f };
	twoSidesMaterial_.fresnelEmission   = 1.0f;
	twoSidesMaterial_.useBackFresnel    = 1.0f;
	twoSidesMaterial_.backFresnel       = -4.0f;
	twoSidesMaterial_.backFresnelEmission = 1.0f;
	twoSidesMaterial_.backFresnelColor  = { 1.f, 1.f, 1.f, 1.f };

	// TwoSides37 material — IceSpikes2 mesh particle.
	// _MainTex=Noise62 (ST 1x1), _Noise=Noise1 (ST 2x2), _Mask=none
	spikesMaterial_ = ps::MatTwoSides{};
	spikesMaterial_.mainTex            = &noise62Tex_;
	spikesMaterial_.maskTex            = nullptr;
	spikesMaterial_.noiseTex           = &noise1Tex_;
	spikesMaterial_.mainTexST          = { 1.f, 1.f, 0.f, 0.f };
	spikesMaterial_.maskTexST          = { 1.f, 1.f, 0.f, 0.f };
	spikesMaterial_.noiseTexST         = { 2.f, 2.f, 0.f, 0.f };
	spikesMaterial_.texSpeed           = { 0.f, 0.f, 0.f, 0.f };
	spikesMaterial_.emission           = 7.0f;
	spikesMaterial_.opacity            = 1.0f;
	spikesMaterial_.useFresnel         = 1.0f;
	spikesMaterial_.fresnelPower       = 2.0f;
	spikesMaterial_.frontFacesColor    = { 0.5755f, 0.5755f, 0.5755f, 1.f };
	spikesMaterial_.backFacesColor     = { 1.f, 1.f, 1.f, 1.f };
	spikesMaterial_.fresnelColor       = { 1.f, 1.f, 1.f, 1.f };
	spikesMaterial_.fresnelEmission    = 1.0f;
	spikesMaterial_.useBackFresnel     = 1.0f;
	spikesMaterial_.backFresnel        = -4.0f;
	spikesMaterial_.backFresnelEmission = 1.0f;
	spikesMaterial_.backFresnelColor   = { 1.f, 1.f, 1.f, 1.f };

	// Waves21cg5 material for SlashPath. HS_Blend_CG uses the Waves21 texture
	// as the flow/distortion map, not as the generic noise slot.
	slashPathMaterial_ = ps::MatSmokeBlendCG{};
	slashPathMaterial_.mainTex      = &gradient3tTex_;
	slashPathMaterial_.flowTex      = &waves21Tex_;
	slashPathMaterial_.maskTex      = &gradient4_3Tex_;
	slashPathMaterial_.flowTexST    = { 1.f, 1.f, 0.f, 0.f };
	slashPathMaterial_.maskTexST    = { 1.f, 1.f, 0.f, 0.f };
	slashPathMaterial_.distortionSpeedXYPowerZ = { 0.f, -0.5f, 0.5f, 0.f };
	slashPathMaterial_.emission     = 2.0f;
	slashPathMaterial_.opacity      = 1.0f;
	slashPathMaterial_.textureOpacity = 0.0f;
	slashPathMaterial_.multiplyTexture = 1.0f;
	slashPathMaterial_.useFresnel   = 0.0f;
	slashPathMaterial_.useCenterGlow = 0.0f;
	slashPathMaterial_.useDepth     = 1.0f;
	slashPathMaterial_.depthPower   = 1.0f;

	// M_VFX_Piercing_Fire — Vefects Piercing slash material.
	// Numeric/color/vector params come from the exported .json; texture slots
	// are bound here. _Distortion_Mask (gradient mask) and _Texture1 (opacity
	// mask) have no DDS yet, so they stay null (shader treats null as white).
	piercingMaterial_ = ps::MatPiercing{};
	if (!loadPiercingMaterialMetadata("../resources/effects/M_VFX_Piercing_Fire.json", piercingMaterial_)) {
		gSharedLog << "[Piercing Material] Warning: ../resources/effects/M_VFX_Piercing_Fire.json 로드 실패. 기본값을 사용합니다.\n";
	}
	piercingMaterial_.colorNoiseTex      = &vfxNoise01Tex_;    // _Color_Noise_Texture
	piercingMaterial_.piercingTex        = &piercingFireTex_;  // _Piercing_Texture
	piercingMaterial_.piercingNoiseTex   = &vfxNoise02Tex_;    // _TextureSample1
	piercingMaterial_.distortionNoiseTex = &vfxNoise02Tex_;    // _Distortion_Noise_Texture
	piercingMaterial_.distortionMaskTex  = &piercingGradientMaskTex_;  // _Distortion_Mask
	piercingMaterial_.emissiveNoiseTex   = &vfxNoise01Tex_;    // _Emissive_Noise_Texture
	piercingMaterial_.emissiveMaskTex    = &piercingFireTex_;  // _Texture0
	piercingMaterial_.opacityMaskTex     = nullptr;            // _Texture1 (null in material)

	// M_VFX_Slash_Fire — Vefects Slash material (SH_VFX_Vefects_Slash_BIRP_New).
	piercingSlashMaterial_ = ps::MatPiercingSlash{};
	if (!loadPiercingSlashMaterialMetadata("../resources/effects/M_VFX_Slash_Fire.json", piercingSlashMaterial_)) {
		gSharedLog << "[PiercingSlash Material] Warning: ../resources/effects/M_VFX_Slash_Fire.json 로드 실패. 기본값을 사용합니다.\n";
	}
	piercingSlashMaterial_.slashTex            = &slashFireTex_;               // _Slash_Texture
	piercingSlashMaterial_.slashNoiseTex       = &vfxNoise02Tex_;              // _Slash_Noise_Texture
	piercingSlashMaterial_.emissiveSlashTex    = &slashFireTex_;               // _Emissive_Slash_Texture
	piercingSlashMaterial_.emissiveDissolveTex = &vfxNoise02Tex_;              // _Emissive_Dissolve_Texture
	piercingSlashMaterial_.distortionNoiseTex  = &vfxNoise02Tex_;              // _Distortion_Noise_Texture
	piercingSlashMaterial_.colorNoiseTex       = &vfxNoise01Tex_;              // _Color_Noise_Texture
	piercingSlashMaterial_.maskTex             = &slashMaskTex_;               // _Mask
	piercingSlashMaterial_.cutoutTex           = &gradientMirrorVerticalTex_;  // _Cutout

	// M_VFX_Slash_Circle_Fire — Vefects Slash material (Slash Circle variant).
	piercingCircleSlashMaterial_ = ps::MatPiercingSlash{};
	if (!loadPiercingSlashMaterialMetadata("../resources/effects/M_VFX_Slash_Circle_Fire.json", piercingCircleSlashMaterial_)) {
		gSharedLog << "[PiercingCircleSlash Material] Warning: ../resources/effects/M_VFX_Slash_Circle_Fire.json 로드 실패. 기본값을 사용합니다.\n";
	}
	piercingCircleSlashMaterial_.slashTex            = &slashGenericTex_;            // _Slash_Texture (Generic)
	piercingCircleSlashMaterial_.slashNoiseTex       = &vfxNoise02Tex_;              // _Slash_Noise_Texture
	piercingCircleSlashMaterial_.emissiveSlashTex    = &slashGenericTex_;            // _Emissive_Slash_Texture
	piercingCircleSlashMaterial_.emissiveDissolveTex = &vfxNoise02Tex_;              // _Emissive_Dissolve_Texture
	piercingCircleSlashMaterial_.distortionNoiseTex  = &vfxNoise02Tex_;              // _Distortion_Noise_Texture
	piercingCircleSlashMaterial_.colorNoiseTex       = &vfxNoise01Tex_;              // _Color_Noise_Texture
	piercingCircleSlashMaterial_.maskTex             = &slashCircleMaskTex_;         // _Mask (Circle Mask)
	piercingCircleSlashMaterial_.cutoutTex           = &gradientMirrorVerticalTex_;  // _Cutout
}

void AssetManager::setupBakedAnimationIds() {
	for (auto& clip : playerAnimations_) {
		clip->id = clip->bakedSamples.idxSrv.idxResource;
	}

	for (auto& clip : goblinAnimations_) {
		clip->id = clip->bakedSamples.idxSrv.idxResource;
	}
	for (auto& clip : snakeAnimations_) {
		clip->id = clip->bakedSamples.idxSrv.idxResource;
	}
	for (auto& clip : mushroomAnimations_) {
		clip->id = clip->bakedSamples.idxSrv.idxResource;
	}
	// Monster skill-caster baked-anim ids.
	for (auto& clip : bomberAnimations_) clip->id = clip->bakedSamples.idxSrv.idxResource;
	for (auto& clip : birdyAnimations_)  clip->id = clip->bakedSamples.idxSrv.idxResource;
	for (auto& clip : slimeAnimations_)  clip->id = clip->bakedSamples.idxSrv.idxResource;
	for (auto& clip : treantAnimations_) clip->id = clip->bakedSamples.idxSrv.idxResource;
	for (auto& clip : bossAnimations_)   clip->id = clip->bakedSamples.idxSrv.idxResource;
}
