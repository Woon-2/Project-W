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
		.pDest = &billBoard0_
		} );

	gfx.addRequestSpritesLoad( RequestSpritesLoad{
		.spritesPath = "../resources/Sprites/slime_anim.bin",
		.pDest = &slimeSprites_
		} );

	gfx.loadAssets();
}