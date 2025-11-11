#include "camera.hpp"

void Camera::update() {
	auto pTarget = pTargetObject_.lock();
	if (!pTarget) {
		return;
	}

	auto rotatedOffset = pTarget->orient().rotate(offsetFromTarget_);

	position_ = pTarget->pos() + rotatedOffset;
	view_ = mu::lookAt( position_, pTarget->pos(), mu::NVec3(0.f, 1.f, 0.f));
}

void Camera::updateGFX(GFX& gfx) {
	gfx.addCameraData(PBRPipeline::CameraData{
		.view = view_,
		.proj = proj_
	});
	gfx.addCameraData(BillboardPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos = position_
	});
}

void Camera::setPerspective(mu::Degree fovy, float aspect, float nearz, float farz) {
	proj_ = mu::persp(fovy, aspect, nearz, farz);
	fovy_ = fovy;
	aspect_ = aspect;
	nearz_ = nearz;
	farz_ = farz;
}

void Camera::setOrtho(float minX, float minY, float maxX, float maxY, float minZ, float maxZ) {
	proj_ = mu::ortho(minX, maxX, minY, maxY, minZ, maxZ);
	minX_ = minX;
	maxX_ = maxX;
	minY_ = minY;
	maxY_ = maxY;
	minZ_ = minZ;
	maxZ_ = maxZ;
}