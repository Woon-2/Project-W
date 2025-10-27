#include "object.hpp"
#include "errorHandling.hpp"

void Object::update(Milliseconds deltaTime) {
	physicUpdateAcc_ += deltaTime;
	while (physicUpdateAcc_ > physicUpdateInterval_) {
		physicalUpdate();
		physicUpdateAcc_ -= deltaTime;
	}

	auto evaluated = PhysicSnapshot{};

	switch (physicEvaluationMethod_) {
	case PhysicEvaluationMethod::LinearInterpolation: {
		if (physicSnapshots_.size() < 2u) {
			evaluated = physicSnapshots_.back();
			break;
		}

		while (physicSnapshots_.size() > 2) {
			physicSnapshots_.pop_front();
		}

		const auto& curr = physicSnapshots_.back();
		const auto& prev = physicSnapshots_.front();

		auto t = physicUpdateAcc_ / physicUpdateInterval_;
		evaluated.pos = mu::lerp(prev.pos, curr.pos, t);
		evaluated.omega = mu::lerp(prev.omega, curr.omega, t);
		evaluated.orient = mu::slerp(prev.orient, curr.orient, t);
		evaluated.scale = std::lerp(prev.scale, curr.scale, t);
		break;
	}
	
	default:
		DISPLAY_ERROR_STR( false, L"[Physics Error] Object::update: 물리량 평가 방법에 "s
			+ L"허용되지 않는 값이 들어있습니다.: "s + std::to_wstring(etoi(physicEvaluationMethod_))
			+ L"\n", false
		);
		break;
	}

	// 월드변환 행렬 갱신
	world_ = mu::Mat4x4(mu::scale(evaluated.scale, evaluated.scale, evaluated.scale))
		* mu::Mat4x4(evaluated.orient) * mu::translate(evaluated.pos);
}

void Object::render(GFX& gfx) {
	gfx.addDrawEvent(SamplePipeline::DrawEvent{
		.world = world_,
		.mesh = pMesh_,
		.subMesh = &pMesh_->subMeshes.at(L"CubeMesh_SubMesh")	// 임시 값
	});
}

void MU_CALLCONV Object::setPos(mu::Vec3 newPos) {
	for (auto& snapshot : physicSnapshots_) {
		snapshot.pos = newPos;
	}
}

void MU_CALLCONV Object::setOmega(mu::Vec3 newOmega) {
	for (auto& snapshot : physicSnapshots_) {
		snapshot.omega = newOmega;
	}
}

void MU_CALLCONV Object::setOrient(mu::NQuat newOrient) {
	for (auto& snapshot : physicSnapshots_) {
		snapshot.orient = newOrient;
	}
}

void Object::setScale(float newScale) {
	for (auto& snapshot : physicSnapshots_) {
		snapshot.scale = newScale;
	}
}

void Object::physicalUpdate() {
	const auto& lastSnapshot = physicSnapshots_.back();

	const auto pos = lastSnapshot.pos;
	const auto omega = lastSnapshot.omega;
	auto orient = lastSnapshot.orient;
	const auto scale = lastSnapshot.scale;

	// 쿼터니언 갱신: q' = 0.5 * ω_q * q
	auto wq = mu::Quat(omega, 0.f);
	auto dq = orient * wq * 0.5f;
	orient = orient + dq * physicUpdateInterval_.count();

	physicSnapshots_.emplace_back(pos, omega, orient, scale);

	switch (physicEvaluationMethod_) {
	case PhysicEvaluationMethod::LinearInterpolation:
		while (physicSnapshots_.size() > 2) {
			physicSnapshots_.pop_front();
		}
		break;
	
	default:
		DISPLAY_ERROR_STR( false, L"[Physics Error] Object::physicalUpdate: 물리량 평가 방법에 "s
			+ L"허용되지 않는 값이 들어있습니다.: "s + std::to_wstring(etoi(physicEvaluationMethod_))
			+ L"\n", false
		);
		break;
	}
}