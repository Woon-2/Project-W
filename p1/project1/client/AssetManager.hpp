#ifndef __AssetManager_HPP
#define __AssetManager_HPP

#include "gfx.hpp"

class AssetManager {
public:
	void loadGFXAssets(GFX& gfx);

	const Model* modelCube() const { return &modelCube_; }
	const Model* modelPlayer() const { return &modelPlayer_; }
	const Skybox* skyboxMaterial() const { return &skyboxMaterial_; }

private:
	std::unordered_map<std::string, Texture> texHashMap_{};
	Model modelCube_{};
	Model modelPlayer_{};
	Skybox skyboxMaterial_{};
};

#endif	// __AssetManager_HPP