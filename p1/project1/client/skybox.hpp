#ifndef __skybox_HPP
#define __skybox_HPP

#include "gfx.hpp"

class SkyboxObject {
public:
	void setModel(const Model* pModel) { pModel_ = pModel; }
	void setSkyboxMaterial(const Skybox* pSkyboxMaterial) { pSkyboxMaterial_ = pSkyboxMaterial; }
	void render(GFX& gfx);

private:
	const Model* pModel_ = nullptr;
	const Skybox* pSkyboxMaterial_ = nullptr;
};

#endif	// __skybox_HPP