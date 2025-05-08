#include "inputSystem.hpp"
#include "game/physicsSystem.hpp"

#include "keyboardXX.hpp"

void StandAloneInputHandler::handleEvent( CInputEvent event,
    float floatVal0, float floatVal1, const Win32::WndClient& client,
    ControllerAdapters& controllerAdapters
) {
    switch (event) {
    case CInputEvent::MoveForward:
        moveForward(floatVal0);
        break;
    case CInputEvent::MoveBackward:
        moveBackward(floatVal0);
        break;
    case CInputEvent::MoveLeft:
        moveLeft(floatVal0);
        break;
    case CInputEvent::MoveRight:
        moveRight(floatVal0);
        break;
    case CInputEvent::Rotation:
        yaw(floatVal0, client);
        break;
        
    case CInputEvent::SetRenderModeColor:
        controllerAdapters.renderModeController.setMode(Renderer::Mode::Color);
        break;
    case CInputEvent::SetRenderModeCascade0Depth:
		controllerAdapters.renderModeController.setMode(Renderer::Mode::Cascade0Depth);
		break;
	case CInputEvent::SetRenderModeCascade1Depth:
		controllerAdapters.renderModeController.setMode(Renderer::Mode::Cascade1Depth);
		break;
	case CInputEvent::SetRenderModeCascade2Depth:
		controllerAdapters.renderModeController.setMode(Renderer::Mode::Cascade2Depth);
		break;
    default:
        throw std::runtime_error("Invalid event");
        break;
    }
}

void StandAloneInputHandler::moveForward(float deltaTime) {
    addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(2u) ) ) * forceStep_ * deltaTime  );
}

void StandAloneInputHandler::moveBackward(float deltaTime) {
    addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(2u) ) ) * -forceStep_ * 0.45f * deltaTime  );
}

void StandAloneInputHandler::moveLeft(float deltaTime) {
    addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(0u) ) ) * -forceStep_ * deltaTime  );
}

void StandAloneInputHandler::moveRight(float deltaTime) {
    addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(0u) ) ) * forceStep_ * deltaTime  );
}

void StandAloneInputHandler::moveUp(float deltaTime) {
    addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(1u) ) ) * forceStep_ * 0.55f * deltaTime  );
}

void StandAloneInputHandler::moveDown(float deltaTime) {
    addForce( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(1u) ) ) * -forceStep_ * 0.55f * deltaTime  );
}


void MU_CALLCONV StandAloneInputHandler::addForce(mu::Vec3 force) {
    auto pRigidBody = RigidBody::at(entityId_);
    if (!pRigidBody) {
        throw ECS_EXCEPT("RigidBody component doesn't exist");
    }

    pRigidBody->addForce(force);
}

void StandAloneInputHandler::yaw(float yawValue, const Win32::WndClient& client) {
    auto pModel = gfx::d3d12engine::Model::at(entityId_);
    if (!pModel) {
        throw ECS_EXCEPT("Model component doesn't exist");
    }

    const auto mouseSensitivity = mu::pi * 2.f;

    pModel->get().root()->coord() << mu::rotateY(
        mu::Radian(yawValue * mouseSensitivity / static_cast<float>(client.width))
    );
}

void InputSystem::update( float deltaTime, const Win32::WndClient& client,
    ControllerAdapters& controllerAdapters
) {
	pKeyboard_->patchKeyState();

    for (auto& playerController : components<PlayerController>()) {
        if (!playerController) {
            throw ECS_EXCEPT("PlayerController component is not valid");
        }

        for (const auto& [key, event] : keyMap_) {
            if (pKeyboard_->pressed(key)) {
                playerController->handleEvent(event, deltaTime, 0.f, client, controllerAdapters);
            }
        }

        while (auto oEv = pMouse_->read()) {
            auto& ev = *oEv;
            if (ev.type() == ic::Mouse::Event::Type::RawDelta) {
                playerController->handleEvent( CInputEvent::Rotation,
                    static_cast<float>(ev.rawDelta().x), static_cast<float>(ev.rawDelta().y),
                    client, controllerAdapters
                );
            }
        }
    }
}

void InputSystem::initKeyMap() {
    keyMap_['W'] = CInputEvent::MoveForward;
    keyMap_['S'] = CInputEvent::MoveBackward;
    keyMap_['A'] = CInputEvent::MoveLeft;
    keyMap_['D'] = CInputEvent::MoveRight;
    keyMap_[VK_CONTROL] = CInputEvent::MoveDown;
    keyMap_[VK_SPACE] = CInputEvent::MoveUp;

    keyMap_['1'] = CInputEvent::SetRenderModeColor;
    keyMap_['2'] = CInputEvent::SetRenderModeCascade0Depth;
    keyMap_['3'] = CInputEvent::SetRenderModeCascade1Depth;
    keyMap_['4'] = CInputEvent::SetRenderModeCascade2Depth;
}