#ifndef __skybox_HPP
#define __skybox_HPP

#include "pch.hpp"
#include "gfx.hpp"

class Skybox {
public:
	void setModel(const Model* pModel) { pModel_ = pModel; }
	void setTexture(const Texture* pTex) { pTexSkybox_ = pTex; }
	void render(GFX& gfx);

private:
	const Model* pModel_ = nullptr;
	const Texture* pTexSkybox_ = nullptr;
};

#endif	// __skybox_HPP