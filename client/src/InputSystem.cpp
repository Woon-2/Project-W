#include "inputSystem.hpp"

#include "player.hpp"

#include "keyboardXX.hpp"

void InputSystem::update()
{
	pKeyboard_->patchKeyState();

	auto& system = ecs::gSystems[typeid(InputSystem).name()];


	for (auto const& entity : system->entites_)
	{
		auto& controller = ecs::GetComponent<PlayerController>(entity);
		auto& position = ecs::GetComponent<Position>(entity);

		controller.processInput(position, pKeyboard_);
	}
}
