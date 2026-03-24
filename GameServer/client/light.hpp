#ifndef __light_HPP
#define __light_HPP

#include "gfx.hpp"

class Camera;

class Light {
public:
	mu::Vec3 color{};
	float intensity = 0.f;
	mu::Vec3 atten{};
	float cosTheta = 0.f;
	float cosPhi = 0.f;
	float falloff = 0.f;
	PBRPipeline::LightData::Type type = PBRPipeline::LightData::Type::PointLight;
	bool isMainDirectionalLight = false;

	void update(Milliseconds deltaTime);
	void MU_CALLCONV updateShadowAuxDirectional( mu::Vec3 pointOfView, float distance,
		float left, float right, float bottom, float top, float nearZ, float farZ	
	);
	void render(GFX& gfx);

	void MU_CALLCONV setPos(mu::Vec3 newPos);
	mu::Vec3 MU_CALLCONV pos() const { return pos_; }
	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat MU_CALLCONV orient() const { return orient_; }
	mu::Mat4x4 MU_CALLCONV shadowView() const { return view_; }
	mu::Mat4x4 MU_CALLCONV shadowProj() const { return proj_; }
	mu::NVec3  MU_CALLCONV dir() const {
		return mu::NVec3(orient_.rotate(mu::Vec3(0.f, 0.f, 1.f)));
	}

private:
	mu::Vec3 pos_{};
	mu::NQuat orient_{};

	mu::Mat4x4 view_{};
	mu::Mat4x4 proj_{};
};

#endif	// __light_HPP