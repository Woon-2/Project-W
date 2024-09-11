#include "player.hpp"

#include "inputSystem.hpp"
#include "physicsSystem.hpp"

void Player::Init()
{
	entityNumber_ = ecs::CreateEntity();
	Position position{ 0, 0, 0 };
	ecs::AddComponent(entityNumber_, position);
	PlayerController controller;
	ecs::AddComponent(entityNumber_, controller);
	Rigidbody rb;
	ecs::AddComponent(entityNumber_, rb);

	ecs::Signature signature;
	signature.set(ecs::GetComponentType<Position>());
	signature.set(ecs::GetComponentType<PlayerController>());
	signature.set(ecs::GetComponentType<Rigidbody>());
	ecs::SetSignature(entityNumber_, signature);
	
	ecs::SetEntity(typeid(PhysicsSystem).name(), entityNumber_);

	ecs::GetComponent<Rigidbody>(entityNumber_).setPosition(mu::Vec3(
		static_cast<float>(position.x),
		static_cast<float>(position.y),
		static_cast<float>(position.z)
	));
}
