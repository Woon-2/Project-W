#include "object.hpp"
#include "errorHandling.hpp"

void Object::update(Milliseconds deltaTime) {
	// 물리 업데이트를 위한 시간 누산
	physicUpdateAcc_ += deltaTime;
	// physicalUpdate를 주기에 맞춰서 호출하는 건
	// 별도의 함수로 마련되어 있지 않고,
	// update에서 누산된 시간을 통해 주기에 맞게 호출해준다.
	while (physicUpdateAcc_ > physicUpdateInterval_) {
		physicalUpdate();
		physicUpdateAcc_ -= deltaTime;
	}

	switch (physicEvaluationMethod_) {
	case PhysicEvaluationMethod::LinearInterpolation: {
		if (physicSnapshots_.size() < 2u) {
			// 선형보간할 대상이 없다.
			const auto& curr = physicSnapshots_.back();
			pos_ = curr.pos;
			omega_ = curr.omega;
			orient_ = curr.orient;
			scale_ = curr.scale;
			right_ = orient_.rotate(mu::Vec3(1.f, 0.f, 0.f));
			up_ = orient_.rotate(mu::Vec3(0.f, 1.f, 0.f));
			forward_ = orient_.rotate(mu::Vec3(0.f, 0.f, 1.f));
			break;
		}

		while (physicSnapshots_.size() > 2) {
			// 선형보간은 두 대상 사이에서만 일어난다.
			physicSnapshots_.pop_front();
		}

		const auto& curr = physicSnapshots_.back();
		const auto& prev = physicSnapshots_.front();

		auto t = physicUpdateAcc_ / physicUpdateInterval_;
		pos_ = mu::lerp(prev.pos, curr.pos, t);
		omega_ = mu::lerp(prev.omega, curr.omega, t);
		orient_ = mu::slerp(prev.orient, curr.orient, t);	// 쿼터니언
		scale_ = mu::lerp(prev.scale, curr.scale, t);
		right_ = orient_.rotate(mu::Vec3(1.f, 0.f, 0.f));
		up_ = orient_.rotate(mu::Vec3(0.f, 1.f, 0.f));
		forward_ = orient_.rotate(mu::Vec3(0.f, 0.f, 1.f));
		break;
	}
	
	default:
		DISPLAY_ERROR_STR( false, "[Physics Error] Object::update: 물리량 평가 방법에 "s
			+ "허용되지 않는 값이 들어있습니다.: "s + std::to_string(etoi(physicEvaluationMethod_))
			+ "\n", false
		);
		break;
	}

	// 월드변환 행렬 갱신
	world_ = mu::Mat4x4(mu::scale(scale_)) * mu::Mat4x4(orient_) * mu::translate(pos_);
}

void Object::render(GFX& gfx) {
	if (pModel_) {
		for (auto& [mesh, dressXform] : pModel_->meshWithDressXforms) {
			for (std::size_t i = 0u; i < mesh.subMeshes.size(); ++i) {
				gfx.addDrawEvent(PBRPipeline::DrawEvent{
					.world = dressXform * world_,
					.mesh = &mesh,
					.subMesh = &mesh.subMeshes[i],
					.material = &mesh.materialSets[materialSetIdx_].materials[i]
				});
			}
		}
	}
}

void MU_CALLCONV Object::setPos(mu::Vec3 newPos) {
	for (auto& snapshot : physicSnapshots_) {
		snapshot.pos = newPos;
	}
	pos_ = newPos;
}

void MU_CALLCONV Object::setOmega(mu::Vec3 newOmega) {
	for (auto& snapshot : physicSnapshots_) {
		snapshot.omega = newOmega;
	}
	omega_ = newOmega;
}

void MU_CALLCONV Object::setOrient(mu::NQuat newOrient) {
	for (auto& snapshot : physicSnapshots_) {
		snapshot.orient = newOrient;
	}
	orient_ = newOrient;
	right_ = orient_.rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_ = orient_.rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = orient_.rotate(mu::Vec3(0.f, 0.f, 1.f));
}

void MU_CALLCONV Object::setScale(mu::Vec3 newScale) {
	for (auto& snapshot : physicSnapshots_) {
		snapshot.scale = newScale;
	}
	scale_ = newScale;
}

// 물리량들을 갱신해 PhysicSnapshot 객체를 생성, 저장한다.
// 그리고 더 이상 유효하지 않은 PhysicSnapshot 객체를 제거한다.
// 
// 물리 업데이트를 고정 시간 간격으로 이루어지게 해
// 너무 유동적인 delta time으로 인한 시뮬레이션의 불안정성과
// 물리 업데이트의 성능적 비용 문제를 해결한다.
// 물리 업데이트 주기는 physicUpdateInterval_ 변수에 저장된다.
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
		DISPLAY_ERROR_STR( false, "[Physics Error] Object::physicalUpdate: 물리량 평가 방법에 "s
			+ "허용되지 않는 값이 들어있습니다.: "s + std::to_string(etoi(physicEvaluationMethod_))
			+ "\n", false
		);
		break;
	}
}