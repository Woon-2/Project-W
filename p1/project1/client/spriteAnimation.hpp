#ifndef __spriteAnimation_HPP
#define __spriteAnimation_HPP

#include "gfxUtil.hpp"

class GFX;

class SpriteAnimation {
public:
	void init(const SpriteAnimationClip* pAnimData);
	void update( Milliseconds deltaTime );
	void render( GFX& gfx ) const;
	bool done() const { return done_; }

	void setSpeed(float speed) { speed_ = speed; }
	float speed() const { return speed_; }

	mu::Vec3 MU_CALLCONV pos() const { return pos_; }
	void MU_CALLCONV setPos(mu::Vec3 pos) { pos_ = pos; }

	mu::Vec2 MU_CALLCONV scale() const { return scale_; }
	void MU_CALLCONV setScale(mu::Vec2 scale) { scale_ = scale; }

	mu::Vec3 MU_CALLCONV tint() const { return tint_; }
	void MU_CALLCONV setTint(mu::Vec3 tint) { tint_ = tint; }

private:
	mu::Mat4x4 world_{};	// GFX에 행렬을 전달할 때만 사용된다.
	std::normal_distribution<float> distRandomAdvance_{};

	mu::Vec3 pos_{};
	mu::Vec3 tint_ = mu::Vec3(1.f, 1.f, 1.f);
	mu::Vec2 scale_ = mu::Vec2(1.f, 1.f);	// 향후 셰이더에서 반영할 수 있도록

	const SpriteAnimationClip* pAnimData_ = nullptr;

	float speed_ = 1.f;
	Milliseconds timeAcc_ = 0ms;
	std::size_t currFrameIdx_ = 0u;
	bool done_ = false;
};

#endif // __spriteAnimation_HPP