#include "pch.hpp"
#include "camera.hpp"

void Camera::update() {
	auto pTarget = pTargetObject_.lock();
	if (!pTarget) {
		return;
	}

	// 카메라 자체의 회전 적용
	auto rotatedOffsetFromTarget = offsetFromTargetPreRotation_.rotate(offsetFromTarget_);
	// 타겟 오브젝트의 회전 적용
	rotatedOffsetFromTarget = pTarget->orient().rotate(rotatedOffsetFromTarget);
	// 타겟 오브젝트에 대한 카메라의 초점 오프셋 적용
	auto rotatedOffsetTargetPivot = xxPreRotation_.rotate(offsetTargetPivot_);
	rotatedOffsetTargetPivot = pTarget->orient().rotate(rotatedOffsetTargetPivot);

	eye_ = pTarget->renderState().pos + rotatedOffsetFromTarget;
	at_ = pTarget->renderState().pos + rotatedOffsetTargetPivot;

	view_ = mu::lookAt(eye_, at_, mu::NVec3(0.f, 1.f, 0.f));
}

void Camera::updateGFX(GFX& gfx) {
	gfx.addCameraData(PBRPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos = eye_
	});
	gfx.addCameraData(PBRSkinnedPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos = eye_
	});
	gfx.addCameraData(PBRDeferredPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos = eye_
	});
	gfx.addCameraData(PBRDeferredSkinnedPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos = eye_
	});
	gfx.addCameraData(SkyboxPipeline::CameraData{
		.view = view_,
		.proj = proj_
	});
	gfx.addCameraData(BVPipeline::CameraData{
		.view = view_,
		.proj = proj_
	});
	gfx.addCameraData(BillboardPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos = eye_
	});
	gfx.addCameraData( TerrainPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos = eye_
		} );
	gfx.addCameraData(TerrainDeferredPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos = eye_
	});

	gfx.addCameraData(MeshParticlePipeline::CameraData{
		.view = view_,
		.proj = proj_
	});
	gfx.addCameraData(SmokeBlendCGPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos = eye_
	});
	gfx.addCameraData(SwordSlashPipeline::CameraData{
		.view = view_,
		.proj = proj_
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
