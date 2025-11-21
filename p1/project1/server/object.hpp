#ifndef __object_HPP
#define __object_HPP

#include "physics.hpp"

struct PhysicState {
	mu::Vec3 pos{};
	mu::Vec3 omega{};
	mu::NQuat orient{};
	mu::Vec3 scale{};

	std::vector<AABB> aabbs{};
};

// 물체의 상태는 PhysicState, RenderState 두 층위로 관리된다.
// 위치, 방향, 크기와 같이 물리량에 관련된 정보는 PhysicState에 보관되고
// 월드 변환 행렬, 모델 정보와 같이 렌더링에 관련된 정보는 RenderState에 보관된다.
//
// PhysicState의 갱신 주기와 RenderState의 갱신 주기는 다르다.
// (게임 객체가 서로 다른 주기로 갱신한다.)
// 객체의 물리량 갱신은 PhysicSystem이 도맡아 하므로,
// Object::update는 그 내용을 바탕으로 RenderState의 갱신을 주요 과제로 삼는다.
// 게임 객체는 이전 PhysicState와 현재 PhysicState의 내용을 보관하여,
// Object::update를 호출한 시점에 맞게 두 PhysicState를 보간해 월드변환들을 갱신한다.
class Object {
public:
	// 물리 시뮬레이션과 별개로 게임 객체의
	// 자체적인 갱신 루틴을 필요로 할 때 이 함수에 작성한다.
	void update(Milliseconds deltaTime);

	// 게임 객체의 위치를 갱신한다.
	// PhysicState의 AABB 역시 갱신된다.
	void MU_CALLCONV setPos(mu::Vec3 newPos);
	mu::Vec3 MU_CALLCONV pos() const { return physicState_.pos; }
	// 게임 객체의 각속도를 갱신한다.
	void MU_CALLCONV setOmega(mu::Vec3 newOmega);
	mu::Vec3 MU_CALLCONV omega() const { return physicState_.omega; }
	// 게임 객체의 방향을 갱신한다.
	// 게임 객체의 방향 벡터들도 전부 갱신된다.
	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat MU_CALLCONV orient() const { return physicState_.orient; }
	// 게임 객체의 크기를 갱신한다.
	// PhysicState의 AABB 역시 갱신된다.
	void MU_CALLCONV setScale(mu::Vec3 newScale);
	mu::Vec3 MU_CALLCONV scale() const { return physicState_.scale; }

	mu::Vec3 MU_CALLCONV forward() const { return forward_; }
	mu::Vec3 MU_CALLCONV right() const { return right_; }
	mu::Vec3 MU_CALLCONV up() const { return up_; }

	PhysicState& physicState() { return physicState_; }

	// 재질 집합 인덱스를 설정한다.
	void setMaterialSetIdx(u32t idx) { materialSetIdx_ = idx; }
	u32t mateiralSetIdx() const { return materialSetIdx_; }

	void setId( i32t id ) {	id_ = id; }
	i32t getId( ) const { return id_; }

private:
	PhysicState physicState_{};

	mu::Vec3 forward_{};
	mu::Vec3 right_{};
	mu::Vec3 up_{};

	u32t materialSetIdx_ = 0u;
	i32t id_{ -1 };
};

#endif	// __object_HPP