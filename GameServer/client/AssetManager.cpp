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
	gfx.addRequestSpritesLoad( RequestSpriteAnimLoad{
		.sheetPath  = "../resources/Sprites/Flame.dds",
		.rows       = 3,
		.cols       = 3,
		.frameCount = 9,
		.type       = SpriteAnimType::Loop,
		.frameTime  = 80ms,
		.pDest      = &flameAnimation_
	} );

	gfx.addRequestSpritesLoad( RequestSpriteAnimLoad{
		.sheetPath  = "../resources/Sprites/A_Smoke_2.dds",
		.rows       = 3,
		.cols       = 3,
		.frameCount = 9,
		.type       = SpriteAnimType::Loop,
		.frameTime  = 80ms,
		.pDest      = &smokeAnimation_
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
}