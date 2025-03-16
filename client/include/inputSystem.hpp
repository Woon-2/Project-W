#ifndef __INPUTSYSTEM_HPP
#define __INPUTSYSTEM_HPP

#include "ecs.hpp"

#include "keyboardXX.hpp"

#include "renderer.hpp"

#include "d3d12engine/d3d12Engine.hpp"

#include <map>

struct ControllerAdapters {
    RenderModeController renderModeController;
};

class PlayerController : public ecs::Component {
public:
    enum class Event {
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight,
        MoveUp,
        MoveDown,
        YawLeft,
        YawRight,
        SetRenderModeColor,
        SetRenderModeAlbedo,
        SetRenderModeNormal,
        SetRenderModeDepth,
        SetRenderModeDirectionalLightDepth,
    };

    ENABLE_COMPONENT(PlayerController);

    PlayerController(const ecs::Entity& entity) NOEXCEPT
        : Component(entity), forceStep_(3200.f), yawStep_(0.6f * mu::pi) {}

    void handleEvent(Event event, float deltaTime, const ControllerAdapters& controllerAdapters);

private:
    void moveForward(float deltaTime) {
        addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
            ecs::Component::atC(ecs::Components::Model, entityID().value())
        )->get().root()->coord().localXform().row(0u) ) ) * forceStep_ * deltaTime  );
    }
    void moveBackward(float deltaTime) {
        addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
            ecs::Component::atC(ecs::Components::Model, entityID().value())
        )->get().root()->coord().localXform().row(0u) ) ) * -forceStep_ * deltaTime  );
    }
    void moveLeft(float deltaTime) {
        addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
            ecs::Component::atC(ecs::Components::Model, entityID().value())
        )->get().root()->coord().localXform().row(2u) ) ) * forceStep_ * deltaTime  );
    }
    void moveRight(float deltaTime) {
        addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
            ecs::Component::atC(ecs::Components::Model, entityID().value())
        )->get().root()->coord().localXform().row(2u) ) ) * -forceStep_ * deltaTime  );
    }
    void moveUp(float deltaTime) {
        addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
            ecs::Component::atC(ecs::Components::Model, entityID().value())
        )->get().root()->coord().localXform().row(1u) ) ) * forceStep_ * deltaTime  );
    }
    void moveDown(float deltaTime) {
        addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
            ecs::Component::atC(ecs::Components::Model, entityID().value())
        )->get().root()->coord().localXform().row(1u) ) ) * -forceStep_ * deltaTime  );
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

	void update(float deltaTime, const ControllerAdapters& controllerAdapters);

private:
    void initKeyMap();

    std::map<std::uint8_t, PlayerController::Event> keyMap_;
	ic::Keyboard* pKeyboard_;
};

#endif // !__INPUTSYSTEM_HPP
