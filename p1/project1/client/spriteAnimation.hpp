#ifndef __spriteAnimation_HPP
#define __spriteAnimation_HPP

#include "gfxUtil.hpp"

class GFX;

class SpriteAnimation {
public:
	void init(const SpriteAnimationClip* pAnimData);
	void update( Milliseconds deltaTime );
	void render( GFX& gfx );
	bool done() const { return done_; }

private:
	mu::Mat4x4 world_{};	// GFX에 행렬을 전달할 때만 사용된다.
	std::normal_distribution<float> distRandomAdvance_{};

	const SpriteAnimationClip* pAnimData_ = nullptr;

	float speed_ = 1.f;
	Milliseconds timeAcc_ = 0ms;
	std::size_t currFrameIdx_ = 0u;
	bool done_ = false;
};

#endif // __spriteAnimation_HPP