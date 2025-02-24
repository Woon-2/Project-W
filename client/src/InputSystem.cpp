#include "inputSystem.hpp"
#include "physicsSystem.hpp"

#include "player.hpp"

#include "keyboardXX.hpp"

void PlayerController::handleEvent( Event event, float deltaTime,
    const ControllerAdapters& controllerAdapters
) {
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
    case Event::MoveUp:
        moveUp(deltaTime);
        break;
    case Event::MoveDown:
        moveDown(deltaTime);
        break;
    case Event::YawLeft:
        yawLeft(deltaTime);
        break;
    case Event::YawRight:
        yawRight(deltaTime);
        break;
    case Event::SetRenderModeColor:
        controllerAdapters.renderModeController.setMode(Renderer::Mode::Color);
        break;
    case Event::SetRenderModeAlbedo:
        controllerAdapters.renderModeController.setMode(Renderer::Mode::Albedo);
        break;
    case Event::SetRenderModeNormal:
        controllerAdapters.renderModeController.setMode(Renderer::Mode::Normal);
        break;
    case Event::SetRenderModeDepth:
        controllerAdapters.renderModeController.setMode(Renderer::Mode::Depth);
        break;
    case Event::SetRenderModeDirectionalLightDepth:
        controllerAdapters.renderModeController.setMode(Renderer::Mode::DirectionalLightDepth);
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

    auto pRigidBody = RigidBody::at(entityID().value());
    if (!pRigidBody) {
        throw ECS_EXCEPT("RigidBody component doesn't exist");
    }

    pRigidBody->addForce(force);
}

void PlayerController::yawLeft(float deltaTime) {
    if (!valid()) {
        throw ECS_EXCEPT("Component is not valid");
    }

    auto pModel = gfx::d3d12engine::Model::at(entityID().value());
    if (!pModel) {
        throw ECS_EXCEPT("Model component doesn't exist");
    }

    auto yaw = yawStep_;
    yaw *= -deltaTime;

    pModel->get().root()->coord() << mu::rotateYH(yaw);
}

void PlayerController::yawRight(float deltaTime) {
    if (!valid()) {
        throw ECS_EXCEPT("Component is not valid");
    }

    auto pModel = gfx::d3d12engine::Model::at(entityID().value());
    if (!pModel) {
        throw ECS_EXCEPT("Model component doesn't exist");
    }

    auto yaw = yawStep_;
    yaw *= deltaTime;

    pModel->get().root()->coord() << mu::rotateYH(yaw);
}

void InputSystem::update(float deltaTime, const ControllerAdapters& controllerAdapters) {
	pKeyboard_->patchKeyState();

    for (auto& playerController : components<PlayerController>()) {
        if (!playerController) {
            throw ECS_EXCEPT("PlayerController component is not valid");
        }

        for (const auto& [key, event] : keyMap_) {
            if (pKeyboard_->pressed(key)) {
                playerController->handleEvent(event, deltaTime, controllerAdapters);
            }
        }
    }
}

void InputSystem::initKeyMap() {
    keyMap_['W'] = PlayerController::Event::MoveForward;
    keyMap_['S'] = PlayerController::Event::MoveBackward;
    keyMap_['A'] = PlayerController::Event::MoveLeft;
    keyMap_['D'] = PlayerController::Event::MoveRight;
    keyMap_[VK_CONTROL] = PlayerController::Event::MoveDown;
    keyMap_[VK_SPACE] = PlayerController::Event::MoveUp;
    keyMap_['Q'] = PlayerController::Event::YawLeft;
    keyMap_['E'] = PlayerController::Event::YawRight;

    keyMap_['1'] = PlayerController::Event::SetRenderModeColor;
    keyMap_['2'] = PlayerController::Event::SetRenderModeAlbedo;
    keyMap_['3'] = PlayerController::Event::SetRenderModeNormal;
    keyMap_['4'] = PlayerController::Event::SetRenderModeDepth;
    keyMap_['5'] = PlayerController::Event::SetRenderModeDirectionalLightDepth;
}