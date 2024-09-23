#ifndef __INPUTSYSTEM_HPP
#define __INPUTSYSTEM_HPP

#include "ecs.hpp"

#include "keyboardXX.hpp"
#include "player.hpp"

#include <map>

class Rigidbody;

class PlayerController : public ecs::Component {
public:
    enum class Event {
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight
    };

    ENABLE_COMPONENT(PlayerController);

    PlayerController(const ecs::Entity& entity) NOEXCEPT
        : Component(entity), forceStep_(5.f) {}

    void handleEvent(Event event, float deltaTime);

private:
    void moveForward(float deltaTime) {
        addForce( mu::Vec3(0.f, 0.f, forceStep_ * deltaTime) );
    }
    void moveBackward(float deltaTime) {
        addForce( mu::Vec3(0.f, 0.f, -forceStep_ * deltaTime) );
    }
    void moveLeft(float deltaTime) {
        addForce( mu::Vec3(-forceStep_ * deltaTime, 0.f, 0.f) );
    }
    void moveRight(float deltaTime) {
        addForce( mu::Vec3(forceStep_ * deltaTime, 0.f, 0.f) );
    }

    void MU_CALLCONV addForce(mu::Vec3 force);

    float forceStep_;
};


class InputSystem : public ecs::System<PlayerController> {
public:
    InputSystem(ic::Keyboard& keyboard) : pKeyboard_(&keyboard) {
        initKeyMap();
    }

	void update(float deltaTime);

private:
    void initKeyMap();

    std::map<std::uint8_t, PlayerController::Event> keyMap_;
	ic::Keyboard* pKeyboard_;
};

#endif // !__INPUTSYSTEM_HPP
