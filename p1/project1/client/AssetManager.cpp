#include "AssetManager.hpp"

void AssetManager::loadModels(GFX& gfx) {
	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/cube.bin",
		.pDest = &modelCube_
	} );

	gfx.addRequestModelLoad( RequestModelLoad{
		.modelPath = "../resources/models/vanguard.bin",
		.pDest = &modelPlayer_
	} );

	gfx.loadModels();
}