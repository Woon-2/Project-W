#include "light.hpp"
#include "errorHandling.hpp"

void Light::update(Milliseconds deltaTime) {

}

void Light::render(GFX& gfx) {
	gfx.addLightData(PBRPipeline::LightData{
		.pos = pos(),
		.dir = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f))),
		.color = color,
		.intensity = intensity,
		.cosTheta = cosTheta,
		.cosPhi = cosPhi,
		.falloff = falloff,
		.atten = atten,
		.type = type
	});
}

void MU_CALLCONV Light::setPos(mu::Vec3 newPos) {
	pos_ = newPos;
}

void MU_CALLCONV Light::setOrient(mu::NQuat newOrient) {
	orient_ = newOrient;
}