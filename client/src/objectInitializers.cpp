#include "objectInitializers.hpp"

#include "game/physicsSystem.hpp"
#include "game/animSystem.hpp"

#include "assetMap.hpp"

#include <optional>

// move lower bound
inline constexpr auto characterMoveLb2 = 0.12f * 0.12f;
// walk upper bound
inline constexpr auto characterWalkUb2 = 1.6f * 1.6f;
inline constexpr auto characterRunUb2 = 10.f * 10.f;

// this function finds the appropriate animation clip from velocity,
// with the state name followed by the direction suffix.
// like:
// {state name}+"ForwardLeft|Forward|ForwardRight|Left|Right|BackwardLeft|Backward|BackwardRight"
void __8moveTransition(std::string_view fromState, std::string_view toState,
    MilliSeconds fadeDuration, AnimInstance::ClipMode clipMode,
    AnimController& con) {
    const auto entityId = con.entityID().value();
    const auto rigidBody = RigidBody::atC(entityId);
    if (!rigidBody) {
        return;
    }
    const auto velocity = rigidBody->velocity();
    const auto model = gfx::d3d12engine::Model::atC(entityId);
    const auto forward = mu::Vec3(model->get().root()->coord().xform().row(2u));
    const auto look = mu::quatFromTo(forward, mu::Vec3(0.f, 0.f, 1.f)).rotate(velocity);

    struct __Direction {
        std::string_view name;
        mu::Vec3 direction;
    };

    static const std::vector<__Direction> directions = {
        {"Forward",       mu::Vec3( 0.f, 0.f, 1.f )},
        {"ForwardRight",  mu::NVec3( 1.f, 0.f, 1.f )},
        {"Right",         mu::Vec3( 1.f, 0.f, 0.f )},
        {"BackwardRight", mu::NVec3( 1.f, 0.f, -1.f )},
        {"Backward",      mu::Vec3( 0.f, 0.f, -1.f )},
        {"BackwardLeft",  mu::NVec3( 1.f, 0.f, -1.f)},
        {"Left",          mu::Vec3( -1.f, 0.f, 0.f )},
        {"ForwardLeft",   mu::NVec3( 1.f, 0.f, 1.f )}
    };

    auto possiblePrevKeys = std::vector<std::string>();
    possiblePrevKeys.reserve(directions.size() + 1u);   // + 1u for additional current key
    for (const auto& dir : directions) {
        possiblePrevKeys.push_back(std::string(fromState) + dir.name.data());
    }

    auto curKey = std::string(toState)
        + std::ranges::max_element(
            directions, {}, [&](const __Direction& dir) {
                return mu::dot(look, dir.direction);
            }
        )->name.data();

    // reuse the possiblePrevKeys vector for optimization
    possiblePrevKeys.push_back(curKey);

    con.restoreAnimSequences(possiblePrevKeys);
    possiblePrevKeys.pop_back(); // remove the last element
    fadeOutSelect(possiblePrevKeys, fadeDuration, con);
    fadeIn( curKey, possiblePrevKeys, fadeDuration, con, clipMode );
}

