#ifndef __AssetManager_HPP
#define __AssetManager_HPP

#include "pch.hpp"
#include "gfx.hpp"

class AssetManager {
public:
	void loadGFXAssets(GFX& gfx);
	
	void setBillboard0Tex( const Texture& tex ) { billBoard0_ = tex; }

	const Model* modelCube() const { return &modelCube_; }
	const Model* modelPlayer() const { return &modelPlayer_; }
	const Skybox* skyboxMaterial() const { return &skyboxMaterial_; }
	const Texture* billBoard0() const { return &billBoard0_; }
	const std::vector<Texture>& slimeSprites() const { return slimeSprites_; }

private:
	Model modelCube_;
	Model modelPlayer_;
	Skybox skyboxMaterial_;
	Texture billBoard0_;
	std::vector<Texture> slimeSprites_;
};

#endif	// __AssetManager_HPP