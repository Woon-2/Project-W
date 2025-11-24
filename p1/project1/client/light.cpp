#include "pch.hpp"
#include "light.hpp"
#include "errorHandling.hpp"
#include "camera.hpp"

void Light::update(Milliseconds deltaTime) {

}

void MU_CALLCONV Light::updateShadowAuxDirectional( mu::Vec3 pointOfView, float distance,
	float left, float right, float bottom, float top, float nearZ, float farZ	
) {
	const auto dir = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f)));
	pos_ = pointOfView - mu::Vec3(dir) * distance;
	view_ = mu::lookAt(pos_, pointOfView, mu::NVec3(0.f, 1.f, 0.f, mu::NVec3::NoNormalize_t{}));
	proj_ = mu::ortho(left, right, bottom, top, nearZ, farZ);
}

void Light::render(GFX& gfx) {
	gfx.addLightData(PBRPipeline::LightData{
		.pos = pos_,
		.dir = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f))),
		.color = color,
		.intensity = intensity,
		.cosTheta = cosTheta,
		.cosPhi = cosPhi,
		.falloff = falloff,
		.atten = atten,
		.type = type,
		.isMainDirectionalLight = isMainDirectionalLight,
		.view = view_,
		.proj = proj_
	});
}

void MU_CALLCONV Light::setPos(mu::Vec3 newPos) {
	pos_ = newPos;
}

void MU_CALLCONV Light::setOrient(mu::NQuat newOrient) {
	orient_ = newOrient;
}