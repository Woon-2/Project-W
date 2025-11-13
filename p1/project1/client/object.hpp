#ifndef __object_HPP
#define __object_HPP

#include "pch.hpp"
#include "gfx.hpp"
#include "physics.hpp"

class Object {
public:
	void setModel(const Model* pModel) { pModel_ = pModel; }
	void update(Milliseconds deltaTime);
	void render(GFX& gfx);

	void MU_CALLCONV setPos(mu::Vec3 newPos);
	mu::Vec3 MU_CALLCONV pos() const { return pos_; }
	void MU_CALLCONV setOmega(mu::Vec3 newOmega);
	mu::Vec3 MU_CALLCONV omega() const { return omega_; }
	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat MU_CALLCONV orient() const { return orient_; }
	void MU_CALLCONV setScale(mu::Vec3 newScale);
	mu::Vec3 MU_CALLCONV scale() const { return scale_; }
	mu::Vec3 MU_CALLCONV forward() const { return forward_; }
	mu::Vec3 MU_CALLCONV right() const { return right_; }
	mu::Vec3 MU_CALLCONV up() const { return up_; }

	void setMaterialSetIdx(u32t idx) { materialSetIdx_ = idx; }
	u32t mateiralSetIdx() const { return materialSetIdx_; }

	void setId( i32t id ) {	id_ = id; }
	i32t getId( ) const { return id_; }

private:
	// 물리량들을 갱신해 PhysicSnapshot 객체를 생성, 저장한다.
	// 그리고 더 이상 유효하지 않은 PhysicSnapshot 객체를 제거한다.
	// 
	// 물리 업데이트를 고정 시간 간격으로 이루어지게 해
	// 너무 유동적인 delta time으로 인한 시뮬레이션의 불안정성과
	// 물리 업데이트의 성능적 비용 문제를 해결한다.
	// 물리 업데이트 주기는 physicUpdateInterval_ 변수에 저장된다.
	void physicalUpdate();

	mu::Mat4x4 world_{};	// GFX에 행렬을 전달할 때만 사용된다.
	const Model* pModel_ = nullptr;

	std::list<PhysicSnapshot> physicSnapshots_ = std::list<PhysicSnapshot>(1u);
	PhysicEvaluationMethod physicEvaluationMethod_
		= PhysicEvaluationMethod::LinearInterpolation;

	mu::Vec3 pos_;
	mu::NVec3 forward_;
	mu::NVec3 right_;
	mu::NVec3 up_;
	mu::Vec3 omega_{};
	mu::NQuat orient_{};
	mu::Vec3 scale_{};

	Seconds physicUpdateAcc_{0s};	// 물리 업데이트를 위한 시간 누산기
	Seconds physicUpdateInterval_{1s/60.f};	// 60 FPS로 물리 업데이트

	u32t materialSetIdx_ = 0u;
	i32t id_{ -1 };
};

#endif	// __object_HPP