#ifndef __AssetManager_HPP
#define __AssetManager_HPP

#include "pch.hpp"
#include "gfx.hpp"

class AssetManager {
public:
	void loadGFXAssets(GFX& gfx);

	const Model* modelCube() const { return &modelCube_; }
	const Model* modelPlayer() const { return &modelPlayer_; }

private:
	Model modelCube_;
	Model modelPlayer_;
	Texture texSkybox_;
};

#endif	// __AssetManager_HPP