// this function finds the appropriate animation clip from velocity,
// with the state name followed by the direction suffix.
// like:
// {state name}+"ForwardLeft|Forward|ForwardRight|Left|Right|BackwardLeft|Backward|BackwardRight"
void __8moveTransition(std::vector<std::string> possiblePrevKeys, std::string_view toState,
    MilliSeconds fadeDuration, AnimInstance::ClipMode clipMode,
    AnimController& con) {
    const auto entityId = con.entityID().value();
    const auto rigidBody = RigidBody::atC(entityId);
    if (!rigidBody) {
        return;
    }
    const auto velocity = rigidBody->velocity();
    const auto model = gfx::d3d12engine::Model::atC(entityId);
    const auto forward = mu::Vec3(model->get().root()->coord().xform().row(2u));
    const auto look = mu::quatFromTo(forward, mu::Vec3(0.f, 0.f, 1.f)).rotate(velocity);

    struct __Direction {
        std::string_view name;
        mu::Vec3 direction;
    };

    static const std::vector<__Direction> directions = {
        {"Forward",       mu::Vec3( 0.f, 0.f, 1.f )},
        {"ForwardRight",  mu::NVec3( 1.f, 0.f, 1.f )},
        {"Right",         mu::Vec3( 1.f, 0.f, 0.f )},
        {"BackwardRight", mu::NVec3( 1.f, 0.f, -1.f )},
        {"Backward",      mu::Vec3( 0.f, 0.f, -1.f )},
        {"BackwardLeft",  mu::NVec3( 1.f, 0.f, -1.f)},
        {"Left",          mu::Vec3( -1.f, 0.f, 0.f )},
        {"ForwardLeft",   mu::NVec3( 1.f, 0.f, 1.f )}
    };

    auto curKey = std::string(toState)
        + std::ranges::max_element(
            directions, {}, [&](const __Direction& dir) {
                return mu::dot(look, dir.direction);
            }
        )->name.data();

    // reuse the possiblePrevKeys vector for optimization
    possiblePrevKeys.push_back(curKey);

    con.restoreAnimSequences(possiblePrevKeys);
    possiblePrevKeys.pop_back(); // remove the last element
    fadeOutSelect(possiblePrevKeys, fadeDuration, con);
    fadeIn( std::move(curKey), std::move(possiblePrevKeys), fadeDuration, con, clipMode );
}

std::vector<std::string> __8movePossibleKeys(std::string_view fromState) {
    struct __Direction {
        std::string_view name;
        mu::Vec3 direction;
    };

    static const std::vector<__Direction> directions = {
        {"Forward",       mu::Vec3( 0.f, 0.f, 1.f )},
        {"ForwardRight",  mu::NVec3( 1.f, 0.f, 1.f )},
        {"Right",         mu::Vec3( 1.f, 0.f, 0.f )},
        {"BackwardRight", mu::NVec3( 1.f, 0.f, -1.f )},
        {"Backward",      mu::Vec3( 0.f, 0.f, -1.f )},
        {"BackwardLeft",  mu::NVec3( 1.f, 0.f, -1.f)},
        {"Left",          mu::Vec3( -1.f, 0.f, 0.f )},
        {"ForwardLeft",   mu::NVec3( 1.f, 0.f, 1.f )}
    };

    auto possiblePrevKeys = std::vector<std::string>();
    possiblePrevKeys.reserve(directions.size() + 1u);   // + 1u for additional current key
    for (const auto& dir : directions) {
        possiblePrevKeys.push_back(std::string(fromState) + dir.name.data());
    }
    return possiblePrevKeys;
}

void characterStateIdleUpdate(fsm::FSM& fsm, AnimController& con, MilliSeconds deltaTime) {
    const auto entityId = con.entityID().value();
    const auto rigidBody = RigidBody::atC(entityId);
    if (!rigidBody) {
        return;
    }
    const auto velocity = rigidBody->velocity();
    const auto speed2 = velocity.len2();
    
    if (speed2 >= characterMoveLb2) {
        if (speed2 < characterWalkUb2) {
            fsm.pushDeferredEvent(fsm::Event::transition("Idle", "Walk"));
        }
        else if (speed2 < characterRunUb2) {
            fsm.pushDeferredEvent(fsm::Event::transition("Idle", "Run"));
        }
        else {
            fsm.pushDeferredEvent(fsm::Event::transition("Idle", "Sprint"));
        }
    }
}

fsm::State characterStateIdle(fsm::FSM& fsm, AnimController& con) {
    for (;;) {
        while (auto events = co_await fsm.getEvents()) {
            while (auto ev = events.pop()) {
                if (ev->evType() == AnimController::evAnimUpdate) {
                    characterStateIdleUpdate(
                        fsm, con, ev->get<MilliSeconds>()
                    );
                }
                // do something
            }
        }
    
        co_await fsm.completeStateUpdate();
    }
}

