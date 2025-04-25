#include "objectInitializers.hpp"

#include "game/physicsSystem.hpp"
#include "game/animSystem.hpp"

#include "assetMap.hpp"

#include <optional>

// move lower bound
inline constexpr auto characterMoveLb2 = 0.04f * 0.04f;
// walk upper bound
inline constexpr auto characterWalkUb2 = 1.6f * 1.6f;
inline constexpr auto characterRunUb2 = 10.f * 10.f;

void characterStateIdleUpdate(fsm::FSM& fsm, AnimController& con, Milliseconds deltaTime) {
    const auto entityId = con.entityID().value();
    const auto rigidBody = RigidBody::atC(entityId);
    if (!rigidBody) {
        return;
    }
    const auto velocity = rigidBody->velocity();
    const auto speed2 = velocity.len2();

    if (speed2 > characterMoveLb2) {
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
                        fsm, con, ev->get<Milliseconds>()
                    );
                }
                // do something
            }
        }
    
        co_await fsm.completeStateUpdate();
    }
}

void characterStateWalkUpdate(fsm::FSM& fsm, AnimController& con, Milliseconds deltaTime) {
    const auto entityId = con.entityID().value();
    const auto rigidBody = RigidBody::atC(entityId);
    if (!rigidBody) {
        fsm.pushDeferredEvent(fsm::Event::transition("Walk", "Idle"));
        return;
    }
    const auto velocity = rigidBody->velocity();
    const auto speed2 = velocity.len2();

    if (speed2 < characterMoveLb2) {
        fsm.pushDeferredEvent(fsm::Event::transition("Walk", "Idle"));
    }
    else if (speed2 >= characterWalkUb2) {
        if (speed2 < characterRunUb2) {
            fsm.pushDeferredEvent(fsm::Event::transition("Walk", "Run"));
        }
        else {
            fsm.pushDeferredEvent(fsm::Event::transition("Walk", "Sprint"));
        }
    }
}

fsm::State characterStateWalk(fsm::FSM& fsm, AnimController& con) {
    for (;;) {
        while (auto events = co_await fsm.getEvents()) {
            while (auto ev = events.pop()) {
                if (ev->evType() == AnimController::evAnimUpdate) {
                    characterStateWalkUpdate(
                        fsm, con, ev->get<Milliseconds>()
                    );
                }
                // do something
            }
        }
    
        co_await fsm.completeStateUpdate();
    }
}

void characterStateRunUpdate(fsm::FSM& fsm, AnimController& con, Milliseconds deltaTime) {
    const auto entityId = con.entityID().value();
    const auto rigidBody = RigidBody::atC(entityId);
    if (!rigidBody) {
        fsm.pushDeferredEvent(fsm::Event::transition("Run", "Idle"));
        return;
    }
    const auto velocity = rigidBody->velocity();
    const auto speed2 = velocity.len2();

    if (speed2 < characterMoveLb2) {
        fsm.pushDeferredEvent(fsm::Event::transition("Run", "Idle"));
    }
    else if (speed2 < characterWalkUb2) {
        fsm.pushDeferredEvent(fsm::Event::transition("Run", "Walk"));
    }
    else if (speed2 >= characterRunUb2) {
        fsm.pushDeferredEvent(fsm::Event::transition("Run", "Sprint"));
    }
}

fsm::State characterStateRun(fsm::FSM& fsm, AnimController& con) {
    for (;;) {
        while (auto events = co_await fsm.getEvents()) {
            while (auto ev = events.pop()) {
                if (ev->evType() == AnimController::evAnimUpdate) {
                    characterStateRunUpdate(
                        fsm, con, ev->get<Milliseconds>()
                    );
                }
                // do something
            }
        }
    
        co_await fsm.completeStateUpdate();
    }
}

void characterStateSprintUpdate(fsm::FSM& fsm, AnimController& con, Milliseconds deltaTime) {
    const auto entityId = con.entityID().value();
    const auto rigidBody = RigidBody::atC(entityId);
    if (!rigidBody) {
        fsm.pushDeferredEvent(fsm::Event::transition("Sprint", "Idle"));
        return;
    }
    const auto velocity = rigidBody->velocity();
    const auto speed2 = velocity.len2();

    if (speed2 < characterMoveLb2) {
        fsm.pushDeferredEvent(fsm::Event::transition("Sprint", "Idle"));
    }
    else if (speed2 < characterWalkUb2) {
        fsm.pushDeferredEvent(fsm::Event::transition("Sprint", "Walk"));
    }
    else if (speed2 < characterRunUb2) {
        fsm.pushDeferredEvent(fsm::Event::transition("Sprint", "Run"));
    }
}

