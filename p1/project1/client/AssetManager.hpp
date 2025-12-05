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
	const std::vector<Texture>& slimeSprites() const { return slimeSprites_; }
	const Texture* playerHpLine() const { return &playerHpLine_; }
	const Texture* playerHpFrame() const { return &playerHpFrame_; }
	const std::vector<std::shared_ptr<AnimClip>>& vanguardAnimations() const { return vanguardAnimations_; }

private:
	std::unordered_map<std::string, Texture> texHashMap_{};
	std::vector<std::shared_ptr<AnimClip>> vanguardAnimations_{};
	Model modelCube_{};
	Model modelPlayer_{};
	Model modelRifle_{};
	Skybox skyboxMaterial_{};
	Texture billBoard0_;
	std::vector<Texture> slimeSprites_;
	Texture playerHpLine_;
	Texture playerHpFrame_;
};

#endif	// __AssetManager_HPP