void characterStateWalkUpdate(fsm::FSM& fsm, AnimController& con, MilliSeconds deltaTime) {
    const auto entityId = con.entityID().value();
    const auto rigidBody = RigidBody::atC(entityId);
    if (!rigidBody) {
        fsm.pushDeferredEvent(fsm::Event::transition("Walk", "Idle"));
        return;
    }
    const auto velocity = rigidBody->velocity();
    const auto speed2 = velocity.len2();

    if (speed2 >= characterMoveLb2) {
        if (speed2 >= characterWalkUb2) {
            fsm.pushDeferredEvent(fsm::Event::transition("Walk", "Run"));
        }
        else if (speed2 >= characterRunUb2) {
            fsm.pushDeferredEvent(fsm::Event::transition("Walk", "Sprint"));
        }
    }
    else  {
        fsm.pushDeferredEvent(fsm::Event::transition("Walk", "Idle"));
    }
}

fsm::State characterStateWalk(fsm::FSM& fsm, AnimController& con) {
    for (;;) {
        while (auto events = co_await fsm.getEvents()) {
            while (auto ev = events.pop()) {
                if (ev->evType() == AnimController::evAnimUpdate) {
                    characterStateWalkUpdate(
                        fsm, con, ev->get<MilliSeconds>()
                    );
                }
                // do something
            }
        }
    
        co_await fsm.completeStateUpdate();
    }
}

void characterStateRunUpdate(fsm::FSM& fsm, AnimController& con, MilliSeconds deltaTime) {
    const auto entityId = con.entityID().value();
    const auto rigidBody = RigidBody::atC(entityId);
    if (!rigidBody) {
        fsm.pushDeferredEvent(fsm::Event::transition("Run", "Idle"));
        return;
    }
    const auto velocity = rigidBody->velocity();
    const auto speed2 = velocity.len2();

    if (speed2 >= characterMoveLb2) {
        if (speed2 < characterWalkUb2) {
            fsm.pushDeferredEvent(fsm::Event::transition("Run", "Walk"));
        }
        else if (speed2 >= characterRunUb2) {
            fsm.pushDeferredEvent(fsm::Event::transition("Run", "Sprint"));
        }
    }
    else {
        fsm.pushDeferredEvent(fsm::Event::transition("Run", "Idle"));
    }
}

fsm::State characterStateRun(fsm::FSM& fsm, AnimController& con) {
    for (;;) {
        while (auto events = co_await fsm.getEvents()) {
            while (auto ev = events.pop()) {
                if (ev->evType() == AnimController::evAnimUpdate) {
                    characterStateRunUpdate(
                        fsm, con, ev->get<MilliSeconds>()
                    );
                }
                // do something
            }
        }
    
        co_await fsm.completeStateUpdate();
    }
}

void characterStateSprintUpdate(fsm::FSM& fsm, AnimController& con, MilliSeconds deltaTime) {
    const auto entityId = con.entityID().value();
    const auto rigidBody = RigidBody::atC(entityId);
    if (!rigidBody) {
        fsm.pushDeferredEvent(fsm::Event::transition("Sprint", "Idle"));
        return;
    }
    const auto velocity = rigidBody->velocity();
    const auto speed2 = velocity.len2();

    if (speed2 >= characterMoveLb2) {
        if (speed2 < characterWalkUb2) {
            fsm.pushDeferredEvent(fsm::Event::transition("Sprint", "Walk"));
        }
        else if (speed2 < characterRunUb2) {
            fsm.pushDeferredEvent(fsm::Event::transition("Sprint", "Run"));
        }
    }
    else {
        fsm.pushDeferredEvent(fsm::Event::transition("Sprint", "Idle"));
    }
}

