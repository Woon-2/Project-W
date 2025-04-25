#include "objectInitializers.hpp"

#include "game/physicsSystem.hpp"
#include "game/animSystem.hpp"

#include "assetMap.hpp"

#include <optional>

fsm::State characterStateIdle(fsm::FSM& fsm) {
    for (;;) {
        while (auto events = co_await fsm.getEvents()) {
            while (auto ev = events.pop()) {
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
        animCon.fsm().addState("Idle", characterStateIdle);
        animCon.fsm().start("Idle");

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

        circular(
            std::vector<std::string>{ "GO_Character_Idle", "GO_Character_Idle1",
                "GO_Character_Idle2", "GO_Character_Walk", "GO_Character_Run",
                "GO_Character_Sprint"
            },
            animCon
        ).resume();

        // animCon.play("GO_Character_Run", AnimInstance::ClipMode::Presampled);
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