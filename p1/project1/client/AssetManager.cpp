#include "pch.hpp"
#include "AssetManager.hpp"

void AssetManager::loadGFXAssets(GFX& gfx) {
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/cube.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelCube_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/vanguard.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelPlayer_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/rifle.bin",
		.pTexHashMap = &texHashMap_,
		.pDest = &modelRifle_
	} );
	
	gfx.addRequestSkyboxLoad( RequestSkyboxLoad{
		.skyboxPath = "../resources/models/skybox.bin",
		.pDest = &skyboxMaterial_
	} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name = "Billboard0",
		.texturePath = "../client/CubeMesh_Albedo.dds",
		.pDest = &billBoard0_
		} );

	gfx.addRequestSpritesLoad( RequestSpritesLoad{
		.spritesPath = "../resources/Sprites/slime_anim.bin",
		.pDest = &slimeSprites_
		} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name = "PlayerHpLine",
		.texturePath = "../resources/UI/player_hp_line.dds",
		.pDest = &playerHpLine_
		} );

	gfx.addRequestTextureLoad( RequestTextureLoad{
		.name = "PlayerHpFrame",
		.texturePath = "../resources/UI/player_hp_frame.dds",
		.pDest = &playerHpFrame_
		} );

	gfx.loadAssets();
}

void AssetManager::loadAnimations() {
	auto tmp = loadAnimClipsFromFile("../resources/animations/vanguardAnimations.anim");
	vanguardAnimations_.reserve(tmp.size());

	for (auto& clip : tmp) {
		vanguardAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}
}