fsm::State characterStateSprint(fsm::FSM& fsm, AnimController& con) {
    for (;;) {
        while (auto events = co_await fsm.getEvents()) {
            while (auto ev = events.pop()) {
                if (ev->evType() == AnimController::evAnimUpdate) {
                    characterStateSprintUpdate(
                        fsm, con, ev->get<MilliSeconds>()
                    );
                }
                // do something
            }
        }
    
        co_await fsm.completeStateUpdate();
    }
}

void initAnimationsCharacter(
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip,
    AnimController& animCon
) {
    const auto& animClipSlot = resStorage.slot(slotKeyAnimClip);

    animCon.addClip("GO_Character_Idle",
        animClipSlot.get<AnimClip>("GO_Character_Idle")
    );
    animCon.addClip("GO_Character_Idle1",
        animClipSlot.get<AnimClip>("GO_Character_Idle1")
    );
    animCon.addClip("GO_Character_Idle2",
        animClipSlot.get<AnimClip>("GO_Character_Idle2")
    );
    animCon.addClip("GO_Character_WalkForward",
        animClipSlot.get<AnimClip>("GO_Character_WalkForward")
    );
    animCon.addClip("GO_Character_RunForward",
        animClipSlot.get<AnimClip>("GO_Character_RunForward")
    );
    animCon.addClip("GO_Character_SprintForward",
        animClipSlot.get<AnimClip>("GO_Character_SprintForward")
    );
    animCon.addClip("GO_Character_WalkBackward",
        animClipSlot.get<AnimClip>("GO_Character_WalkBackward")
    );
    animCon.addClip("GO_Character_RunBackward",
        animClipSlot.get<AnimClip>("GO_Character_RunBackward")
    );
    animCon.addClip("GO_Character_SprintBackward",
        animClipSlot.get<AnimClip>("GO_Character_SprintBackward")
    );
    animCon.addClip("GO_Character_WalkLeft",
        animClipSlot.get<AnimClip>("GO_Character_WalkLeft")
    );
    animCon.addClip("GO_Character_RunLeft",
        animClipSlot.get<AnimClip>("GO_Character_RunLeft")
    );
    animCon.addClip("GO_Character_SprintLeft",
        animClipSlot.get<AnimClip>("GO_Character_SprintLeft")
    );
    animCon.addClip("GO_Character_WalkRight",
        animClipSlot.get<AnimClip>("GO_Character_WalkRight")
    );
    animCon.addClip("GO_Character_RunRight",
        animClipSlot.get<AnimClip>("GO_Character_RunRight")
    );
    animCon.addClip("GO_Character_SprintRight",
        animClipSlot.get<AnimClip>("GO_Character_SprintRight")
    );
    animCon.addClip("GO_Character_WalkForwardLeft",
        animClipSlot.get<AnimClip>("GO_Character_WalkForwardLeft")
    );
    animCon.addClip("GO_Character_RunForwardLeft",
        animClipSlot.get<AnimClip>("GO_Character_RunForwardLeft")
    );
    animCon.addClip("GO_Character_SprintForwardLeft",
        animClipSlot.get<AnimClip>("GO_Character_SprintForwardLeft")
    );
    animCon.addClip("GO_Character_WalkForwardRight",
        animClipSlot.get<AnimClip>("GO_Character_WalkForwardRight")
    );
    animCon.addClip("GO_Character_RunForwardRight",
        animClipSlot.get<AnimClip>("GO_Character_RunForwardRight")
    );
    animCon.addClip("GO_Character_SprintForwardRight",
        animClipSlot.get<AnimClip>("GO_Character_SprintForwardRight")
    );
    animCon.addClip("GO_Character_WalkBackwardLeft",
        animClipSlot.get<AnimClip>("GO_Character_WalkBackwardLeft")
    );
    animCon.addClip("GO_Character_RunBackwardLeft",
        animClipSlot.get<AnimClip>("GO_Character_RunBackwardLeft")
    );
    animCon.addClip("GO_Character_SprintBackwardLeft",
        animClipSlot.get<AnimClip>("GO_Character_SprintBackwardLeft")
    );
    animCon.addClip("GO_Character_WalkBackwardRight",
        animClipSlot.get<AnimClip>("GO_Character_WalkBackwardRight")
    );
    animCon.addClip("GO_Character_RunBackwardRight",
        animClipSlot.get<AnimClip>("GO_Character_RunBackwardRight")
    );
    animCon.addClip("GO_Character_SprintBackwardRight",
        animClipSlot.get<AnimClip>("GO_Character_SprintBackwardRight")
    );


    animCon.fsm().addState("Idle", characterStateIdle, animCon);
    animCon.fsm().addState("Walk", characterStateWalk, animCon);
    animCon.fsm().addState("Run", characterStateRun, animCon);
    animCon.fsm().addState("Sprint", characterStateSprint, animCon);
    animCon.fsm().start("Idle");

    animCon.fsm().addTransition("Idle", "Idle", [](){});
    animCon.fsm().addTransition("Walk", "Walk", [](){});
    animCon.fsm().addTransition("Run", "Run", [](){});
    animCon.fsm().addTransition("Sprint", "Sprint", [](){});

    animCon.fsm().addTransition("Idle", "Walk", [&animCon](){
        __8moveTransition({
            "GO_Character_Idle",
            "GO_Character_Idle1",
            "GO_Character_Idle2",
            "GO_Character_Walk"
        }, "GO_Character_Walk",
            360_ms, AnimInstance::ClipMode::KeyFrame, animCon
        );
    });
    animCon.fsm().addTransition("Idle", "Run", [&animCon](){
        __8moveTransition({
            "GO_Character_Idle",
            "GO_Character_Idle1",
            "GO_Character_Idle2",
            "GO_Character_Run"
        }, "GO_Character_Run",
            360_ms, AnimInstance::ClipMode::KeyFrame, animCon
        );
    });
    animCon.fsm().addTransition("Idle", "Sprint", [&animCon](){
        __8moveTransition({
            "GO_Character_Idle",
            "GO_Character_Idle1",
            "GO_Character_Idle2",
            "GO_Character_Sprint"
        }, "GO_Character_Sprint",
            360_ms, AnimInstance::ClipMode::KeyFrame, animCon
        );
    });

    animCon.fsm().addTransition("Walk", "Idle", [&animCon](){
        auto possiblePrevKeys = __8movePossibleKeys("GO_Character_Walk");
        possiblePrevKeys.push_back("GO_Character_Idle");
        possiblePrevKeys.push_back("GO_Character_Idle1");
        possiblePrevKeys.push_back("GO_Character_Idle2");

        animCon.restoreAnimSequences(possiblePrevKeys);

        possiblePrevKeys.pop_back();
        possiblePrevKeys.pop_back();
        possiblePrevKeys.pop_back();

        softCircular( { "GO_Character_Idle",
                "GO_Character_Idle1",
                "GO_Character_Idle2"
            }, std::move(possiblePrevKeys), 360_ms, animCon,
            AnimInstance::ClipMode::KeyFrame
        );
    });
    animCon.fsm().addTransition("Walk", "Run", [&animCon](){
        __8moveTransition("GO_Character_Walk",
            "GO_Character_Run", 360_ms,
            AnimInstance::ClipMode::KeyFrame, animCon
        );
    });
    animCon.fsm().addTransition("Walk", "Sprint", [&animCon](){
        __8moveTransition("GO_Character_Walk",
            "GO_Character_Sprint", 360_ms,
            AnimInstance::ClipMode::KeyFrame, animCon
        );
    });

    animCon.fsm().addTransition("Run", "Idle", [&animCon](){
        auto possiblePrevKeys = __8movePossibleKeys("GO_Character_Run");
        possiblePrevKeys.push_back("GO_Character_Idle");
        possiblePrevKeys.push_back("GO_Character_Idle1");
        possiblePrevKeys.push_back("GO_Character_Idle2");

        animCon.restoreAnimSequences(possiblePrevKeys);

        possiblePrevKeys.pop_back();
        possiblePrevKeys.pop_back();
        possiblePrevKeys.pop_back();

        softCircular( { "GO_Character_Idle",
                "GO_Character_Idle1",
                "GO_Character_Idle2"
            }, std::move(possiblePrevKeys), 360_ms, animCon,
            AnimInstance::ClipMode::KeyFrame
        );
    });
    animCon.fsm().addTransition("Run", "Walk", [&animCon](){
        __8moveTransition("GO_Character_Run",
            "GO_Character_Walk", 360_ms,
            AnimInstance::ClipMode::KeyFrame, animCon
        );
    });
    animCon.fsm().addTransition("Run", "Sprint", [&animCon](){
        __8moveTransition("GO_Character_Run",
            "GO_Character_Sprint", 220_ms,
            AnimInstance::ClipMode::KeyFrame, animCon
        );
    });

    animCon.fsm().addTransition("Sprint", "Idle", [&animCon](){
        auto possiblePrevKeys = __8movePossibleKeys("GO_Character_Sprint");
        possiblePrevKeys.push_back("GO_Character_Idle");
        possiblePrevKeys.push_back("GO_Character_Idle1");
        possiblePrevKeys.push_back("GO_Character_Idle2");

        animCon.restoreAnimSequences(possiblePrevKeys);

        possiblePrevKeys.pop_back();
        possiblePrevKeys.pop_back();
        possiblePrevKeys.pop_back();

        softCircular( { "GO_Character_Idle",
                "GO_Character_Idle1",
                "GO_Character_Idle2"
            }, std::move(possiblePrevKeys), 360_ms, animCon,
            AnimInstance::ClipMode::KeyFrame
        );
    });
    animCon.fsm().addTransition("Sprint", "Walk", [&animCon](){
        __8moveTransition("GO_Character_Sprint",
            "GO_Character_Walk", 360_ms,
            AnimInstance::ClipMode::KeyFrame, animCon
        );
    });
    animCon.fsm().addTransition("Sprint", "Run", [&animCon](){
        __8moveTransition("GO_Character_Sprint",
            "GO_Character_Run", 220_ms,
            AnimInstance::ClipMode::KeyFrame, animCon
        );
    });

    animCon.play("GO_Character_Idle", 0_ms, AnimInstance::ClipMode::KeyFrame);

    softCircular( {
            "GO_Character_Idle", "GO_Character_Idle1", "GO_Character_Idle2"
        }, "GO_Character_Idle", 360_ms, animCon,
        AnimInstance::ClipMode::KeyFrame
    );
}

