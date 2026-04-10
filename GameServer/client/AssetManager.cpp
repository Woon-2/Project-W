#include "pch.hpp"
#include "AssetManager.hpp"

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

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/anubis/anubis.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelAnubis_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/bat/bat.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelBat_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/bomber/bomber.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelBomber_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/demon/demon.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelDemon_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/dragon/dragon.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelDragon_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/eyeball/eyeball.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelEyeball_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/fishman/fishman.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelFishman_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/gargoyle/gargoyle.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelGargoyle_
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
		.meshPath    = "../resources/effects/SwordSlash.meshbin",
		.pTexHashMap = &texHashMap_,
		.pDestMesh   = &swordSlashMesh_,
		.pDestTex    = &swordSlashTex_
	} );

	gfx.loadAssets(configs);
}

void AssetManager::loadAnimations() {
	auto tmpPlayerAnims = loadAnimClipsFromFile("../resources/animations/playerAnimations.anim");
	playerAnimations_.reserve(tmpPlayerAnims.size());

	for (auto& clip : tmpPlayerAnims) {
		playerAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

	auto tmpGoblinAnims = loadAnimClipsFromFile("../resources/animations/goblinAnimations.anim");
	goblinAnimations_.reserve(tmpGoblinAnims.size());

	for (auto& clip : tmpGoblinAnims) {
		goblinAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

	auto tmpAnubisAnims = loadAnimClipsFromFile("../resources/animations/anubisAnimations.anim");
	anubisAnimations_.reserve(tmpAnubisAnims.size());

	for (auto& clip : tmpAnubisAnims) {
		anubisAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

	auto tmpBatAnims = loadAnimClipsFromFile("../resources/animations/batAnimations.anim");
	batAnimations_.reserve(tmpBatAnims.size());

	for (auto& clip : tmpBatAnims) {
		batAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

	auto tmpBomberAnims = loadAnimClipsFromFile("../resources/animations/bomberAnimations.anim");
	bomberAnimations_.reserve(tmpBomberAnims.size());

	for (auto& clip : tmpBomberAnims) {
		bomberAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

	auto tmpDemonAnims = loadAnimClipsFromFile("../resources/animations/demonAnimations.anim");
	demonAnimations_.reserve(tmpDemonAnims.size());

	for (auto& clip : tmpDemonAnims) {
		demonAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

	auto tmpDragonAnims = loadAnimClipsFromFile("../resources/animations/dragonAnimations.anim");
	dragonAnimations_.reserve(tmpDragonAnims.size());

	for (auto& clip : tmpDragonAnims) {
		dragonAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

	auto tmpEyeballAnims = loadAnimClipsFromFile("../resources/animations/eyeballAnimations.anim");
	eyeballAnimations_.reserve(tmpEyeballAnims.size());

	for (auto& clip : tmpEyeballAnims) {
		eyeballAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

	auto tmpFishmanAnims = loadAnimClipsFromFile("../resources/animations/fishmanAnimations.anim");
	fishmanAnimations_.reserve(tmpFishmanAnims.size());

	for (auto& clip : tmpFishmanAnims) {
		fishmanAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}

	auto tmpGargoyleAnims = loadAnimClipsFromFile("../resources/animations/gargoyleAnimations.anim");
	gargoyleAnimations_.reserve(tmpFishmanAnims.size());

	for (auto& clip : tmpGargoyleAnims) {
		gargoyleAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}
}