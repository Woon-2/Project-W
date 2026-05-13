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
}

void AssetManager::setupBakedAnimationIds() {
	for (auto& clip : playerAnimations_) {
		clip->id = clip->bakedSamples.idxSrv.idxResource;
	}

	for (auto& clip : goblinAnimations_) {
		clip->id = clip->bakedSamples.idxSrv.idxResource;
	}
}
