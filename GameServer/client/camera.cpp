#include "pch.hpp"
#include "camera.hpp"
#include "physicsWorld.hpp"

void Camera::update(float dt) {
	auto pTarget = pTargetObject_.lock();
	if (!pTarget) {
		return;
	}

	auto rotatedOffsetFromTarget = offsetFromTargetPreRotation_.rotate(offsetFromTarget_);
	rotatedOffsetFromTarget = pTarget->orient().rotate(rotatedOffsetFromTarget);
	auto rotatedOffsetTargetPivot = xxPreRotation_.rotate(offsetTargetPivot_);
	rotatedOffsetTargetPivot = pTarget->orient().rotate(rotatedOffsetTargetPivot);

	at_ = pTarget->renderState().pos + rotatedOffsetTargetPivot;
	const mu::Vec3 desiredEye = pTarget->renderState().pos + rotatedOffsetFromTarget;
	const float desiredLen = (desiredEye - at_).len();

	if (currentArmLength_ <= 0.f)
		currentArmLength_ = desiredLen;

	const float allowed = (physicsWorld_ && desiredLen > 1e-6f)
		? physicsWorld_->queryCameraArm(at_, desiredEye, cameraRadius_)
		: desiredLen;

	// fast-in: snap immediately when blocked; slow-out: lerp back to full distance.
	if (allowed < currentArmLength_)
		currentArmLength_ = allowed;
	else
		currentArmLength_ += std::min(armReturnRate_ * dt, allowed - currentArmLength_);

	if (desiredLen > 1e-6f)
		eye_ = at_ + (desiredEye - at_) * (currentArmLength_ / desiredLen);
	else
		eye_ = desiredEye;

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
	gfx.addCameraData(BlendCGMeshPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos = eye_
	});
	gfx.addCameraData(SwordSlashPipeline::CameraData{
		.view = view_,
		.proj = proj_
	});
	gfx.addCameraData(TwoSidesPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos  = eye_
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
