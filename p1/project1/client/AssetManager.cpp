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
	
	gfx.addRequestSkyboxLoad( RequestSkyboxLoad{
		.skyboxPath = "../resources/models/skybox.bin",
		.pDest = &skyboxMaterial_
	} );

	gfx.loadAssets();
}

void AssetManager::loadAnimations() {
	vanguardAnimations_ = loadAnimClipsFromFile("../resources/animations/vanguardAnimations.anim");
}