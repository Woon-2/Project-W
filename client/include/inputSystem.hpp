#ifndef __INPUTSYSTEM_HPP
#define __INPUTSYSTEM_HPP

#include "ecs.hpp"

#include "keyboardXX.hpp"
#include "player.hpp"

#include "d3d12engine/d3d12Engine.hpp"

#include <map>

class Rigidbody;

class PlayerController : public ecs::Component {
public:
    enum class Event {
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight,
        YawLeft,
        YawRight
    };

    ENABLE_COMPONENT(PlayerController);

    PlayerController(const ecs::Entity& entity) NOEXCEPT
        : Component(entity), forceStep_(400.f), yawStep_(0.6f * mu::pi) {}

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
    void yawLeft(float deltaTime);
    void yawRight(float deltaTime);

    void MU_CALLCONV addForce(mu::Vec3 force);

    float forceStep_;
    mu::Radian yawStep_;
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
