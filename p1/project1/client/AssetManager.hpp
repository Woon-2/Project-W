#ifndef __AssetManager_HPP
#define __AssetManager_HPP

#include "gfx.hpp"
#include "animation.hpp"

class AssetManager {
public:
	void loadGFXAssets(GFX& gfx);
	void loadAnimations();

	const Model* modelCube() const { return &modelCube_; }
	const Model* modelPlayer() const { return &modelPlayer_; }
	const Skybox* skyboxMaterial() const { return &skyboxMaterial_; }
	const std::vector<std::shared_ptr<AnimClip>>& vanguardAnimations() const { return vanguardAnimations_; }

private:
	std::unordered_map<std::string, Texture> texHashMap_{};
	std::vector<std::shared_ptr<AnimClip>> vanguardAnimations_{};
	Model modelCube_{};
	Model modelPlayer_{};
	Skybox skyboxMaterial_{};
};

#endif	// __AssetManager_HPP