#include "inputSystem.hpp"
#include "game/physicsSystem.hpp"

#include "keyboardXX.hpp"

void InputNetworkForwarder::buildPackets(Session& session) {
    CSInput curPacket{};
    
    for (const auto& ev : inputEvents_) {
        curPacket.events[curPacket.eventCnt++] = ev;
        if (curPacket.eventCnt >= CSInput::maxEventCnt) {
            session.enqueuePacket(Packet{
                .size = calcPacketSize<CSInput>(CSInput::maxEventCnt),
                .type = PacketType::CSInput,
                .csInput = curPacket
            });
            curPacket.eventCnt = 0;
        }
    }

    if (curPacket.eventCnt > 0) {
        session.enqueuePacket(Packet{
            .size = calcPacketSize<CSInput>(curPacket.eventCnt),
            .type = PacketType::CSInput,
            .csInput = curPacket
        });
    }

    inputEvents_.clear();
}

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

void NetworkInputHandler::handleEvent( CInputEvent event,
    float floatVal0, float floatVal1, const Win32::WndClient& client,
    ControllerAdapters& controllerAdapters
) {
    // Moving: floatVal0 = deltaTime, floatVal1 = None
    // Rotation: floatVal0 = yawValue, floatVal1 = None
    switch (event) {
    case CInputEvent::MoveForward:
        controllerAdapters.inputNetworkForwarder.pushInputEvent(
            InputEvent{ InputEventType::MoveForward, floatVal0, floatVal1 }
        );
        break;
    case CInputEvent::MoveBackward:
        controllerAdapters.inputNetworkForwarder.pushInputEvent(
            InputEvent{ InputEventType::MoveBackward, floatVal0, floatVal1 }
        );
        break;
    case CInputEvent::MoveLeft:
        controllerAdapters.inputNetworkForwarder.pushInputEvent(
            InputEvent{ InputEventType::MoveLeft, floatVal0, floatVal1 }
        );
        break;
    case CInputEvent::MoveRight:
        controllerAdapters.inputNetworkForwarder.pushInputEvent(
            InputEvent{ InputEventType::MoveRight, floatVal0, floatVal1 }
        );
        break;
    case CInputEvent::Rotation: {
        const auto mouseSensitivity = mu::pi * 2.f;
        controllerAdapters.inputNetworkForwarder.pushInputEvent(
            InputEvent{ InputEventType::Rotation,
                floatVal0 * mouseSensitivity / static_cast<float>(client.width),
                0.f
            }
        );
        break;
    }
        
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

void StandAloneInputHandler::moveForward(float deltaTimeSec) {
    addMomentum( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(2u) ) ) * forceStep_ * deltaTimeSec  );
}

void StandAloneInputHandler::moveBackward(float deltaTimeSec) {
    addMomentum( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(2u) ) ) * -forceStep_ * 0.45f * deltaTimeSec  );
}

void StandAloneInputHandler::moveLeft(float deltaTimeSec) {
    addMomentum( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(0u) ) ) * -forceStep_ * deltaTimeSec  );
}

void StandAloneInputHandler::moveRight(float deltaTimeSec) {
    addMomentum( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(0u) ) ) * forceStep_ * deltaTimeSec  );
}

void StandAloneInputHandler::moveUp(float deltaTimeSec) {
    addMomentum( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(1u) ) ) * forceStep_ * 0.55f * deltaTimeSec  );
}

void StandAloneInputHandler::moveDown(float deltaTimeSec) {
    addMomentum( mu::Vec3( mu::NVec3( static_cast<const gfx::d3d12engine::Model*>(
        ecs::Component::atC(ecs::Components::Model, entityId_)
    )->get().root()->coord().localXform().row(1u) ) ) * -forceStep_ * 0.55f * deltaTimeSec  );
}


void MU_CALLCONV StandAloneInputHandler::addMomentum(mu::Vec3 momentum) {
    auto pRigidBody = RigidBody::at(entityId_);
    if (!pRigidBody) {
        throw ECS_EXCEPT("RigidBody component doesn't exist");
    }

    pRigidBody->accMomentum(momentum);
}

void StandAloneInputHandler::yaw(float yawValue, const Win32::WndClient& client) {
    auto pCoord = gameEngine::Coord::at(entityId_);
    if (!pCoord) {
        throw ECS_EXCEPT("Coord component doesn't exist");
    }

    const auto mouseSensitivity = mu::pi * 2.f;

    pCoord->accRotation(
        mu::NQuat(mu::quatRotMat(mu::rotateY(
            mu::Radian(yawValue * mouseSensitivity / static_cast<float>(client.width))
        )))
    );
}

void InputSystem::update( MilliSeconds deltaTime, const Win32::WndClient& client,
    ControllerAdapters& controllerAdapters
) {
	pKeyboard_->patchKeyState();

    for (auto& playerController : components<PlayerController>()) {
        if (!playerController) {
            throw ECS_EXCEPT("PlayerController component is not valid");
        }

        for (const auto& [key, event] : keyMap_) {
            if (pKeyboard_->pressed(key)) {
                const auto deltaTimeSec = std::chrono::duration_cast<Seconds>(deltaTime).count();
                playerController->handleEvent(event, deltaTimeSec, 0.f, client, controllerAdapters);
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