#include "rspch.hpp"
#include "AssetManager.hpp"

void AssetManager::loadAssets() {
	modelCube_ = loadModelFromFile("../resources/models/cube/cubeServer.bin");
	modelPlayer_ = loadModelFromFile("../resources/models/player/playerServer.bin");

	level_ = loadLevelFromFile("../resources/levels/level.bin", *this);
}
