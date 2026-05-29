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

void AssetManager::loadGFXAssets(GFX& gfx, const AssetConfigs& configs) {
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

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/goblin/goblin.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelGoblin_
	} );
	
	gfx.addRequestSkyboxLoad( RequestSkyboxLoad{
		.skyboxPath = "../resources/skyboxes/skybox.bin",
		.pDest = &skyboxMaterial_
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

	gfx.addRequestTerrainLoad( RequestTerrainLoad{
		.terrainDir  = "../resources/terrains/",
		.pTexHashMap = &texHashMap_,
		.pDest       = &terrain_
	} );
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

	auto tmpPlayerAnims = loadAnimClipsFromFile("../resources/animations/playerAnimations.anim", gfx);
	playerAnimations_.reserve(tmpPlayerAnims.size());

	auto tmpGoblinAnims = loadAnimClipsFromFile("../resources/animations/goblinAnimations.anim", gfx);
	goblinAnimations_.reserve(tmpGoblinAnims.size());

	gfx.loadAssets(configs);

	for (auto& clip : tmpPlayerAnims) {
		playerAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

	for (auto& clip : tmpGoblinAnims) {
		goblinAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

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
}
