#include "physicsSystem.hpp"
#include "player.hpp"

#include "inputSystem.hpp"

#include <iostream>

Rigidbody::Rigidbody() NOEXCEPT
{
	mass_ = 1;
	cornerLocation_ = 0;
}

void MU_CALLCONV Rigidbody::addForce(mu::Vec3 force) NOEXCEPT
{
	force_ += force;
}

void MU_CALLCONV Rigidbody::updateRigid(float dt, mu::Vec3& position, float friction) NOEXCEPT
{
	updateForce(dt, position, friction);
	updateAngular(dt);
}

void Rigidbody::updateForce(float dt, mu::Vec3& position, float friction) NOEXCEPT
{
	// 선운동량과 위치
	position += momentum_ / mass_ * dt;
	momentum_ += force_ * dt;

	// 속도 계산
	velocity_ += (force_ / mass_) * dt;	// 가속도 적용 - acceleration = force / mass;

	// 마찰력 적용 (속도에 비례하는 반대 방향의 힘)
	mu::Vec3 frictionForce = -friction * velocity_;

	// 마찰력을 선운동량과 속도에 적용
	momentum_ += frictionForce * dt;
	velocity_ += (frictionForce / mass_) * dt;	// 가속도 적용

	// 속도가 아주 작을 경우 속도를 0으로 설정
	if (velocity_.len() < 0.1f) {
		velocity_ = mu::Vec3(0.0f, 0.0f, 0.0f); // 속도를 0으로 설정
		momentum_ = mu::Vec3(0.0f, 0.0f, 0.0f); // 운동량도 0으로 설정
	}

	// 힘을 다 사용했으므로 초기화
	force_ = mu::Vec3(0.0f, 0.0f, 0.0f);
	oldPosition_ = position;

	// 체크용 코드
	{
		std::cout << "force.x : " << force_.x() << '\n';
		std::cout << "force.y : " << force_.y() << '\n';
		std::cout << "force.z : " << force_.z() << '\n';
		std::cout << "velocity.x : " << velocity_.x() << '\n';
		std::cout << "velocity.y : " << velocity_.y() << '\n';
		std::cout << "velocity.z : " << velocity_.z() << '\n';
	}


}

void Rigidbody::updateAngular(float dt) NOEXCEPT
{
}

void PhysicsSystem::update(float deltaTime)
{
	pKeyboard_->patchKeyState();

	auto& system = ecs::gSystems[typeid(PhysicsSystem).name()];

	for (auto const& entity : system->entites_)
	{
		auto& rb = ecs::GetComponent<Rigidbody>(entity);
		auto& pos = ecs::GetComponent<Position>(entity);
		auto& controller = ecs::GetComponent<PlayerController>(entity);

		mu::Vec3 position{ static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z) };

		controller.processInput(rb, pKeyboard_);

		rb.updateRigid(deltaTime, position, 0.8f);
		// rb.setPosition(position);
	}
}
