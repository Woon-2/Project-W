#include "light.hpp"
#include "errorHandling.hpp"

void Light::update(Milliseconds deltaTime) {
	// 물리 업데이트를 위한 시간 누산
	physicUpdateAcc_ += deltaTime;
	// physicalUpdate를 주기에 맞춰서 호출하는 건
	// 별도의 함수로 마련되어 있지 않고,
	// update에서 누산된 시간을 통해 주기에 맞게 호출해준다.
	while (physicUpdateAcc_ > physicUpdateInterval_) {
		physicalUpdate();
		physicUpdateAcc_ -= deltaTime;
	}

	auto evaluated = PhysicSnapshot{};	// 최종 평가된 물리량들을 담는다.

	switch (physicEvaluationMethod_) {
	case PhysicEvaluationMethod::LinearInterpolation: {
		if (physicSnapshots_.size() < 2u) {
			// 선형보간할 대상이 없다.
			evaluated = physicSnapshots_.back();
			break;
		}

		while (physicSnapshots_.size() > 2) {
			// 선형보간은 두 대상 사이에서만 일어난다.
			physicSnapshots_.pop_front();
		}

		const auto& curr = physicSnapshots_.back();
		const auto& prev = physicSnapshots_.front();

		auto t = physicUpdateAcc_ / physicUpdateInterval_;
		evaluated.pos = mu::lerp(prev.pos, curr.pos, t);
		evaluated.omega = mu::lerp(prev.omega, curr.omega, t);
		evaluated.orient = mu::slerp(prev.orient, curr.orient, t);	// 쿼터니언
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
	for (auto& snapshot : physicSnapshots_) {
		snapshot.pos = newPos;
	}
}

void MU_CALLCONV Light::setOmega(mu::Vec3 newOmega) {
	for (auto& snapshot : physicSnapshots_) {
		snapshot.omega = newOmega;
	}
}

void MU_CALLCONV Light::setOrient(mu::NQuat newOrient) {
	for (auto& snapshot : physicSnapshots_) {
		snapshot.orient = newOrient;
	}
}

void Light::setScale(float newScale) {
	for (auto& snapshot : physicSnapshots_) {
		snapshot.scale = newScale;
	}
}

// 물리량들을 갱신해 PhysicSnapshot 객체를 생성, 저장한다.
// 그리고 더 이상 유효하지 않은 PhysicSnapshot 객체를 제거한다.
// 
// 물리 업데이트를 고정 시간 간격으로 이루어지게 해
// 너무 유동적인 delta time으로 인한 시뮬레이션의 불안정성과
// 물리 업데이트의 성능적 비용 문제를 해결한다.
// 물리 업데이트 주기는 physicUpdateInterval_ 변수에 저장된다.
void Light::physicalUpdate() {
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