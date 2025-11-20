#ifndef __spriteAnimation_HPP
#define __spriteAnimation_HPP

#include "pch.hpp"
#include "gfx.hpp"

class SpriteAnimation {
public:
	void seTexture( const Texture& pTex ) { pCurrentTexture_ = cloneTextureIdxOnly( pTex ); }
	void update( Milliseconds deltaTime );
	void render( GFX& gfx );

private:
	mu::Mat4x4 world_{};	// GFX에 행렬을 전달할 때만 사용된다.
	Texture pCurrentTexture_;

	float frmaeDuration_ = 1000.f / 24.f; // 24fps
	float frameAcc_ = 0.f;
};

#endif // __spriteAnimation_HPP