#include "rspch.hpp"
#include "object.hpp"
#include "Model.hpp"

// 모델을 설정한다.
// 모델에 바운딩 볼륨이 존재할 경우, 월드 공간 바운딩 볼륨을 구축한다.
// (모델의 바운딩 볼륨을 기반으로 게임 객체의 월드 변환을 적용한
//  월드 공간 바운딩 볼륨을 따로 두어야 월드 공간 충돌 처리가 가능하다.)
void Object::setModel(const Model* pModel){
	DISPLAY_ERROR_STR(pModel != nullptr, "[Game Error] Object::setModel: 널 모델이 전달되었습니다.", false);
	if (pModel == nullptr) {
		return;
	}

	pModel_ = pModel;

	physicState_.aabbs.resize(pModel->aabbs.size());
	for (std::size_t i = 0u; i < physicState_.aabbs.size(); ++i) {
		physicState_.aabbs[i].center
			= pModel->aabbs[i].center * physicState_.scale
			+ physicState_.pos;
		physicState_.aabbs[i].size
			= pModel->aabbs[i].size * physicState_.scale;
	}

	const auto pos2D = mu::Vec2(physicState_.pos.x(), physicState_.pos.z());
	const auto scale2D = mu::Vec2(physicState_.scale.x(), physicState_.scale.z());

	physicState_.boundingRects.resize(pModel->boundingRects.size());
	for (std::size_t i = 0u; i < physicState_.boundingRects.size(); ++i) {
		physicState_.boundingRects[i].center
			= pModel->boundingRects[i].center * scale2D + pos2D;
		physicState_.boundingRects[i].size
			= pModel->boundingRects[i].size * scale2D;
	}
}

// 물리 시뮬레이션과 별개로 게임 객체의
	// 자체적인 갱신 루틴을 필요로 할 때 이 함수에 작성한다.
void Object::update(Milliseconds deltaTime) {

}

// 게임 객체의 위치를 갱신한다.
// PhysicState의 AABB와 Bounding Rect 역시 갱신된다.
void MU_CALLCONV Object::setPos(mu::Vec3 newPos) {
	physicState_.pos = newPos;


	if (!pModel_) {
		return;
	}

	physicState_.aabbs.resize(pModel_->aabbs.size());
	for (std::size_t i = 0u; i < physicState_.aabbs.size(); ++i) {
		physicState_.aabbs[i].center
			= pModel_->aabbs[i].center * physicState_.scale
			+ physicState_.pos;
		physicState_.aabbs[i].size
			= pModel_->aabbs[i].size * physicState_.scale;
	}

	const auto pos2D = mu::Vec2(physicState_.pos.x(), physicState_.pos.z());
	const auto scale2D = mu::Vec2(physicState_.scale.x(), physicState_.scale.z());

	physicState_.boundingRects.resize(pModel_->boundingRects.size());
	for (std::size_t i = 0u; i < physicState_.boundingRects.size(); ++i) {
		physicState_.boundingRects[i].center
			= pModel_->boundingRects[i].center * scale2D + pos2D;
		physicState_.boundingRects[i].size
			= pModel_->boundingRects[i].size * scale2D;
	}
}

// 게임 객체의 속도를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 속도가 모두 갱신된다.
void MU_CALLCONV Object::setVelocity(mu::Vec3 newVelocity) {
	physicState_.velocity = newVelocity;
}

// 게임 객체의 각속도를 갱신한다.
void MU_CALLCONV Object::setOmega(mu::Vec3 newOmega) {
	physicState_.omega = newOmega;
}

// 게임 객체의 방향을 갱신한다.
// 게임 객체의 방향 벡터들도 전부 갱신된다.
void MU_CALLCONV Object::setOrient(mu::NQuat newOrient) {
	physicState_.orient = newOrient;
	right_ = physicState_.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_ = physicState_.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = physicState_.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));
}

// 게임 객체의 크기를 갱신한다.
// PhysicState의 AABB와 Bounding Rect 역시 갱신된다.
void MU_CALLCONV Object::setScale(mu::Vec3 newScale) {
	physicState_.scale = newScale;


	if (!pModel_) {
		return;
	}

	physicState_.aabbs.resize(pModel_->aabbs.size());
	for (std::size_t i = 0u; i < physicState_.aabbs.size(); ++i) {
		physicState_.aabbs[i].center
			= pModel_->aabbs[i].center * physicState_.scale
			+ physicState_.pos;
		physicState_.aabbs[i].size
			= pModel_->aabbs[i].size * physicState_.scale;
	}

	const auto pos2D = mu::Vec2(physicState_.pos.x(), physicState_.pos.z());
	const auto scale2D = mu::Vec2(physicState_.scale.x(), physicState_.scale.z());

	physicState_.boundingRects.resize(pModel_->boundingRects.size());
	for (std::size_t i = 0u; i < physicState_.boundingRects.size(); ++i) {
		physicState_.boundingRects[i].center
			= pModel_->boundingRects[i].center * scale2D + pos2D;
		physicState_.boundingRects[i].size
			= pModel_->boundingRects[i].size * scale2D;
	}
}