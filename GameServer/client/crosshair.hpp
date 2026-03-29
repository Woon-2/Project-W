#ifndef __crosshair_HPP
#define __crosshair_HPP

#include "gfx.hpp"
#include "event.hpp"

class Crosshair {
public:
	void setTexture( const Texture* pTex ) { pTex_ = pTex; }
	void MU_CALLCONV setPivot(mu::Vec2 pivot) { pivot_ = pivot; }
	void MU_CALLCONV setScale(mu::Vec2 scale) { scale_ = scale; }
	void update( Milliseconds deltaTime );
	void render( GFX& gfx );

private:
	mu::Mat4x4 world_{};

	mu::Vec2 pivot_{};
	mu::Vec2 scale_{};

	const Texture* pTex_ = nullptr;
};

#endif 