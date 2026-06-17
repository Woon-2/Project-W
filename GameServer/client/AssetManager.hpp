#ifndef __AssetManager_HPP
#define __AssetManager_HPP

#include "gfx.hpp"
#include "animation.hpp"
#include "swordSlashMaterial.hpp"
#include "piercingMaterial.hpp"
#include "piercingSlashMaterial.hpp"
#include "protocol.hpp"

class AssetManager {
public:
	void loadGFXAssets(GFX& gfx, const AssetConfigs& configs = AssetConfigs{});

	// Dependency-split loading. Phase 1 loads only what the lobby/waiting-room 3D
	// needs (cube, player model, skybox, player animations); Phase 2 loads the
	// remaining in-game assets (goblin, effect meshes/textures, particle materials,
	// goblin animations). loadGFXAssets() runs both in order (used by standalone).
	void loadLobbyVisualAssets(GFX& gfx, const AssetConfigs& configs = AssetConfigs{});
	void loadRemainingInGameAssets(GFX& gfx, const AssetConfigs& configs = AssetConfigs{});

	const Model* modelCube() const { return &modelCube_; }
	const Model* modelPlayer() const { return &modelPlayer_; }
	const Model* modelGoblin() const { return &modelGoblin_; }
	const Skybox* skyboxMaterial() const { return &skyboxMaterial_; }
	const Texture* playerHpLine() const { return &playerHpLine_; }
	const Texture* playerHpFrame() const { return &playerHpFrame_; }
	const Texture* playerHpHeart() const { return &playerHpHeart_; }
	const Texture* katana() const { return &katana_; }
	const Texture* spearHook() const { return &spearHook_; }
	const Texture* crystalWand() const { return &crystalWand_; }
	const Texture* heavyArrow() const { return &heavyArrow_; }
	const Texture* uiLeftButton() const { return &uiLeftButton_; }
	const Texture* playerWeaponIcon(PlayerWeaponType weaponType) const;
	const Texture* digitAtlasTex() const { return &digitAtlasTex_; }
	const Texture* killIconTex()   const { return &killIconTex_; }
	// Skill dial icon resolved by compiled skill asset name (e.g. "SlashWave").
	// Returns nullptr if the asset has no dial icon.
	const Texture* skillIconByAssetName(std::string_view assetName) const;
	const Texture* flameTex() const { return &flameTex_; }
	const Texture* smokeTex() const { return &smokeTex_; }
	const Mesh*    meshSlash3()            const { return &meshSlash3_; }
	const Mesh*    meshHalfTrail()         const { return &meshHalfTrail_; }
	const Mesh*    meshSlashWave()         const { return &meshSlashWave_; }
	const Mesh*    meshIceSpikes2()        const { return &meshIceSpikes2_; }
	const Mesh*    meshArrow()             const { return &meshArrow_; }
	const Texture* arrowTex()              const { return &arrowTex_; }
	const Texture* smoke24Tex()            const { return &smoke24Tex_; }
	const Texture* smoke12Tex()            const { return &smoke12Tex_; }
	const Texture* crater62Tex()           const { return &crater62Tex_; }
	const Texture* noise43bTex()           const { return &noise43bTex_; }
	const Texture* fire13Tex()             const { return &fire13Tex_; }
	const Texture* trail62ForTex()         const { return &trail62ForTex_; }
	const Texture* trail62Tex()            const { return &trail62Tex_; }
	const Texture* trail67Tex()            const { return &trail67Tex_; }
	const Texture* circleTex()             const { return &circleTex_; }
	const Texture* stoneTex()              const { return &stoneTex_; }
	const Texture* gradient3tTex()         const { return &gradient3tTex_; }
	const Texture* gradient4_3Tex()        const { return &gradient4_3Tex_; }
	const Texture* waves21Tex()            const { return &waves21Tex_; }
	const Texture* noise62Tex()            const { return &noise62Tex_; }
	const Texture* noise1Tex()             const { return &noise1Tex_; }
	const Texture* crystalFree1Tex()       const { return &crystalFree1Tex_; }
	const Texture* circle2Tex()            const { return &circle2Tex_; }
	const Texture* slashTex()              const { return &slashTex_; }
	const Texture* smoke26Tex()            const { return &smoke26Tex_; }
	const Texture* flashTex()              const { return &flashTex_; }
	const Texture* arrowHitTex()           const { return &arrowHitTex_; }
	const Texture* arrowMuzzleTex()        const { return &arrowMuzzleTex_; }
	const Texture* EnergyExplosionHitTex() const { return &energyExplosionHitTex_; }
	const Texture* EnergyExplosionHitWhiteBGTex() const { return &energyExplosionHitWhiteBGTex_; }
	const Texture* EnergyExplosionChargeTex() const { return &energyExplosionChargeTex_; }
	const Texture* EnergyExplosionArrowTex() const { return &energyExplosionArrowTex_; }
	const Mesh*    meshRing()              const { return &meshRing_; }
	const Texture* texWindRing()           const { return &texWindRing_; }
	const Texture* texDotParticle()        const { return &texDotParticle_; }
	const Texture* windBulletMuzzleTex()   const { return &texWindBulletMuzzle_; }
	const Texture* windBulletHitTex()      const { return &texWindBulletHit_; }
	const ps::MatSwordSlash& swordSlashMaterial()  const { return swordSlashMaterial_; }
	const ps::MatSwordSlash& swordSlash2Material() const { return swordSlash2Material_; }
	const ps::MatSmokeBlendCG& smokeBlendCGMaterial() const { return smokeBlendCGMaterial_; }
	const ps::MatTwoSides& twoSidesMaterial()      const { return twoSidesMaterial_; }
	const ps::MatTwoSides& spikesMaterial()        const { return spikesMaterial_; }
	const ps::MatSmokeBlendCG& slashPathMaterial() const { return slashPathMaterial_; }
	const ps::MatPiercing& piercingMaterial()      const { return piercingMaterial_; }
	const ps::MatPiercingSlash& piercingSlashMaterial() const { return piercingSlashMaterial_; }
	const ps::MatPiercingSlash& piercingCircleSlashMaterial() const { return piercingCircleSlashMaterial_; }
	const Mesh*    meshVfxProjectile02()   const { return &meshVfxProjectile02_; }
	const Mesh*    meshVfxSlash01HD()      const { return &meshVfxSlash01HD_; }
	const std::vector<std::shared_ptr<AnimClip>>& playerAnimations()   const { return playerAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& goblinAnimations()   const { return goblinAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& snakeAnimations()    const { return snakeAnimations_; }
	const std::vector<std::shared_ptr<AnimClip>>& mushroomAnimations() const { return mushroomAnimations_; }
	const Model* modelSnake()    const { return &modelSnake_; }
	const Model* modelMushroom() const { return &modelMushroom_; }

private:
	void setupBakedAnimationIds();

	std::unordered_map<std::string, Texture> texHashMap_{};
	std::vector<std::shared_ptr<AnimClip>> playerAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> goblinAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> snakeAnimations_{};
	std::vector<std::shared_ptr<AnimClip>> mushroomAnimations_{};
	Model modelCube_{};
	Model modelPlayer_{};
	Model modelGoblin_{};
	Model modelSnake_{};
	Model modelMushroom_{};
	Skybox skyboxMaterial_{};
	Texture playerHpLine_{};
	Texture playerHpFrame_{};
	Texture playerHpHeart_{};
	Texture katana_{};
	Texture spearHook_{};
	Texture crystalWand_{};
	Texture heavyArrow_{};
	Texture uiLeftButton_{};
	Texture digitAtlasTex_{};
	Texture killIconTex_{};
	// Skill dial icons (12), one per selectable weapon skill.
	Texture skillIconSlashWave_{};
	Texture skillIconSlash7_{};
	Texture skillIconSlashCombo_{};
	Texture skillIconArrowVolley_{};
	Texture skillIconArrowRain_{};
	Texture skillIconEnergyExplosionArrow_{};
	Texture skillIconCrystalsFront_{};
	Texture skillIconCrystalsCross_{};
	Texture skillIconRedEnergy_{};
	Texture skillIconPiercingSlash_{};
	Texture skillIconPiercingCircle_{};
	Texture skillIconPiercingMulti_{};
	Texture flameTex_{};
	Texture smokeTex_{};
	Mesh    meshSlash3_{};
	Mesh    meshHalfTrail_{};
	Mesh    meshSlashWave_{};
	Mesh    meshIceSpikes2_{};
	Mesh    meshArrow_{};
	Texture arrowTex_{};
	Texture smoke24Tex_{};
	Texture smoke12Tex_{};
	Texture crater62Tex_{};
	Texture noise43bTex_{};
	Texture fire13Tex_{};
	Texture trail62ForTex_{};
	Texture trail62Tex_{};
	Texture trail67Tex_{};
	Texture circleTex_{};
	Texture stoneTex_{};
	Texture gradient3tTex_{};
	Texture gradient4_3Tex_{};
	Texture waves21Tex_{};
	Texture noise62Tex_{};
	Texture noise1Tex_{};
	Texture crystalFree1Tex_{};
	Texture circle2Tex_{};
	Texture slashTex_{};
	Texture smoke26Tex_{};
	Texture flashTex_{};
	Texture arrowHitTex_{};
	Texture arrowMuzzleTex_{};
	Texture energyExplosionHitTex_{};
	Texture energyExplosionHitWhiteBGTex_{};
	Texture energyExplosionChargeTex_{};
	Texture energyExplosionArrowTex_{};
	Mesh    meshRing_{};
	Texture texWindRing_{};
	Texture texDotParticle_{};
	Texture texWindBulletMuzzle_{};
	Texture texWindBulletHit_{};
	ps::MatSwordSlash   swordSlashMaterial_{};
	ps::MatSwordSlash   swordSlash2Material_{};
	ps::MatSmokeBlendCG smokeBlendCGMaterial_{};
	ps::MatTwoSides     twoSidesMaterial_{};
	ps::MatTwoSides     spikesMaterial_{};
	ps::MatSmokeBlendCG slashPathMaterial_{};
	Texture piercingFireTex_{};
	Texture vfxNoise01Tex_{};
	Texture vfxNoise02Tex_{};
	Texture piercingGradientMaskTex_{};
	Mesh    meshVfxProjectile02_{};
	ps::MatPiercing piercingMaterial_{};
	Texture slashFireTex_{};
	Texture slashMaskTex_{};
	Texture gradientMirrorVerticalTex_{};
	Mesh    meshVfxSlash01HD_{};
	ps::MatPiercingSlash piercingSlashMaterial_{};
	Texture slashGenericTex_{};
	Texture slashCircleMaskTex_{};
	ps::MatPiercingSlash piercingCircleSlashMaterial_{};
};

#endif	// __AssetManager_HPP