fsm::State characterStateSprint(fsm::FSM& fsm, AnimController& con) {
    for (;;) {
        while (auto events = co_await fsm.getEvents()) {
            while (auto ev = events.pop()) {
                if (ev->evType() == AnimController::evAnimUpdate) {
                    characterStateSprintUpdate(
                        fsm, con, ev->get<Milliseconds>()
                    );
                }
                // do something
            }
        }
    
        co_await fsm.completeStateUpdate();
    }
}

void initAnimations(
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip,
    AssetModel assetModel, AnimController& animCon
) {
    const auto& animClipSlot = resStorage.slot(slotKeyAnimClip);

    switch (assetModel) {
    case AssetModel::Character:
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
            fadeIn("GO_Character_Walk", 500_ms, animCon);
            fadeOut("GO_Character_Idle", 500_ms, animCon);
        });
        animCon.fsm().addTransition("Idle", "Run", [&animCon](){
            fadeIn("GO_Character_Run", 500_ms, animCon);
            fadeOut("GO_Character_Idle", 500_ms, animCon);
        });
        animCon.fsm().addTransition("Idle", "Sprint", [&animCon](){
            fadeIn("GO_Character_Sprint", 500_ms, animCon);
            fadeOut("GO_Character_Idle", 500_ms, animCon);
        });

        animCon.fsm().addTransition("Walk", "Idle", [&animCon](){
            fadeIn("GO_Character_Idle", 500_ms, animCon);
            fadeOut("GO_Character_Walk", 500_ms, animCon);
        });
        animCon.fsm().addTransition("Walk", "Run", [&animCon](){
            fadeIn("GO_Character_Run", 500_ms, animCon);
            fadeOut("GO_Character_Walk", 500_ms, animCon);
        });
        animCon.fsm().addTransition("Walk", "Sprint", [&animCon](){
            fadeIn("GO_Character_Sprint", 500_ms, animCon);
            fadeOut("GO_Character_Walk", 500_ms, animCon);
        });

        animCon.fsm().addTransition("Run", "Idle", [&animCon](){
            fadeIn("GO_Character_Idle", 500_ms, animCon);
            fadeOut("GO_Character_Run", 500_ms, animCon);
        });
        animCon.fsm().addTransition("Run", "Walk", [&animCon](){
            fadeIn("GO_Character_Walk", 500_ms, animCon);
            fadeOut("GO_Character_Run", 500_ms, animCon);
        });
        animCon.fsm().addTransition("Run", "Sprint", [&animCon](){
            fadeIn("GO_Character_Sprint", 500_ms, animCon);
            fadeOut("GO_Character_Run", 500_ms, animCon);
        });

        animCon.fsm().addTransition("Sprint", "Idle", [&animCon](){
            fadeIn("GO_Character_Idle", 500_ms, animCon);
            fadeOut("GO_Character_Sprint", 500_ms, animCon);
        });
        animCon.fsm().addTransition("Sprint", "Walk", [&animCon](){
            fadeIn("GO_Character_Walk", 500_ms, animCon);
            fadeOut("GO_Character_Sprint", 500_ms, animCon);
        });
        animCon.fsm().addTransition("Sprint", "Run", [&animCon](){
            fadeIn("GO_Character_Run", 500_ms, animCon);
            fadeOut("GO_Character_Sprint", 500_ms, animCon);
        });

        animCon.addClip("GO_Character_Idle",
            animClipSlot.get<AnimClip>("GO_Character_Idle")
        );
        animCon.addClip("GO_Character_Idle1",
            animClipSlot.get<AnimClip>("GO_Character_Idle1")
        );
        animCon.addClip("GO_Character_Idle2",
            animClipSlot.get<AnimClip>("GO_Character_Idle2")
        );
        animCon.addClip("GO_Character_Walk",
            animClipSlot.get<AnimClip>("GO_Character_Walk")
        );
        animCon.addClip("GO_Character_Run",
            animClipSlot.get<AnimClip>("GO_Character_Run")
        );
        animCon.addClip("GO_Character_Sprint",
            animClipSlot.get<AnimClip>("GO_Character_Sprint")
        );

        animCon.play("GO_Character_Idle");
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

        entity.createComponent<BoundingVolume>(*bvhPathSlot.get<std::filesystem::path>(key));
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
        gfx::d3d12engine::rp::ShadowMapAnimated::id
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
        gfx::d3d12engine::rp::ShadowMap::id
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
        gfx::d3d12engine::rp::ShadowMap::id
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
        gfx::d3d12engine::rp::ShadowMap::id
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
        gfx::d3d12engine::rp::ShadowMap::id
    );
    return entity;
}