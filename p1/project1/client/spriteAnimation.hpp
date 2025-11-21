#ifndef __spriteAnimation_HPP
#define __spriteAnimation_HPP

#include "pch.hpp"
#include "gfx.hpp"

class SpriteAnimation {
public:
	void setTexture( const std::vector<Texture>& textures );
	void update( Milliseconds deltaTime );
	void render( GFX& gfx );

private:
	mu::Mat4x4 world_{};	// GFX에 행렬을 전달할 때만 사용된다.
	Texture pCurrentTexture_;
	std::vector<Texture> sprites_{};

	float frmaeDuration_ = 1000.f / 24.f; // 24fps
	float frameAcc_ = 0.f;
	std::size_t currentFrameIdx_ = 0;
};

#endif // __spriteAnimation_HPP