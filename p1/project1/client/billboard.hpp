#ifndef __billboard_HPP
#define __billboard_HPP

#include "pch.hpp"
#include "gfx.hpp"

class Billboard {
public:
	void setTexture( const Texture* pTex ) { pTex_ = pTex; }
	void update( Milliseconds deltaTime );
	void render( GFX& gfx );

private:
	mu::Mat4x4 world_{};	// GFX에 행렬을 전달할 때만 사용된다.
	const Texture* pTex_ = nullptr;
};


#endif	// __billboard_HPP