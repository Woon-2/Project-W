#include "physicsSystem.hpp"
#include "player.hpp"

#include "inputSystem.hpp"

#include <iostream>

RigidBody::RigidBody(const ecs::Entity& entity) NOEXCEPT
	: Component(entity) {
	mass_ = 1;
	cornerLocation_ = 0;
}

void MU_CALLCONV RigidBody::addForce(mu::Vec3 force) NOEXCEPT
{
	force_ += force;
}

void MU_CALLCONV RigidBody::updateRigid(float dt, float friction) NOEXCEPT
{
	updateForce(dt, friction);
	updateAngular(dt);
}

void RigidBody::updateForce(float dt, float friction) NOEXCEPT
{
	oldPosition_ = position_;

	// 선운동량과 위치
	position_ += momentum_ / mass_ * dt;
	momentum_ += force_ * dt;

	// 속도 계산
	velocity_ += (force_ / mass_) * dt;	// 가속도 적용 - acceleration = force / mass;

	// 마찰력 적용 (속도에 비례하는 반대 방향의 힘)
	mu::Vec3 frictionForce = -friction * velocity_;

	// 마찰력을 선운동량과 속도에 적용
	momentum_ += frictionForce * dt;
	velocity_ += (frictionForce / mass_) * dt;	// 가속도 적용

	// 속도가 아주 작을 경우 속도를 0으로 설정
	static constexpr auto epsilon = 0.0002f;
	if (velocity_.len() < epsilon) {
		velocity_ = mu::Vec3(0.0f, 0.0f, 0.0f); // 속도를 0으로 설정
		momentum_ = mu::Vec3(0.0f, 0.0f, 0.0f); // 운동량도 0으로 설정
	}

	// 힘을 다 사용했으므로 초기화
	force_ = mu::Vec3(0.0f, 0.0f, 0.0f);
}

void RigidBody::updateAngular(float dt) NOEXCEPT
{
}

void PhysicsSystem::update(float deltaTime)
{
	for (auto& pRigidBody : components<RigidBody>()) {
		if (pRigidBody) {
			pRigidBody->updateRigid(deltaTime, 4.5f);
		}
	}
}
