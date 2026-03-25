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
	const Model* modelAnubis() const { return &modelAnubis_; }
	const Model* modelBat() const { return &modelBat_; }
	const Model* modelBomber() const { return &modelBomber_; }
	const Model* modelDemon() const { return &modelDemon_; }
	const Model* modelDragon() const { return &modelDragon_; }
	const Model* modelEyeball() const { return &modelEyeball_; }
	const Model* modelFishman() const { return &modelFishman_; }
	const Model* modelGargoyle() const { return &modelGargoyle_; }
	const Skybox* skyboxMaterial() const { return &skyboxMaterial_; }
	const Texture* playerHpLine() const { return &playerHpLine_; }
	const Texture* playerHpFrame() const { return &playerHpFrame_; }
	TextImage* textPlayerHp() { return &textPlayerHp_; }
	const SpriteAnimationClip* flameAnimation() const { return &flameAnimation_; }
	const std::vector<std::shared_ptr<AnimClip>>& playerAnimations() const { return playerAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& goblinAnimations() const { return goblinAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& anubisAnimations() const { return anubisAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& batAnimations() const { return batAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& bomberAnimations() const { return bomberAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& demonAnimations() const { return demonAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& dragonAnimations() const { return dragonAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& eyeballAnimations() const { return eyeballAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& fishmanAnimations() const { return fishmanAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& gargoyleAnimations() const { return gargoyleAnimations_; }

private:
	std::unordered_map<std::string, Texture> texHashMap_{};
	std::vector<std::shared_ptr<AnimClip>> playerAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> goblinAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> anubisAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> batAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> bomberAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> demonAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> dragonAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> eyeballAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> fishmanAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> gargoyleAnimations_{};
	Model modelCube_{};
	Model modelPlayer_{};
	Model modelGoblin_{};
	Model modelAnubis_{};
	Model modelBat_{};
	Model modelBomber_{};
	Model modelDemon_{};
	Model modelDragon_{};
	Model modelEyeball_{};
	Model modelFishman_{};
	Model modelGargoyle_{};
	Skybox skyboxMaterial_{};
	Texture playerHpLine_{};
	Texture playerHpFrame_{};
	TextImage textPlayerHp_;
	SpriteAnimationClip flameAnimation_{};
};

#endif	// __AssetManager_HPP