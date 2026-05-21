#include "rspch.hpp"
#include "skillSystem.hpp"
#include "../collision.hpp"
#include <algorithm>

// ---------------------------------------------------------------------------
// ServerSkillSystem
// ---------------------------------------------------------------------------

void ServerSkillSystem::registerAssets(std::vector<SkillAsset>&& assets) {
    assetRegistry_ = std::move(assets);
    assetRegistry_.shrink_to_fit();
}

const SkillAsset* ServerSkillSystem::findAsset(u32t id) const {
    for (const SkillAsset& a : assetRegistry_)
        if (a.id == id) return &a;
    return nullptr;
}

int ServerSkillSystem::allocInstance(const SkillAsset* asset, i32t ownerId) {
    for (int i = 0; i < kMaxInstances; ++i) {
        if (!instances_[i].active) {
            auto& inst = instances_[i];
            inst = ServerSkillInstance{};
            inst.asset   = asset;
            inst.ownerId = ownerId;
            inst.active  = true;
            return i;
        }
    }
    return -1;
}

void ServerSkillSystem::terminateInstance(ServerSkillInstance& inst) {
    for (auto& slot : inst.slots)
        slot.live = false;
    inst.active = false;
}

int ServerSkillSystem::startSkill(u32t assetId, i32t ownerId, Milliseconds initialElapsed) {
    const SkillAsset* asset = findAsset(assetId);
    if (!asset) return -1;

    int idx = allocInstance(asset, ownerId);
    if (idx < 0) return -1;

    ServerSkillInstance& inst = instances_[idx];
    inst.elapsed = initialElapsed;

    // Fast-forward timeline events that have already passed
    while (inst.nextEvIdx < (int)asset->timeline.size()) {
        const TimelineEvent& ev = asset->timeline[inst.nextEvIdx];
        if (ev.time > initialElapsed) break;

        if (ev.type == SkillEventType::SpawnHitbox) {
            u8t slot = asset->hitboxDefs[ev.payload.spawnHitbox.defIdx].slot;
            if (slot < ServerSkillInstance::kMaxSlots) {
                inst.slots[slot].live   = true;
                inst.slots[slot].defIdx = ev.payload.spawnHitbox.defIdx;
            }
        } else if (ev.type == SkillEventType::DestroyHitbox) {
            u8t slot = ev.payload.destroyHitbox.slot;
            if (slot < ServerSkillInstance::kMaxSlots)
                inst.slots[slot].live = false;
        }
        ++inst.nextEvIdx;
    }

    if (asset->totalDuration.count() > 0.f && initialElapsed >= asset->totalDuration)
        terminateInstance(inst);

    return idx;
}

// ---------------------------------------------------------------------------
// Bone height approximation table
// ---------------------------------------------------------------------------

float ServerSkillSystem::approxBoneHeight(std::string_view boneName) {
    if (boneName.find("head")   != std::string_view::npos) return 1.6f;
    if (boneName.find("neck")   != std::string_view::npos) return 1.5f;
    if (boneName.find("spine")  != std::string_view::npos) return 1.0f;
    if (boneName.find("chest")  != std::string_view::npos) return 1.2f;
    if (boneName.find("hand")   != std::string_view::npos) return 1.0f;
    if (boneName.find("weapon") != std::string_view::npos) return 1.0f;
    if (boneName.find("foot")   != std::string_view::npos) return 0.1f;
    return 0.8f;
}

// ---------------------------------------------------------------------------
// Hitbox AABB approximation (server has no animation state)
// ---------------------------------------------------------------------------

AABB ServerSkillSystem::approximateHitboxAABB(const SkillHitboxDef& def,
                                               const mu::Vec3& ownerPos,
                                               const mu::NQuat& ownerOrient) const {
    // Bone attach: translate bone origin using height table
    mu::Vec3 boneOrigin = ownerPos;

    if (def.attach.type == AttachType::Bone) {
        float h = approxBoneHeight(def.attach.targetName);
        boneOrigin = ownerPos + mu::Vec3{ 0.f, h, 0.f };
    }
    // VFXParticle hitboxes are skipped on server (no particle system)

    if (def.localOBBs.empty())
        return AABB{ boneOrigin, { 1.f, 1.f, 1.f } };

    // Merge all local OBBs into one world-space AABB (conservative)
    mu::Vec3 minP{ std::numeric_limits<float>::max() };
    mu::Vec3 maxP{ std::numeric_limits<float>::lowest() };

    for (const OBB& obb : def.localOBBs) {
        // Rotate local center by owner orientation then offset by bone origin
        mu::Vec3 worldCenter = boneOrigin + ownerOrient.rotate(obb.center);

        // Conservatively expand by OBB halfExtent magnitude
        float r = obb.halfExtents.len();
        minP = mu::Vec3{
            std::min(minP.x(), worldCenter.x() - r),
            std::min(minP.y(), worldCenter.y() - r),
            std::min(minP.z(), worldCenter.z() - r)
        };
        maxP = mu::Vec3{
            std::max(maxP.x(), worldCenter.x() + r),
            std::max(maxP.y(), worldCenter.y() + r),
            std::max(maxP.z(), worldCenter.z() + r)
        };
    }

    mu::Vec3 center = (minP + maxP) * 0.5f;
    mu::Vec3 size   = maxP - minP;
    return AABB{ center, size };
}