void initAnimations(
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip,
    AssetModel assetModel, AnimController& animCon
) {
    const auto& animClipSlot = resStorage.slot(slotKeyAnimClip);

    switch (assetModel) {
    case AssetModel::Character:
        initAnimationsCharacter(resStorage, slotKeyAnimClip, animCon);
        break;

    default:
        break;
    }
}

void buildEntityWithAsset( mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyModel,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyBVHPath,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip,
    std::optional<AssetModel> assetModel,
    std::optional<AssetBVH> assetBVH,
    ecs::Entity& entity
) {
    entity.createComponent<gameEngine::Coord>();
    entity.as<gameEngine::Coord>().get().setLocalXform(mu::translate(translation));

    auto& modelSlot = resStorage.slot(slotKeyModel);
    auto& bvhPathSlot = resStorage.slot(slotKeyBVHPath);

    if (assetModel.has_value()) {
        const auto asset = assetModel.value();
        const auto key = assetModelInfo(asset).key;
        bool hasAnimation = !assetModelInfo(asset).animationPath.empty();

        if (!modelSlot.contains<gfx::d3d12::RefModel>(key)) {
            throw GFX_EXCEPT("Model not found: " + key);
        }

        entity.createComponent<gfx::d3d12engine::Model>(modelSlot, key, entity.as<gameEngine::Coord>());
        if (hasAnimation) {
            entity.createComponent<AnimController>(std::to_string(entity.id().value()));
            entity.as<AnimController>().setSkeleton(
                entity.as<gfx::d3d12engine::Model>().get().refModel()->skeleton()
            );
            initAnimations(resStorage, slotKeyAnimClip, assetModel.value(), entity.as<AnimController>());
        }
        entity.as<gfx::d3d12engine::Model>().get().root()->coord() << mu::Mat4x4(rotation);
    }

    if (assetBVH.has_value()) {
        const auto asset = assetBVH.value();
        const auto& key = assetBVHInfo(asset).key;

        if (!bvhPathSlot.contains<std::filesystem::path>(key)) {
            throw GFX_EXCEPT("BVH path not found: " + key);
        }

        // entity.createComponent<BoundingVolume>(*bvhPathSlot.get<std::filesystem::path>(key));
    }
}

