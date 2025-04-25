#ifndef __objectInitializers_HPP
#define __objectInitializers_HPP

#include "ecs.hpp"
#include "d3d12engine/d3d12Engine.hpp"

ecs::Entity MU_CALLCONV createCharacter(
    mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyModel,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyBVHPath,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip
);

ecs::Entity MU_CALLCONV createHelicopter(
    mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyModel,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyBVHPath,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip
);

ecs::Entity MU_CALLCONV createTree0(
    mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyModel,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyBVHPath,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip
);

ecs::Entity MU_CALLCONV createTree1(
    mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyModel,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyBVHPath,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip
);

ecs::Entity MU_CALLCONV createTree2(
    mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyModel,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyBVHPath,
    const gfx::d3d12::ResourceStorage::SlotID& slotKeyAnimClip
);

#endif  // __objectInitializers_HPP