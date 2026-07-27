#include "pch.hpp"
#include "camera.hpp"
#include "physicsWorld.hpp"

void Camera::update(Milliseconds deltaTime) {
	// updateFocusCinematic()은 렌더용 eye/at/view만 덮어쓴다. 다음 follow 계산은
	// 연출이 적용되기 전의 일반 카메라 상태에서 이어져야 복귀 시 튀지 않는다.
	if (focusCinematicActive_ && baseViewInitialized_) {
		eye_  = baseEye_;
		at_   = baseAt_;
		view_ = baseView_;
		proj_ = baseProj_;
	}

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
	baseEye_  = eye_;
	baseAt_   = at_;
	baseView_ = view_;
	baseViewInitialized_ = true;
}

void MU_CALLCONV Camera::setView(mu::Vec3 eye, mu::Vec3 at) {
	eye_ = eye;
	at_  = at;
	followFilterInitialized_ = false;
	view_ = mu::lookAt(eye, at, mu::NVec3(0.f, 1.f, 0.f));
	baseEye_  = eye_;
	baseAt_   = at_;
	baseView_ = view_;
	baseViewInitialized_ = true;
}

void Camera::playFocusCinematic(
	const std::shared_ptr<Object>& target,
	const FocusCinematicConfig& config) {
	if (!target || focusCinematicActive_) return;

	focusCinematicTarget_ = target;
	focusCinematicConfig_ = config;
	focusCinematicElapsed_ = Milliseconds{ 0.f };
	focusCinematicPoint_ = target->renderState().pos
		+ mu::Vec3{ 0.f, config.focusHeight, 0.f };

	// 현재 카메라가 바라보던 쪽에서 보스에게 다가간다. 보스의 사망 회전에
	// 종속되지 않으므로 래그돌 전환 순간에도 샷 방향이 뒤집히지 않는다.
	mu::Vec3 shotDir{
		eye_.x() - focusCinematicPoint_.x(),
		0.f,
		eye_.z() - focusCinematicPoint_.z()
	};
	const float shotDirLen = shotDir.len();
	if (shotDirLen > 1e-4f) {
		focusCinematicShotDir_ = shotDir * (1.f / shotDirLen);
	}
	else {
		const mu::Vec3 backward = target->orient().rotate(mu::Vec3{ 0.f, 0.f, -1.f });
		const float backwardLen = std::sqrt(
			backward.x() * backward.x() + backward.z() * backward.z());
		focusCinematicShotDir_ = backwardLen > 1e-4f
			? mu::Vec3{ backward.x() / backwardLen, 0.f, backward.z() / backwardLen }
			: mu::Vec3{ 0.f, 0.f, -1.f };
	}

	if (!baseViewInitialized_) {
		baseEye_  = eye_;
		baseAt_   = at_;
		baseView_ = view_;
		baseViewInitialized_ = true;
	}
	focusCinematicActive_ = true;
}

void Camera::cancelFocusCinematic() {
	focusCinematicActive_ = false;
	focusCinematicTarget_.reset();
	focusCinematicElapsed_ = Milliseconds{ 0.f };
	if (baseViewInitialized_) {
		eye_  = baseEye_;
		at_   = baseAt_;
		view_ = baseView_;
		proj_ = baseProj_;
	}
}

float Camera::focusCinematicTimeScale() const {
	if (!focusCinematicActive_) return 1.f;

	const float durationMs = std::max(1.f, focusCinematicConfig_.duration.count());
	const float blendOutMs = std::clamp(
		focusCinematicConfig_.blendOut.count(), 0.f, durationMs);
	const float recoverStartMs = durationMs - blendOutMs;
	const float elapsedMs = focusCinematicElapsed_.count();
	const float minScale = std::clamp(
		focusCinematicConfig_.slowMotionScale, 0.01f, 1.f);

	if (blendOutMs <= 0.f || elapsedMs <= recoverStartMs) return minScale;

	float t = std::clamp((elapsedMs - recoverStartMs) / blendOutMs, 0.f, 1.f);
	t = t * t * (3.f - 2.f * t);
	return minScale + (1.f - minScale) * t;
}

void Camera::updateFocusCinematic(Milliseconds realDeltaTime) {
	if (!focusCinematicActive_) return;

	const float durationMs = std::max(1.f, focusCinematicConfig_.duration.count());
	focusCinematicElapsed_ += std::max(realDeltaTime, Milliseconds{ 0.f });
	const float elapsedMs = focusCinematicElapsed_.count();

	if (auto target = focusCinematicTarget_.lock()) {
		focusCinematicPoint_ = target->renderState().pos
			+ mu::Vec3{ 0.f, focusCinematicConfig_.focusHeight, 0.f };
	}

	const auto smoothStep = [](float t) {
		t = std::clamp(t, 0.f, 1.f);
		return t * t * (3.f - 2.f * t);
	};
	const float blendInMs = std::max(1.f, focusCinematicConfig_.blendIn.count());
	const float blendOutMs = std::max(1.f, focusCinematicConfig_.blendOut.count());
	const float blendIn = smoothStep(elapsedMs / blendInMs);
	const float blendOut = smoothStep((durationMs - elapsedMs) / blendOutMs);
	const float weight = std::min(blendIn, blendOut);

	const mu::Vec3 shotEye = focusCinematicPoint_
		+ focusCinematicShotDir_ * focusCinematicConfig_.shotDistance
		+ mu::Vec3{ 0.f, focusCinematicConfig_.shotHeight, 0.f };
	eye_ = mu::lerp(baseEye_, shotEye, weight);
	at_  = mu::lerp(baseAt_, focusCinematicPoint_, weight);
	if ((eye_ - at_).len2() < 1e-4f)
		eye_ += focusCinematicShotDir_ * 0.1f;
	view_ = mu::lookAt(eye_, at_, mu::NVec3(0.f, 1.f, 0.f));

	if (perspectiveProjection_) {
		const float baseFovy = static_cast<float>(fovy_);
		const float focusFovy = static_cast<float>(focusCinematicConfig_.zoomFovy);
		const mu::Degree presentationFovy{
			baseFovy + (focusFovy - baseFovy) * weight
		};
		proj_ = mu::perspReversedZ(presentationFovy, aspect_, nearz_, farz_);
	}

	if (elapsedMs >= durationMs) {
		focusCinematicActive_ = false;
		focusCinematicTarget_.reset();
		focusCinematicElapsed_ = Milliseconds{ 0.f };
		eye_  = baseEye_;
		at_   = baseAt_;
		view_ = baseView_;
		proj_ = baseProj_;
	}
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
	gfx.addCameraData(EnergyOrbPipeline::CameraData{
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
	baseProj_ = proj_;
	fovy_ = fovy;
	aspect_ = aspect;
	nearz_ = nearz;
	farz_ = farz;
	perspectiveProjection_ = true;
}

void Camera::setOrtho(float minX, float minY, float maxX, float maxY, float minZ, float maxZ) {
	proj_ = mu::ortho(minX, maxX, minY, maxY, minZ, maxZ);
	baseProj_ = proj_;
	minX_ = minX;
	maxX_ = maxX;
	minY_ = minY;
	maxY_ = maxY;
	minZ_ = minZ;
	maxZ_ = maxZ;
	perspectiveProjection_ = false;
}
