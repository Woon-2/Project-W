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
	
	gfx.addRequestTextureLoad( RequestTextureLoad{
		.texPath = "../resources/models/textures/skybox.dds",
		.pDest = &texSkybox_
	} );

	gfx.loadAssets();
}