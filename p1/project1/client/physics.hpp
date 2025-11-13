#ifndef __physics_HPP
#define __physics_HPP

#include "pch.hpp"

// 물리 업데이트와 일반 업데이트는 주기가 다르다.
// 일반 업데이트에선 최근의 물리 업데이트 결과들을 이용해
// 최종적으로 평가될 물리량을 결정한다.
// 이때 물리 업데이트 결과들을 저장하기 위해 PhysicSnapshot 구조체를 활용한다.
struct PhysicSnapshot {
	mu::Vec3 pos{};
	mu::Vec3 omega{};
	mu::NQuat orient{};
	mu::Vec3 scale{};
};

// PhysicSnapshot들을 조합하여 update 함수에서 최종 물리량을 결정할 때 사용할 정책
enum class PhysicEvaluationMethod {
	LinearInterpolation
};


#endif	// __physics_HPP