#ifndef __light_HPP
#define __light_HPP

#include "pch.hpp"
#include "gfx.hpp"
#include "physics.hpp"

class Light {
public:
	mu::Vec3 color{};
	float intensity = 0.f;
	mu::Vec3 atten{};
	float cosTheta = 0.f;
	float cosPhi = 0.f;
	float falloff = 0.f;
	PBRPipeline::LightData::Type type;

	void update(Milliseconds deltaTime);
	void render(GFX& gfx);

	void MU_CALLCONV setPos(mu::Vec3 newPos);
	mu::Vec3 MU_CALLCONV pos() const { return pos_; }
	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat MU_CALLCONV orient() const { return orient_; }

private:
	mu::Vec3 pos_;
	mu::NQuat orient_;
};

#endif	// __light_HPP