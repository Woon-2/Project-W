#include "inputSystem.hpp"
#include "physicsSystem.hpp"

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

		// controller.processInput(position, pKeyboard_);
	}
}

void PlayerController::processInput(Rigidbody& rb, ic::Keyboard* pKeyboard)
{    
    if (pKeyboard->pressed('A')) {
        // position.x -= 0.5;
        rb.addForce((-0.5, 0.0, 0.0));
    }

    if (pKeyboard->pressed('D')) {
        // position.x += 0.5;
        rb.addForce((+0.5, 0.0, 0.0));
    }

    if (pKeyboard->pressed('W')) {
        // position.z -= 0.5;
        rb.addForce((0.0, 0.0, -0.5));
    }

    if (pKeyboard->pressed('S')) {
        // position.z += 0.5;
        rb.addForce((0.0, 0.0, 0.5));
    }
    
}
