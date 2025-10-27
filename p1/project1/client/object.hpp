#ifndef __object_HPP
#define __object_HPP

#include "pch.hpp"
#include "gfx.hpp"

// 물리 업데이트와 일반 업데이트는 주기가 다르다.
// 일반 업데이트에선 최근의 물리 업데이트 결과들을 이용해
// 최종적으로 평가될 물리량을 결정한다.
// 이때 물리 업데이트 결과들을 저장하기 위해 PhysicSnapshot 구조체를 활용한다.
struct PhysicSnapshot {
	mu::Vec3 pos{};
	mu::Vec3 omega{};
	mu::NQuat orient{};
	float scale = 0.f;
};

// PhysicSnapshot들을 조합하여 update 함수에서 최종 물리량을 결정할 때 사용할 정책
enum class PhysicEvaluationMethod {
	LinearInterpolation
};

class Object {
public:
	void setMesh(const Mesh* pMesh) { pMesh_ = pMesh; }
	void update(Milliseconds deltaTime);
	void render(GFX& gfx);

	void MU_CALLCONV setPos(mu::Vec3 newPos);
	mu::Vec3 MU_CALLCONV pos() const { return physicSnapshots_.back().pos; }
	void MU_CALLCONV setOmega(mu::Vec3 newOmega);
	mu::Vec3 MU_CALLCONV omega() const { return physicSnapshots_.back().omega; }
	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat MU_CALLCONV orient() const { return physicSnapshots_.back().orient; }
	void setScale(float newScale);
	float scale() const { return physicSnapshots_.back().scale; }

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
	const Mesh* pMesh_ = nullptr;

	std::list<PhysicSnapshot> physicSnapshots_ = std::list<PhysicSnapshot>(1u);
	PhysicEvaluationMethod physicEvaluationMethod_
		= PhysicEvaluationMethod::LinearInterpolation;

	Seconds physicUpdateAcc_{0s};	// 물리 업데이트를 위한 시간 누산기
	Seconds physicUpdateInterval_{1s/60.f};	// 60 FPS로 물리 업데이트
};

#endif	// __object_HPP