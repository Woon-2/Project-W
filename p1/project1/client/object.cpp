#include "object.hpp"


/*
mu::transpose(mu::Mat4x4() * mu::Mat4x4(mu::rotate(mu::Degree(30.f), mu::NVec3(0.f, 1.f, 0.f)))
				* mu::Mat4x4(mu::rotate(mu::Degree(30.f), mu::NVec3(1.f, 0.f, 0.f))) * mu::translate(mu::Vec3(0.f, -0.2f, 0.6f))
				* mu::Mat4x4(mu::scale(0.25f, 0.25f, 0.25f)))
*/

Object::Object()
	: world_( mu::Mat4x4(mu::scale(0.25f, 0.25f, 0.25f))
		* mu::translate(mu::Vec3(0.f, -0.2f, 0.6f))
		* mu::Mat4x4(mu::rotate(mu::Degree(30.f), mu::NVec3(0.f, 1.f, 0.f, mu::NVec3::NoNormalize_t{})))
		* mu::Mat4x4(mu::rotate(mu::Degree(30.f), mu::NVec3(1.f, 0.f, 0.f, mu::NVec3::NoNormalize_t{})))
	) {}

void Object::update(Milliseconds deltaTime) {
	rotationDegree_ += (deltaTime / 1000ms) * 360.f;
	if (rotationDegree_ >= mu::Degree(360.f)) {
		rotationDegree_ -= 360.f;
	}

	world_ = mu::Mat4x4(mu::scale(0.25f, 0.25f, 0.25f))
		* mu::Mat4x4(mu::rotate(rotationDegree_, mu::NVec3(0.f, 1.f, 0.f, mu::NVec3::NoNormalize_t{})))
		* mu::translate(mu::Vec3(0.f, -0.2f, 0.6f))
		* mu::Mat4x4(mu::rotate(mu::Degree(30.f), mu::NVec3(0.f, 1.f, 0.f, mu::NVec3::NoNormalize_t{})))
		* mu::Mat4x4(mu::rotate(mu::Degree(30.f), mu::NVec3(1.f, 0.f, 0.f, mu::NVec3::NoNormalize_t{})));
}

void Object::render(GFX& gfx) {
	gfx.addDrawEvent(SamplePipeline::DrawEvent{
		.world = world_,
		.mesh = pMesh_,
		.subMesh = &pMesh_->subMeshes.at(L"CubeMesh_SubMesh")	// ÀÓ½Ã °ª
	});
}
