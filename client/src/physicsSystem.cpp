#include "physicsSystem.hpp"
#include "player.hpp"

#include "inputSystem.hpp"

Rigidbody::Rigidbody() NOEXCEPT
{
	mass_ = 1;
	cornerLocation_ = 0;
}

void MU_CALLCONV Rigidbody::addForce(mu::Vec3 force) NOEXCEPT
{
	force_ += force;
}

void MU_CALLCONV Rigidbody::updateRigid(float dt, mu::Vec3& position) NOEXCEPT
{
	updateForce(dt, position);
	updateAngular(dt);
}

void Rigidbody::updateForce(float dt, mu::Vec3& position) NOEXCEPT
{
	// 선운동량과 위치
	position += momentum_ / mass_ * dt;
	momentum_ += force_ * dt;
	velocity_ += (force_ / mass_) * dt;	// 가속도 적용 - acceleration = force / mass;
}

void Rigidbody::updateAngular(float dt) NOEXCEPT
{
}

void PhysicsSystem::update(float deltaTime)
{
	auto& system = ecs::gSystems[typeid(PhysicsSystem).name()];

	for (auto const& entity : system->entites_)
	{
		auto& rb = ecs::GetComponent<Rigidbody>(entity);
		auto& pos = ecs::GetComponent<Position>(entity);
		auto& controller = ecs::GetComponent<PlayerController>(entity);

		mu::Vec3 position{ static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z) };

		controller.processInput(rb, pKeyboard_);

		rb.updateRigid(deltaTime, position);
		// rb.setPosition(position);
	}
}
