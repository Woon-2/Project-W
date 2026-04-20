#ifndef __AssetManager_HPP
#define __AssetManager_HPP

#include "gfx.hpp"
#include "animation.hpp"

class AssetManager {
public:
	void loadGFXAssets(GFX& gfx, const AssetConfigs& configs = AssetConfigs{});
	void loadAnimations();

	const Model* modelCube() const { return &modelCube_; }
	const Model* modelPlayer() const { return &modelPlayer_; }
	const Model* modelGoblin() const { return &modelGoblin_; }
	const Skybox* skyboxMaterial() const { return &skyboxMaterial_; }
	const TerrainData* terrain() const { return &terrain_; }
	const Texture* playerHpLine() const { return &playerHpLine_; }
	const Texture* playerHpFrame() const { return &playerHpFrame_; }
	const SpriteAnimationClip* flameAnimation() const { return &flameAnimation_; }
	const SpriteAnimationClip* smokeAnimation() const { return &smokeAnimation_; }
	const Mesh*    swordSlashMesh() const { return &swordSlashMesh_; }
	const Texture* swordSlashTex()  const { return &swordSlashTex_; }
	const std::vector<std::shared_ptr<AnimClip>>& playerAnimations() const { return playerAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& goblinAnimations() const { return goblinAnimations_; }

private:
	std::unordered_map<std::string, Texture> texHashMap_{};
	std::vector<std::shared_ptr<AnimClip>> playerAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> goblinAnimations_{};
	Model modelCube_{};
	Model modelPlayer_{};
	Model modelGoblin_{};
	Skybox skyboxMaterial_{};
	TerrainData terrain_{};
	Texture playerHpLine_{};
	Texture playerHpFrame_{};
	SpriteAnimationClip flameAnimation_{};
	SpriteAnimationClip smokeAnimation_{};
	Mesh    swordSlashMesh_{};
	Texture swordSlashTex_{};
};

#endif	// __AssetManager_HPP