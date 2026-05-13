#include "pch.hpp"
#include "camera.hpp"
#include "physicsWorld.hpp"

void Camera::update(Milliseconds deltaTime) {
	auto pTarget = pTargetObject_.lock();
	if (!pTarget) {
		return;
	}

	const auto dt = Seconds(deltaTime).count();

	auto rotatedOffsetFromTarget = offsetFromTargetPreRotation_.rotate(offsetFromTarget_);
	rotatedOffsetFromTarget = pTarget->orient().rotate(rotatedOffsetFromTarget);

	auto rotatedTargetPivot = pTarget->orient().rotate(offsetTargetPivot_);
	at_ = pTarget->renderState().pos + rotatedTargetPivot;

	const mu::Vec3 desiredEye = pTarget->renderState().pos + rotatedOffsetFromTarget;

	if (!springInitialized_) {
		springPos_        = desiredEye;
		velocity_         = {};
		currentArmLength_ = (desiredEye - at_).len();
		springInitialized_ = true;
	}

	// Spring evolves toward desiredEye from springPos_, independent of collision-adjusted eye_.
	// Using eye_ here would feed collision corrections back into the spring, causing oscillation.
	const mu::Vec3 displacement = desiredEye - springPos_;
	const mu::Vec3 accel = stiffness_ * displacement - damping_ * velocity_;
	velocity_  += accel * dt;
	springPos_ += velocity_ * dt;

	const float desiredLen = (springPos_ - at_).len();

	const float allowed = (physicsWorld_ && desiredLen > 1e-6f)
		? physicsWorld_->queryCameraArm(at_, springPos_, cameraRadius_)
		: desiredLen;

	// fast-in: snap immediately when blocked; slow-out: lerp back to full distance.
	if (allowed < currentArmLength_)
		currentArmLength_ = allowed;
	else
		currentArmLength_ += std::min(armReturnRate_ * dt, allowed - currentArmLength_);

	// eye_ is rendering-only: applies collision result on top of springPos_, never feeds back into spring.
	if (desiredLen > 1e-6f)
		eye_ = at_ + (springPos_ - at_) * (currentArmLength_ / desiredLen);
	else
		eye_ = springPos_;

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
	gfx.addCameraData(TrailPipeline::CameraData{
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
