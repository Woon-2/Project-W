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
    constexpr auto step = 3.f;

    if (pKeyboard->pressed('A')) {
        // position.x -= 0.5;
        rb.addForce(mu::Vec3(-step, 0.0f, 0.0f));
    }

    if (pKeyboard->pressed('D')) {
        // position.x += 0.5;
        rb.addForce(mu::Vec3(+step, 0.0f, 0.0f));
    }

    if (pKeyboard->pressed('W')) {
        // position.z -= 0.5;
        rb.addForce(mu::Vec3(0.0f, 0.0f, step));
    }

    if (pKeyboard->pressed('S')) {
        // position.z += 0.5;
        rb.addForce({ 0.0f, 0.0f, -step });
    }
    
}
