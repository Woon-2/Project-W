#include "pch.hpp"
#include "assetManager.hpp"

void AssetManager::loadAssets() {
	modelCube_ = loadModelFromFile("../resources/models/cubeServer.bin");
	modelPlayer_ = loadModelFromFile("../resources/models/vanguardServer.bin");

	level_ = loadLevelFromFile("../resources/levels/level.bin", *this);
}