ecs::Entity MU_CALLCONV createCharacter(
    mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyModel,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyBVHPath,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip
) {
    auto entity = ecs::Entity();
    buildEntityWithAsset(translation, rotation, resStorage,
        slotKeyModel, slotKeyBVHPath, slotKeyAnimClip,
        AssetModel::Character, {}, entity
    );
    entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
        gfx::d3d12engine::rp::PBRAnimatedIllumination::id
    );
    entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
        gfx::d3d12engine::rp::CascadeShadowMapAnimated::id
    );
    return entity;
}

ecs::Entity MU_CALLCONV createHelicopter(
    mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyModel,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyBVHPath,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip
) {
    auto entity = ecs::Entity();
    buildEntityWithAsset(translation, rotation, resStorage,
        slotKeyModel, slotKeyBVHPath, slotKeyAnimClip,
        AssetModel::Helicopter, {}, entity
    );
    entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
        gfx::d3d12engine::rp::PBRIllumination::id
    );
    entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
        gfx::d3d12engine::rp::CascadeShadowMap::id
    );
    return entity;
}

ecs::Entity MU_CALLCONV createTree0(
    mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyModel,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyBVHPath,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip
) {
    auto entity = ecs::Entity();
    buildEntityWithAsset(translation, rotation, resStorage,
        slotKeyModel, slotKeyBVHPath, slotKeyAnimClip,
        AssetModel::Tree0, {}, entity
    );
    entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
        gfx::d3d12engine::rp::PBRIllumination::id
    );
    entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
        gfx::d3d12engine::rp::CascadeShadowMap::id
    );
    return entity;
}

ecs::Entity MU_CALLCONV createTree1(
    mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyModel,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyBVHPath,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip
) {
    auto entity = ecs::Entity();
    buildEntityWithAsset(translation, rotation, resStorage,
        slotKeyModel, slotKeyBVHPath, slotKeyAnimClip,
        AssetModel::Tree1, {}, entity
    );
    entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
        gfx::d3d12engine::rp::PBRIllumination::id
    );
    entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
        gfx::d3d12engine::rp::CascadeShadowMap::id
    );
    return entity;
}

ecs::Entity MU_CALLCONV createTree2(
    mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyModel,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyBVHPath,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip
) {
    auto entity = ecs::Entity();
    buildEntityWithAsset(translation, rotation, resStorage,
        slotKeyModel, slotKeyBVHPath, slotKeyAnimClip,
        AssetModel::Tree2, {}, entity
    );
    entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
        gfx::d3d12engine::rp::PBRIllumination::id
    );
    entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
        gfx::d3d12engine::rp::CascadeShadowMap::id
    );
    return entity;
}