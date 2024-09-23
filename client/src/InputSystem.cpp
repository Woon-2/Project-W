#include "inputSystem.hpp"
#include "physicsSystem.hpp"

#include "player.hpp"

#include "keyboardXX.hpp"

void PlayerController::handleEvent(Event event, float deltaTime) {
    switch (event) {
    case Event::MoveForward:
        moveForward(deltaTime);
        break;
    case Event::MoveBackward:
        moveBackward(deltaTime);
        break;
    case Event::MoveLeft:
        moveLeft(deltaTime);
        break;
    case Event::MoveRight:
        moveRight(deltaTime);
        break;
    default:
        throw std::runtime_error("Invalid event");
        break;
    }
}

void MU_CALLCONV PlayerController::addForce(mu::Vec3 force) {
    if (!valid()) {
        throw ECS_EXCEPT("Component is not valid");
    }

    auto pRigidBodyBase = Component::at(ecs::Components::RigidBody, entityID().value()).lock();
    if (!pRigidBodyBase) {
        throw ECS_EXCEPT("RigidBody component doesn't exist");
    }

    std::static_pointer_cast<RigidBody>(pRigidBodyBase)->addForce(force);
}

void InputSystem::update(float deltaTime) {
	pKeyboard_->patchKeyState();

    for (auto& weakPC : components<PlayerController>()) {
        auto pc = weakPC.lock();
        if (!pc) {
            throw ECS_EXCEPT("PlayerController component is not valid");
        }

        for (const auto& [key, event] : keyMap_) {
            if (pKeyboard_->pressed(key)) {
                pc->handleEvent(event, deltaTime);
            }
        }
    }
}

void InputSystem::initKeyMap() {
    keyMap_['W'] = PlayerController::Event::MoveForward;
    keyMap_['S'] = PlayerController::Event::MoveBackward;
    keyMap_['A'] = PlayerController::Event::MoveLeft;
    keyMap_['D'] = PlayerController::Event::MoveRight;
}