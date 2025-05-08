#ifndef __INPUTSYSTEM_HPP
#define __INPUTSYSTEM_HPP

#include "ecs.hpp"

#include "keyboardXX.hpp"
#include "mouse.hpp"

#include "renderer.hpp"
#include "cNetEx.hpp"
#include "Window.hpp"

#include "d3d12engine/d3d12Engine.hpp"

#include <map>

struct ControllerAdapters {
    RenderModeController renderModeController;
    InputNetworkForwarder inputNetworkForwarder;
};

enum class CInputEvent : int {
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    Rotation,
    SetRenderModeColor,
    SetRenderModeCascade0Depth,
    SetRenderModeCascade1Depth,
    SetRenderModeCascade2Depth
};

class IPlayerInputHandler {
public:
    virtual ~IPlayerInputHandler() = default;
    virtual void handleEvent( CInputEvent event, float floatVal0, float floatVal1,
        const Win32::WndClient& client, ControllerAdapters& controllerAdapters
    ) = 0;
};

class PlayerController : public ecs::Component {
public:
    ENABLE_COMPONENT(PlayerController);

    PlayerController(const ecs::Entity& entity, std::unique_ptr<IPlayerInputHandler>&& inputHandler) NOEXCEPT
        : Component(entity), pInputHandler_(std::move(inputHandler)), forceStep_(1700.f), yawStep_(0.6f * mu::pi) {}

    void handleEvent( CInputEvent event, float floatVal0, float floatVal1,
        const Win32::WndClient& client, ControllerAdapters& controllerAdapters
    ) {
        if (pInputHandler_) {
            pInputHandler_->handleEvent(event, floatVal0, floatVal1, client, controllerAdapters);
        }
    }

private:
    std::unique_ptr<IPlayerInputHandler> pInputHandler_;
    float forceStep_;
    mu::Radian yawStep_;
};

class StandAloneInputHandler : public IPlayerInputHandler {
public:
    StandAloneInputHandler(ecs::Entity::ID entityId) NOEXCEPT
        : entityId_(entityId), forceStep_(1700.f) {}

    void handleEvent(CInputEvent event, float floatVal0, float floatVal1,
        const Win32::WndClient& client, ControllerAdapters& controllerAdapters
    ) override;

private:
    void moveForward(float deltaTime);
    void moveBackward(float deltaTime);
    void moveLeft(float deltaTime);
    void moveRight(float deltaTime);
    void moveUp(float deltaTime);
    void moveDown(float deltaTime);
    void yaw(float yawValue, const Win32::WndClient& client);

    void MU_CALLCONV addForce(mu::Vec3 force);

    ecs::Entity::ID entityId_;
    float forceStep_;
};

class NetworkInputHandler : public IPlayerInputHandler {
public:
    NetworkInputHandler(ecs::Entity::ID entityId) NOEXCEPT
        : entityId_(entityId) {}

    void handleEvent(CInputEvent event, float floatVal0, float floatVal1,
        const Win32::WndClient& client, ControllerAdapters& controllerAdapters
    ) override;

private:
    ecs::Entity::ID entityId_;
};


class InputSystem : public ecs::System<PlayerController> {
public:
    InputSystem(ic::Keyboard* pKeyboard, ic::Mouse* pMouse) : pKeyboard_(pKeyboard), pMouse_(pMouse) {
        initKeyMap();
    }

	void update(float deltaTime, const Win32::WndClient& client, ControllerAdapters& controllerAdapters) ;

private:
    void initKeyMap();

    std::map<std::uint8_t, CInputEvent> keyMap_;
	ic::Keyboard* pKeyboard_;
    ic::Mouse* pMouse_;
};

#endif // !__INPUTSYSTEM_HPP
