#ifndef __AssetManager_HPP
#define __AssetManager_HPP

#include "gfx.hpp"
#include "animation.hpp"
#include "swordSlashMaterial.hpp"

class AssetManager {
public:
	void loadGFXAssets(GFX& gfx, const AssetConfigs& configs = AssetConfigs{});

	const Model* modelCube() const { return &modelCube_; }
	const Model* modelPlayer() const { return &modelPlayer_; }
	const Model* modelGoblin() const { return &modelGoblin_; }
	const Skybox* skyboxMaterial() const { return &skyboxMaterial_; }
	const TerrainData* terrain() const { return &terrain_; }
	const Texture* playerHpLine() const { return &playerHpLine_; }
	const Texture* playerHpFrame() const { return &playerHpFrame_; }
	const Texture* flameTex() const { return &flameTex_; }
	const Texture* smokeTex() const { return &smokeTex_; }
	const Mesh*    meshSlash3()            const { return &meshSlash3_; }
	const Mesh*    meshHalfTrail()         const { return &meshHalfTrail_; }
	const Mesh*    meshSlashWave()         const { return &meshSlashWave_; }
	const Mesh*    meshIceSpikes2()        const { return &meshIceSpikes2_; }
	const Texture* smoke24Tex()            const { return &smoke24Tex_; }
	const Texture* smoke12Tex()            const { return &smoke12Tex_; }
	const Texture* crater62Tex()           const { return &crater62Tex_; }
	const Texture* noise43bTex()           const { return &noise43bTex_; }
	const Texture* fire13Tex()             const { return &fire13Tex_; }
	const Texture* trail62ForTex()         const { return &trail62ForTex_; }
	const Texture* trail62Tex()            const { return &trail62Tex_; }
	const Texture* gradient3tTex()         const { return &gradient3tTex_; }
	const Texture* gradient4_3Tex()        const { return &gradient4_3Tex_; }
	const Texture* waves21Tex()            const { return &waves21Tex_; }
	const Texture* noise62Tex()            const { return &noise62Tex_; }
	const Texture* noise1Tex()             const { return &noise1Tex_; }
	const Texture* crystalFree1Tex()       const { return &crystalFree1Tex_; }
	const Texture* circle2Tex()            const { return &circle2Tex_; }
	const Texture* slashTex()              const { return &slashTex_; }
	const ps::MatSwordSlash& swordSlashMaterial()  const { return swordSlashMaterial_; }
	const ps::MatSwordSlash& swordSlash2Material() const { return swordSlash2Material_; }
	const ps::MatSmokeBlendCG& smokeBlendCGMaterial() const { return smokeBlendCGMaterial_; }
	const ps::MatTwoSides& twoSidesMaterial()      const { return twoSidesMaterial_; }
	const ps::MatTwoSides& spikesMaterial()        const { return spikesMaterial_; }
	const ps::MatSmokeBlendCG& slashPathMaterial() const { return slashPathMaterial_; }
	const std::vector<std::shared_ptr<AnimClip>>& playerAnimations() const { return playerAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& goblinAnimations() const { return goblinAnimations_; }

private:
	void setupBakedAnimationIds();

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
	Texture flameTex_{};
	Texture smokeTex_{};
	Mesh    meshSlash3_{};
	Mesh    meshHalfTrail_{};
	Mesh    meshSlashWave_{};
	Mesh    meshIceSpikes2_{};
	Texture smoke24Tex_{};
	Texture smoke12Tex_{};
	Texture crater62Tex_{};
	Texture noise43bTex_{};
	Texture fire13Tex_{};
	Texture trail62ForTex_{};
	Texture trail62Tex_{};
	Texture gradient3tTex_{};
	Texture gradient4_3Tex_{};
	Texture waves21Tex_{};
	Texture noise62Tex_{};
	Texture noise1Tex_{};
	Texture crystalFree1Tex_{};
	Texture circle2Tex_{};
	Texture slashTex_{};
	ps::MatSwordSlash   swordSlashMaterial_{};
	ps::MatSwordSlash   swordSlash2Material_{};
	ps::MatSmokeBlendCG smokeBlendCGMaterial_{};
	ps::MatTwoSides     twoSidesMaterial_{};
	ps::MatTwoSides     spikesMaterial_{};
	ps::MatSmokeBlendCG slashPathMaterial_{};
};

#endif	// __AssetManager_HPP
