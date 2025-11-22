#ifndef __assetManager_HPP
#define __assetManager_HPP

#include "model.hpp"
#include "level.hpp"

class AssetManager {
public:
	void loadAssets();

	const Model* modelCube() const { return &modelCube_; }
	const Model* modelPlayer() const { return &modelPlayer_; }

	const Level* level() const { return &level_; }

private:
	Model modelCube_{};
	Model modelPlayer_{};

	Level level_{};
};

#endif	// __assetManager_HPP