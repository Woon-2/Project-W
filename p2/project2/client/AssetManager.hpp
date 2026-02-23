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
	const Model* modelGoblin() const { return &modelGoblin_; }
	const Skybox* skyboxMaterial() const { return &skyboxMaterial_; }
	const Texture* playerHpLine() const { return &playerHpLine_; }
	const Texture* playerHpFrame() const { return &playerHpFrame_; }
	TextImage* textPlayerHp() { return &textPlayerHp_; }
	const std::vector<std::shared_ptr<AnimClip>>& playerAnimations() const { return playerAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& goblinAnimations() const { return goblinAnimations_; }

private:
	std::unordered_map<std::string, Texture> texHashMap_{};
	std::unordered_map<std::string, std::vector<Texture>> spritesHashMap_{};
	std::vector<std::shared_ptr<AnimClip>> playerAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> goblinAnimations_{};
	Model modelCube_{};
	Model modelPlayer_{};
	Model modelGoblin_{};
	Skybox skyboxMaterial_{};
	Texture playerHpLine_{};
	Texture playerHpFrame_{};
	TextImage textPlayerHp_;
};

#endif	// __AssetManager_HPP