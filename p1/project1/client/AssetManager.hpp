#ifndef __AssetManager_HPP
#define __AssetManager_HPP

#include "gfx.hpp"
#include "animation.hpp"

class AssetManager {
public:
	void loadGFXAssets(GFX& gfx);
	void loadAnimations();

	//void setBillboard0Tex( const Texture& tex ) { billBoard0_ = tex; }

	const Model* modelCube() const { return &modelCube_; }
	const Model* modelPlayer() const { return &modelPlayer_; }
	const Model* modelRifle() const { return &modelRifle_; }
	const Skybox* skyboxMaterial() const { return &skyboxMaterial_; }
	const Texture* billBoard0() const { return &billBoard0_; }
	const SpriteAnimationClip* slimeAnimation() const { return &slimeAnimation_; }
	const SpriteAnimationClip* muzzleFlashAnimation() const { return &muzzleFlashAnimation_; }
	const Texture* playerHpLine() const { return &playerHpLine_; }
	const Texture* playerHpFrame() const { return &playerHpFrame_; }
	const std::vector<std::shared_ptr<AnimClip>>& vanguardAnimations() const { return vanguardAnimations_; }

private:
	std::unordered_map<std::string, Texture> texHashMap_{};
	std::unordered_map<std::string, std::vector<Texture>> spritesHashMap_{};
	std::vector<std::shared_ptr<AnimClip>> vanguardAnimations_{};
	Model modelCube_{};
	Model modelPlayer_{};
	Model modelRifle_{};
	Skybox skyboxMaterial_{};
	Texture billBoard0_{};
	Texture playerHpLine_{};
	Texture playerHpFrame_{};
	SpriteAnimationClip slimeAnimation_{};
	SpriteAnimationClip muzzleFlashAnimation_{};
};

#endif	// __AssetManager_HPP