// ---------------------------------------------------------------------------
// Collision check: all live slots vs. all targets
// ---------------------------------------------------------------------------

void ServerSkillSystem::checkCollisions(ServerSkillInstance& inst,
                                         const std::vector<ServerSkillOwner>& owners,
                                         const std::vector<ServerSkillTarget>& targets,
                                         std::vector<SkillHitResult>& outHits) {
    // Find owner state
    const ServerSkillOwner* ownerState = nullptr;
    for (const auto& o : owners) {
        if (o.id == inst.ownerId) { ownerState = &o; break; }
    }
    if (!ownerState) return;

    for (int s = 0; s < ServerSkillInstance::kMaxSlots; ++s) {
        if (!inst.slots[s].live) continue;

        const SkillHitboxDef& def = inst.asset->hitboxDefs[inst.slots[s].defIdx];

        // VFXParticle hitboxes are skipped on server
        if (def.attach.type == AttachType::VFXParticle) continue;

        const AABB hitbox = approximateHitboxAABB(def, ownerState->pos, ownerState->orient);
        const u8t  group  = def.hitGroup;

        auto& groupState = inst.hitGroups[group];
        const float cooldownMs = def.hitGroupCooldownMs;

        for (const ServerSkillTarget& tgt : targets) {
            if (tgt.hp <= 0) continue;
            if (tgt.id == inst.ownerId) continue;

            // Check hit group cooldown
            if (auto it = groupState.lastHitByTarget.find(tgt.id);
                it != groupState.lastHitByTarget.end())
            {
                if (cooldownMs <= 0.f) continue;  // hit once only
                if ((inst.elapsed - it->second).count() < cooldownMs) continue;
            }

            if (!collides(hitbox, tgt.worldAABB).hit) continue;

            outHits.push_back({
                inst.ownerId,
                tgt.id,
                def.onHit.damage,
                inst.asset->id
            });
            groupState.lastHitByTarget[tgt.id] = inst.elapsed;
        }
    }
}

// ---------------------------------------------------------------------------
// Per-instance tick
// ---------------------------------------------------------------------------

void ServerSkillSystem::tickInstance(ServerSkillInstance& inst, Milliseconds dt,
                                      const std::vector<ServerSkillOwner>& owners,
                                      const std::vector<ServerSkillTarget>& targets,
                                      std::vector<SkillHitResult>& outHits) {
    inst.elapsed = inst.elapsed + dt;

    // Fire timeline events
    while (inst.nextEvIdx < (int)inst.asset->timeline.size()) {
        const TimelineEvent& ev = inst.asset->timeline[inst.nextEvIdx];
        if (ev.time > inst.elapsed) break;

        switch (ev.type) {
        case SkillEventType::SpawnHitbox: {
            u8t slot = inst.asset->hitboxDefs[ev.payload.spawnHitbox.defIdx].slot;
            if (slot < ServerSkillInstance::kMaxSlots) {
                inst.slots[slot].live   = true;
                inst.slots[slot].defIdx = ev.payload.spawnHitbox.defIdx;
            }
            break;
        }
        case SkillEventType::DestroyHitbox: {
            u8t slot = ev.payload.destroyHitbox.slot;
            if (slot < ServerSkillInstance::kMaxSlots)
                inst.slots[slot].live = false;
            break;
        }
        // PlayAnimation, PlayVFX, CameraShake: no-op on server
        default:
            break;
        }
        ++inst.nextEvIdx;
    }

    // Check collisions for all live hitboxes
    checkCollisions(inst, owners, targets, outHits);

    // Terminate if duration exceeded
    if (inst.asset->totalDuration.count() > 0.f && inst.elapsed >= inst.asset->totalDuration)
        terminateInstance(inst);
}

// ---------------------------------------------------------------------------
// Main update
// ---------------------------------------------------------------------------

void ServerSkillSystem::update(Milliseconds dt,
                                const std::vector<ServerSkillOwner>& owners,
                                const std::vector<ServerSkillTarget>& targets,
                                std::vector<SkillHitResult>& outHits) {
    for (int i = 0; i < kMaxInstances; ++i) {
        if (!instances_[i].active) continue;
        tickInstance(instances_[i], dt, owners, targets, outHits);
    }
}
