#include "pch.hpp"
#include "camera.hpp"
#include "physicsWorld.hpp"

void Camera::update(Milliseconds deltaTime) {
	auto pTarget = pTargetObject_.lock();
	if (!pTarget) {
		return;
	}

	const auto dt = Seconds(deltaTime).count();
	const auto smoothingAlpha = [](float rate, float deltaSeconds) {
		if (rate <= 0.f) {
			return 1.f;
		}
		return 1.f - std::exp(-rate * deltaSeconds);
	};

	const mu::Vec3 targetForward = pTarget->orient().rotate(mu::Vec3(0.f, 0.f, 1.f));
	const float targetForwardHLen2 = targetForward.x() * targetForward.x() + targetForward.z() * targetForward.z();
	const mu::Radian targetYaw = targetForwardHLen2 > 1e-8f
		? mu::Radian(std::atan2(targetForward.x(), targetForward.z()))
		: mu::Radian(0.f);
	const mu::Radian targetPitch = mu::Radian(std::atan2(
		targetForward.y(),
		std::sqrt(targetForwardHLen2)
	));
	if (!followFilterInitialized_) {
		filteredTargetPitch_ = targetPitch;
		followFilterInitialized_ = true;
	}
	else {
		const float pitchAlpha = smoothingAlpha(targetPitchSmoothingRate_, dt);
		filteredTargetPitch_ = mu::Radian(
			static_cast<float>(filteredTargetPitch_)
			+ (static_cast<float>(targetPitch) - static_cast<float>(filteredTargetPitch_)) * pitchAlpha
		);
	}

	const mu::NQuat filteredTargetRotation(mu::Radian(0.f), filteredTargetPitch_, targetYaw);

	auto rotatedOffsetFromTarget = offsetFromTargetPreRotation_.rotate(offsetFromTarget_);
	rotatedOffsetFromTarget = filteredTargetRotation.rotate(rotatedOffsetFromTarget);

	auto rotatedTargetPivot = filteredTargetRotation.rotate(offsetTargetPivot_);
	const mu::Vec3 desiredAt = pTarget->renderState().pos + rotatedTargetPivot;
	if (!springInitialized_) {
		at_ = desiredAt;
	}
	else {
		at_ += (desiredAt - at_) * smoothingAlpha(atSmoothingRate_, dt);
	}

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

	// 카메라 암이 0으로 붕괴(예: 스폰 직후 피벗이 지면 아래)하거나 타깃 orient가 degenerate한 경우
	// eye_ == at_ 가 되어 lookAt 시선 방향이 0이 된다. lookAt이 degenerate되지 않도록 최소 거리를 보장한다.
	if ((eye_ - at_).len2() < 1e-4f) {
		mu::Vec3 dir = springPos_ - at_;
		float dl = dir.len();
		if (dl < 1e-6f) { dir = mu::Vec3(0.f, 0.f, -1.f); dl = 1.f; }
		eye_ = at_ + dir * (0.1f / dl);
	}

	view_ = mu::lookAt(eye_, at_, mu::NVec3(0.f, 1.f, 0.f));
}

void MU_CALLCONV Camera::setView(mu::Vec3 eye, mu::Vec3 at) {
	eye_ = eye;
	at_  = at;
	followFilterInitialized_ = false;
	view_ = mu::lookAt(eye, at, mu::NVec3(0.f, 1.f, 0.f));
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
	gfx.addCameraData(WindRingPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos  = eye_
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
	gfx.addCameraData(PiercingMeshPipeline::CameraData{
		.view = view_,
		.proj = proj_,
		.pos = eye_
	});
	gfx.addCameraData(PiercingSlashMeshPipeline::CameraData{
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
	proj_ = mu::perspReversedZ(fovy, aspect, nearz, farz);
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
