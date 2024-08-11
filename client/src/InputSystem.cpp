#include "inputSystem.hpp"

#include "player.hpp"

#include "keyboard.hpp"

void InputSystem::init(KeyBoard& keyboard)
{
	keyboard_ = &keyboard;
}

void InputSystem::update()
{
	auto& system = ecs::gSystems[typeid(InputSystem).name()];


	for (auto const& entity : system->entites_)
	{
		auto& controller = ecs::GetComponent<PlayerController>(entity);
		auto& position = ecs::GetComponent<Position>(entity);

		controller.processInput(position, keyboard_->GetKeyStates());
	}
}
