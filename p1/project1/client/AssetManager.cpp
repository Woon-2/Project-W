#include "AssetManager.hpp"

void AssetManager::loadGFXAssets(GFX& gfx) {
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/cube.bin",
		.pDest = &modelCube_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/vanguard.bin",
		.pDest = &modelPlayer_
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

	gfx.addRequestTextImageLoad( RequestTextImageLoad{
		.width = 512,
		.height = 256,
		.pDest = &textPlayerHp_
		} );

	gfx.loadAssets();
}