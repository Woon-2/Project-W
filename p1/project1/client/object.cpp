#include "object.hpp"

void Object::update(Milliseconds deltaTime) {
	// 쿼터니언 갱신: q' = 0.5 * ω_q * q

	auto wq = mu::Quat(omega_, 0.f);
	auto dq = orient_ * wq * 0.5f;
	orient_ = orient_ + dq * std::chrono::duration_cast<Seconds>(deltaTime).count();

	// 월드변환 행렬 갱신
	world_ = mu::Mat4x4(mu::scale(scale_, scale_, scale_))
		* mu::Mat4x4(orient_) * mu::translate(pos_);
}

void Object::render(GFX& gfx) {
	gfx.addDrawEvent(SamplePipeline::DrawEvent{
		.world = world_,
		.mesh = pMesh_,
		.subMesh = &pMesh_->subMeshes.at(L"CubeMesh_SubMesh")	// 임시 값
	});
}
