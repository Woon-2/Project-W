#include "player.hpp"

#include "inputSystem.hpp"

void Player::Init()
{
	entityNumber_ = ecs::CreateEntity();
	Position position{ 0, 0, 0 };
	ecs::AddComponent(entityNumber_, position);
	PlayerController controller;
	ecs::AddComponent(entityNumber_, controller);

	ecs::Signature signature;
	signature.set(ecs::GetComponentType<PlayerController>());
	signature.set(ecs::GetComponentType<Position>());
	ecs::SetSignature(entityNumber_, signature);
	
	ecs::SetEntity(typeid(InputSystem).name(), entityNumber_);
}
