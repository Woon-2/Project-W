#include "pch.hpp"
#include "AssetManager.hpp"

void AssetManager::loadGFXAssets(GFX& gfx) {
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

	gfx.addRequestTextImageLoad( RequestTextImageLoad{
		.width = 512,
		.height = 256,
		.pDest = &textPlayerHp_
	} );

	gfx.loadAssets();
}

void AssetManager::loadAnimations() {
	auto tmp = loadAnimClipsFromFile("../resources/animations/playerAnimations.anim");
	playerAnimations_.reserve(tmp.size());

	for (auto& clip : tmp) {
		playerAnimations_.push_back( std::make_shared<AnimClip>(std::move(clip)) );
	}
}