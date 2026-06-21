#include "rspch.hpp"
#include "skillSystem.hpp"
#include "../Model.hpp"
#include "../collision.hpp"
#include <algorithm>
#include <iostream>
#include <string_view>

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

// Margin added to hitbox broad-phase AABBs. Compensates for the one anim-tick
// staleness between physics step and skill update, preventing false negatives.
static constexpr float kHitboxAABBMargin = 0.2f;

// Union AABB of a set of OBBs, fattened by `margin` on each side.
static AABB unionAABBOfOBBs(const std::vector<OBB>& obbs, float margin) {
    if (obbs.empty()) return AABB{ {}, {} };

    AABB     first = obbToAABB(obbs[0]);
    mu::Vec3 mn    = first.center - first.size * 0.5f;
    mu::Vec3 mx    = first.center + first.size * 0.5f;

    for (std::size_t i = 1; i < obbs.size(); ++i) {
        AABB a = obbToAABB(obbs[i]);
        mn = mu::min(mn, a.center - a.size * 0.5f);
        mx = mu::max(mx, a.center + a.size * 0.5f);
    }

    const mu::Vec3 m{ margin, margin, margin };
    mn = mn - m;
    mx = mx + m;

    AABB out;
    out.center = (mn + mx) * 0.5f;
    out.size   = (mx - mn);
    return out;
}

// Unit quaternion mapping world up (+Y) onto the given (assumed unit) terrain
// normal. Identity when the normal is (near) vertical. Mirrors the client helper.
static mu::NQuat alignQuatYToNormal(mu::Vec3 n) {
    const mu::Vec3 up{ 0.f, 1.f, 0.f };
    const mu::Vec3 axis = mu::cross(up, n);
    const float    s2   = mu::dot(axis, axis);
    if (s2 < 1e-8f) return mu::NQuat{};
    const float c     = std::clamp(mu::dot(up, n), -1.f, 1.f);
    const float angle = std::acos(c);
    return mu::NQuat(mu::quatRotMat(mu::rotateH(mu::Radian{ angle }, mu::Vec3(mu::normalize(axis)))));
}

// Capture the caster's world XZ + yaw at skill start (mirrors client). Server
// Object exposes pos()/orient() rather than a render-state world matrix.
static void captureCastAnchor(SkillInstance& inst, const Object* owner) {
    inst.castAnchor.valid = false;
    if (!owner) return;
    const mu::Mat4x4 orientMat(owner->orient());
    const mu::Vec3   fwd = mu::normalize(mu::Vec3(mu::Vec4(0.f, 0.f, 1.f, 0.f) * orientMat));
    inst.castAnchor.pos   = owner->pos();
    inst.castAnchor.yaw   = std::atan2(fwd.x(), fwd.z());
    inst.castAnchor.valid = true;
}

// ---------------------------------------------------------------------------
// SkillInstancePool (dynamic free-list + active-list)
// ---------------------------------------------------------------------------

int SkillInstancePool::alloc(const SkillAsset* asset, i32t ownerObjectId) {
    int idx;
    if (!freeList.empty()) {
        idx = freeList.back();
        freeList.pop_back();
    } else {
        idx = static_cast<int>(instances.size());
        instances.emplace_back();
    }

    SkillInstance& inst = instances[idx];
    inst.asset         = asset;
    inst.ownerObjectId = ownerObjectId;
    inst.elapsed       = Milliseconds{ 0.f };
    inst.nextEventIdx  = 0;
    inst.active        = true;
    inst.interrupted   = false;
    inst.seed          = 0;
    inst.resetSlots();

    activeList.push_back(idx);
    return idx;
}

void SkillInstancePool::free(int idx) {
    if (idx < 0 || idx >= static_cast<int>(instances.size())) return;
    if (!instances[idx].active) return;
    instances[idx].active = false;

    auto it = std::find(activeList.begin(), activeList.end(), idx);
    if (it != activeList.end()) {
        *it = activeList.back();
        activeList.pop_back();
    }
    freeList.push_back(idx);
}

// ---------------------------------------------------------------------------
// SkillBroadPhase
// ---------------------------------------------------------------------------

bool SkillBroadPhase::overlapYZ(const AABB& a, const AABB& b) {
    const float aMinY = a.center.y() - a.size.y() * 0.5f;
    const float aMaxY = a.center.y() + a.size.y() * 0.5f;
    const float bMinY = b.center.y() - b.size.y() * 0.5f;
    const float bMaxY = b.center.y() + b.size.y() * 0.5f;
    if (aMaxY < bMinY || bMaxY < aMinY) return false;

    const float aMinZ = a.center.z() - a.size.z() * 0.5f;
    const float aMaxZ = a.center.z() + a.size.z() * 0.5f;
    const float bMinZ = b.center.z() - b.size.z() * 0.5f;
    const float bMaxZ = b.center.z() + b.size.z() * 0.5f;
    if (aMaxZ < bMinZ || bMaxZ < aMinZ) return false;

    return true;
}

void SkillBroadPhase::build(const std::vector<HitboxEntry>& hitboxes,
                            const std::vector<TargetEntry>& targets) {
    endpoints_.clear();
    activeHitboxes_.clear();
    activeTargets_.clear();
    candidates_.clear();

    endpoints_.reserve((hitboxes.size() + targets.size()) * 2);

    auto pushEndpoints = [this](const AABB& a, int idx, bool isHitbox) {
        const float minX = a.center.x() - a.size.x() * 0.5f;
        const float maxX = a.center.x() + a.size.x() * 0.5f;
        endpoints_.push_back({ minX, idx, false, isHitbox });
        endpoints_.push_back({ maxX, idx, true,  isHitbox });
    };
    for (int i = 0; i < static_cast<int>(hitboxes.size()); ++i)
        pushEndpoints(hitboxes[i].aabb, i, true);
    for (int i = 0; i < static_cast<int>(targets.size()); ++i)
        pushEndpoints(targets[i].aabb, i, false);

    std::sort(endpoints_.begin(), endpoints_.end(),
              [](const Endpoint& a, const Endpoint& b) {
                  if (a.x != b.x) return a.x < b.x;
                  return (!a.isMax) && b.isMax;  // min before max at same coord
              });

    for (const Endpoint& ep : endpoints_) {
        if (!ep.isMax) {
            if (ep.isHitbox) {
                const HitboxEntry& he = hitboxes[ep.idx];
                for (int t : activeTargets_)
                    if ((he.mask & targets[t].category) && overlapYZ(he.aabb, targets[t].aabb))
                        candidates_.push_back({ he.hitboxIdx, targets[t].target });
                activeHitboxes_.push_back(ep.idx);
            } else {
                const TargetEntry& te = targets[ep.idx];
                for (int h : activeHitboxes_)
                    if ((hitboxes[h].mask & te.category) && overlapYZ(hitboxes[h].aabb, te.aabb))
                        candidates_.push_back({ hitboxes[h].hitboxIdx, te.target });
                activeTargets_.push_back(ep.idx);
            }
        } else {
            std::vector<int>& act = ep.isHitbox ? activeHitboxes_ : activeTargets_;
            auto it = std::find(act.begin(), act.end(), ep.idx);
            if (it != act.end()) { *it = act.back(); act.pop_back(); }
        }
    }
}

// ---------------------------------------------------------------------------
// Asset management
// ---------------------------------------------------------------------------

void SkillSystem::bindRegistry(const std::vector<SkillAsset>* registry) {
    assetRegistry_ = registry;
}

const SkillAsset* SkillSystem::findAsset(std::string_view name) const {
    if (!assetRegistry_) return nullptr;
    for (const SkillAsset& a : *assetRegistry_)
        if (a.name == name) return &a;
    return nullptr;
}

const SkillAsset* SkillSystem::findAsset(u32t id) const {
    if (!assetRegistry_) return nullptr;
    for (const SkillAsset& a : *assetRegistry_)
        if (a.id == id) return &a;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

int SkillSystem::startSkill(u32t assetId, i32t ownerObjectId, SkillDispatchContext& ctx,
                            u32t seed) {
    const SkillAsset* asset = findAsset(assetId);
    if (!asset) return -1;

    int idx = instancePool_.alloc(asset, ownerObjectId);
    if (idx < 0) return -1;

    SkillInstance& inst = instancePool_.instances[idx];
    inst.seed = seed;
    captureCastAnchor(inst, lookupObject(ctx, ownerObjectId));
    while (inst.nextEventIdx < (int)asset->timeline.size()) {
        if (asset->timeline[inst.nextEventIdx].time > Milliseconds{ 0.f }) break;
        dispatchEvent(asset->timeline[inst.nextEventIdx], inst, ctx);
        ++inst.nextEventIdx;
    }
    return idx;
}

int SkillSystem::startSkill(u32t assetId, i32t ownerObjectId, SkillDispatchContext& ctx,
                            Milliseconds initialElapsed, u32t seed, float damageScale) {
    const SkillAsset* asset = findAsset(assetId);
    if (!asset) return -1;

    int idx = instancePool_.alloc(asset, ownerObjectId);
    if (idx < 0) return -1;

    SkillInstance& inst = instancePool_.instances[idx];
    inst.elapsed     = initialElapsed;
    inst.seed        = seed;
    inst.damageScale = damageScale;
    captureCastAnchor(inst, lookupObject(ctx, ownerObjectId));

    while (inst.nextEventIdx < (int)asset->timeline.size()) {
        if (asset->timeline[inst.nextEventIdx].time > initialElapsed) break;
        dispatchEvent(asset->timeline[inst.nextEventIdx], inst, ctx);
        ++inst.nextEventIdx;
    }

    if (asset->totalDuration.count() > 0 && initialElapsed >= asset->totalDuration)
        terminateInstance(inst, ctx);

    return idx;
}

void SkillSystem::interruptAll(i32t ownerObjectId, SkillDispatchContext& ctx) {
    // Snapshot active indices: terminateInstance() mutates activeList.
    instanceScratch_ = instancePool_.activeList;
    for (int idx : instanceScratch_) {
        SkillInstance& inst = instancePool_.instances[idx];
        if (!inst.active || inst.ownerObjectId != ownerObjectId) continue;
        if (inst.asset && !inst.asset->interruptible)               continue;
        terminateInstance(inst, ctx);
    }
}

bool SkillSystem::hasActiveSkill(i32t ownerObjectId) const {
    for (int idx : instancePool_.activeList) {
        const SkillInstance& inst = instancePool_.instances[idx];
        if (inst.active && inst.ownerObjectId == ownerObjectId) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Update (5 passes — mirrors client)
// ---------------------------------------------------------------------------

void SkillSystem::update(Milliseconds dt, SkillDispatchContext& ctx) {
    // Snapshot active indices: tickInstance() may terminate (mutating activeList).
    instanceScratch_ = instancePool_.activeList;
    for (int idx : instanceScratch_) {
        SkillInstance& inst = instancePool_.instances[idx];
        if (!inst.active) continue;
        tickInstance(inst, dt, ctx);
    }

    updateHitboxes(ctx);
    updateParticleHitboxSources(ctx);

    pendingHits_.clear();
    checkHitboxCollisions(ctx);

    processHitResults(ctx);
}

void SkillSystem::tickInstance(SkillInstance& inst, Milliseconds dt, SkillDispatchContext& ctx) {
    inst.elapsed += dt;

    const std::vector<TimelineEvent>& tl = inst.asset->timeline;
    while (inst.nextEventIdx < (int)tl.size()) {
        if (tl[inst.nextEventIdx].time > inst.elapsed) break;
        dispatchEvent(tl[inst.nextEventIdx], inst, ctx);
        ++inst.nextEventIdx;
    }

    if (inst.asset->totalDuration.count() > 0.f &&
        inst.elapsed >= inst.asset->totalDuration) {
        terminateInstance(inst, ctx);
    }
}

void SkillSystem::terminateInstance(SkillInstance& inst, SkillDispatchContext& ctx) {
    cleanupHitboxes(inst);
    const int idx = static_cast<int>(&inst - instancePool_.instances.data());
    instancePool_.free(idx);
}

void SkillSystem::cleanupHitboxes(SkillInstance& inst) {
    for (int h : inst.boneHitboxBySlot)
        if (h >= 0) freeHitbox(h);
    inst.boneHitboxBySlot.clear();

    for (int s : inst.particleSourceBySlot)
        if (s >= 0) freeParticleSource(s);
    inst.particleSourceBySlot.clear();
}

// ---------------------------------------------------------------------------
// Event dispatch
// ---------------------------------------------------------------------------

void SkillSystem::dispatchEvent(const TimelineEvent& ev, SkillInstance& inst,
                                SkillDispatchContext& ctx) {
    switch (ev.type) {

    case SkillEventType::SpawnHitbox: {
        const u8t defIdx = ev.payload.spawnHitbox.defIdx;
        if (defIdx >= inst.asset->hitboxDefs.size()) break;
        const SkillHitboxDef& def = inst.asset->hitboxDefs[defIdx];
        const int slot = def.slot;

        Object* owner = lookupObject(ctx, inst.ownerObjectId);

        if (def.attach.type == AttachType::Bone) {
            int oldH = inst.getBoneHandle(slot);
            if (oldH >= 0) freeHitbox(oldH);

            int hi = allocHitbox();
            if (hi < 0) break;
            inst.setBoneHandle(slot, hi);

            AttachedHitbox& hb        = hitboxPool_[hi];
            hb.active                 = true;
            hb.particleSourceIdx      = -1;
            hb.localOBBs              = def.localOBBs;
            hb.worldOBBs.resize(def.localOBBs.size());
            hb.onHit                  = def.onHit;
            hb.ownerObjectId          = inst.ownerObjectId;
            hb.instanceIdx            = static_cast<i32t>(&inst - instancePool_.instances.data());
            hb.slot                   = static_cast<u8t>(slot);
            hb.hitGroup               = def.hitGroup;
            hb.hitGroupCooldownMs     = def.hitGroupCooldownMs;
            hb.applyAttachRotation    = def.applyAttachRotation;

            if (owner) {
                hb.resolvedAttach = resolveAttach(def.attach, *owner, ctx);
                mu::Mat4x4 xform  = computeAttachTransform(*owner, hb);
                mu::NQuat boneOrient(mu::quatRotMat(xform));
                for (int i = 0; i < (int)hb.localOBBs.size(); ++i) {
                    hb.worldOBBs[i].center     = mu::Vec3(mu::Vec4(hb.localOBBs[i].center, 1.f) * xform);
                    hb.worldOBBs[i].halfExtents = hb.localOBBs[i].halfExtents;
                    hb.worldOBBs[i].orient      = hb.localOBBs[i].orient;
                    hb.worldOBBs[i].orient     *= boneOrient;
                }
            } else {
                hb.resolvedAttach.type    = AttachType::Bone;
                hb.resolvedAttach.boneIdx = -1;
                hb.worldOBBs              = hb.localOBBs;
            }
            hb.worldAABB   = unionAABBOfOBBs(hb.worldOBBs, kHitboxAABBMargin);
            hb.targetMask  = owner ? hostileMask(owner->faction()) : 0u;
        } else if (def.attach.type == AttachType::Ground) {
            // Ground attach: plant OBBs at a caster-relative point snapped to the
            // terrain, then leave them static (updateHitboxes skips non-Bone).
            // Mirrors client so authoritative hit positions match prediction.
            int oldH = inst.getBoneHandle(slot);
            if (oldH >= 0) freeHitbox(oldH);

            int hi = allocHitbox();
            if (hi < 0) break;
            inst.setBoneHandle(slot, hi);

            if (!inst.castAnchor.valid) captureCastAnchor(inst, owner);

            AttachedHitbox& hb        = hitboxPool_[hi];
            hb.active                 = true;
            hb.particleSourceIdx      = -1;
            hb.localOBBs              = def.localOBBs;
            hb.worldOBBs.resize(def.localOBBs.size());
            hb.onHit                  = def.onHit;
            hb.ownerObjectId          = inst.ownerObjectId;
            hb.instanceIdx            = static_cast<i32t>(&inst - instancePool_.instances.data());
            hb.slot                   = static_cast<u8t>(slot);
            hb.hitGroup               = def.hitGroup;
            hb.hitGroupCooldownMs     = def.hitGroupCooldownMs;
            hb.applyAttachRotation    = def.applyAttachRotation;
            hb.resolvedAttach.type    = AttachType::Ground;   // static (skipped by updateHitboxes)
            hb.resolvedAttach.boneIdx = -1;

            const mu::Mat4x4 yawRot = mu::rotateYH(mu::Radian{ inst.castAnchor.yaw });
            const mu::NQuat  yawQuat(mu::quatRotMat(yawRot));

            const int ref = def.attach.groundAnchorRef;
            const bool useRegisteredAnchor =
                ref >= 0 && ref < SkillInstance::kMaxGroundAnchors && inst.groundAnchors[ref].valid;

            if (useRegisteredAnchor) {
                // Point impact: place OBBs rigidly in a registered SetGroundAnchor frame, so a
                // ring of separately-configured hitboxes (each with its own onHit) shares one
                // impact point. center is an offset in the anchor frame. Mirrors the client.
                const SkillInstance::GroundAnchor& ga = inst.groundAnchors[ref];
                const mu::Mat4x4 frameRot(ga.orient);
                for (int i = 0; i < (int)hb.localOBBs.size(); ++i) {
                    hb.worldOBBs[i].center      = ga.pos
                        + mu::Vec3(mu::Vec4(hb.localOBBs[i].center, 0.f) * frameRot);
                    hb.worldOBBs[i].halfExtents = hb.localOBBs[i].halfExtents;
                    hb.worldOBBs[i].orient      = hb.localOBBs[i].orient * ga.orient;
                }
            } else {
                // Distributed eruption: each OBB snaps to the terrain at its own XZ.
                for (int i = 0; i < (int)hb.localOBBs.size(); ++i) {
                    mu::Vec3 wc = inst.castAnchor.pos
                                + mu::Vec3(mu::Vec4(hb.localOBBs[i].center, 0.f) * yawRot);
                    if (ctx.groundHeight)
                        wc.setComponent(1, ctx.groundHeight(wc.x(), wc.z()) + hb.localOBBs[i].center.y());
                    hb.worldOBBs[i].center      = wc;
                    hb.worldOBBs[i].halfExtents = hb.localOBBs[i].halfExtents;
                    hb.worldOBBs[i].orient      = hb.localOBBs[i].orient * yawQuat;
                    if (def.attach.groundAlign && ctx.groundNormal)
                        hb.worldOBBs[i].orient = hb.worldOBBs[i].orient
                                               * alignQuatYToNormal(ctx.groundNormal(wc.x(), wc.z()));
                }
            }
            hb.worldAABB  = unionAABBOfOBBs(hb.worldOBBs, kHitboxAABBMargin);
            hb.targetMask = owner ? hostileMask(owner->faction()) : 0u;
        } else {
            // VFXParticle: create a source entry driven by the deterministic
            // gameplay sampler (counterpart of the client's live ParticleSystem).
            int oldS = inst.getParticleHandle(slot);
            if (oldS >= 0) freeParticleSource(oldS);

            int si = allocParticleSource();
            inst.setParticleHandle(slot, si);

            ParticleHitboxSource& src = particleSources_[si];
            src.active             = true;
            src.templateOBBs       = def.localOBBs;
            src.onHit              = def.onHit;
            src.targetMask         = owner ? hostileMask(owner->faction()) : 0u;
            src.ownerObjectId      = inst.ownerObjectId;
            src.instanceIdx        = static_cast<i32t>(&inst - instancePool_.instances.data());
            src.slot               = static_cast<u8t>(slot);
            src.hitGroup           = def.hitGroup;
            src.hitGroupCooldownMs = def.hitGroupCooldownMs;
            src.useParticleSize    = def.useParticleSize;
            src.applyRotation      = def.applyAttachRotation;
            src.penetrate          = def.penetrate;
            src.consumedKeys.clear();
            src.consumeAnchor      = {};
            src.hitboxHandles.clear();

            // Bind the deterministic sampler. Per-system seed mirrors the
            // client chain: ParticleEffect seed = mixSeed(inst.seed, vfxId),
            // then system i seed = mixSeed(effectSeed, i). Gameplay configs
            // are prebuilt at boot (buildVfxGameplayConfigs) and shared.
            src.vfxId       = def.attach.vfxId;
            src.gameplayCfg = nullptr;
            src.vdef        = nullptr;
            src.systemIdx   = static_cast<u8t>(def.attach.particleSystemIdx);
            src.effectSeed  = pg::mixSeed(inst.seed, def.attach.vfxId);
            if (def.attach.vfxId < inst.asset->vfxDefs.size()) {
                const SkillAsset::VfxDef& vdef = inst.asset->vfxDefs[def.attach.vfxId];
                const int sysIdx = def.attach.particleSystemIdx;
                if (sysIdx >= 0 && sysIdx < static_cast<int>(vdef.systems.size())) {
                    src.vdef        = &vdef;
                    src.gameplayCfg = vdef.systems[static_cast<std::size_t>(sysIdx)]
                                          .gameplayCfg.get();
                }
            }
            if (!src.gameplayCfg) {
                std::cout << "[SkillSystem] VFXParticle hitbox has no gameplay config"
                             " (skill '" << inst.asset->name
                          << "' vfx " << static_cast<int>(def.attach.vfxId)
                          << " sys " << def.attach.particleSystemIdx
                          << "); add a systems table to addVFX. Hitboxes inactive.\n";
            }
        }
        break;
    }

    case SkillEventType::DestroyHitbox: {
        const int slot = ev.payload.destroyHitbox.slot;
        int bh = inst.getBoneHandle(slot);
        if (bh >= 0) { freeHitbox(bh); inst.setBoneHandle(slot, -1); }
        int ps = inst.getParticleHandle(slot);
        if (ps >= 0) { freeParticleSource(ps); inst.setParticleHandle(slot, -1); }
        break;
    }

    case SkillEventType::PlayAnimation: {
        // No-op on server (animation driven by Npc AI state machine, not skill events)
        break;
    }

    case SkillEventType::PlayVFX: {
        // No rendering on the server, but the deterministic VFXParticle
        // hitbox sampler needs the effect's world anchor. The transform math
        // mirrors the client PlayVFX handler (client/skill/skillSystem.cpp)
        // so both sides place the emitter identically.
        const auto& p = ev.payload.playVFX;
        Object* owner = lookupObject(ctx, inst.ownerObjectId);
        if (!owner) break;

        mu::Mat4x4 baseXform = mu::Mat4x4(owner->orient()) * mu::translate(owner->pos());

        const bool hasBoneAttach = (p.attachType == static_cast<u8t>(AttachType::Bone) &&
                                    p.attachTargetName[0] != '\0');
        if (hasBoneAttach && owner->model() && !owner->model()->skeleton.empty()) {
            const auto& nameToIdx = owner->model()->skeleton.nameToIdx;
            auto it = nameToIdx.find(p.attachTargetName);
            if (it != nameToIdx.end()) {
                const auto& boneXforms = owner->boneWorldXforms();
                if (it->second >= 0 && it->second < static_cast<i32t>(boneXforms.size()))
                    baseXform = boneXforms[static_cast<std::size_t>(it->second)];
            }
        }

        mu::Mat4x4 baseRot;
        if (p.flags & kPlayVFXFlagYawOnly) {
            mu::Vec3 fwd = mu::normalize(mu::Vec3(mu::Vec4(0.f, 0.f, 1.f, 0.f) * baseXform));
            baseRot = mu::rotateYH(mu::Radian{ std::atan2(fwd.x(), fwd.z()) });
        } else {
            baseRot = mu::Mat4x4(mu::NQuat(mu::quatRotMat(baseXform)));
        }

        mu::Mat4x4 eulerOff = mu::rotateRPYH(mu::Degree{ p.localEulerDeg.z() },   // roll
                                             mu::Degree{ p.localEulerDeg.y() },   // pitch
                                             mu::Degree{ p.localEulerDeg.x() });  // yaw
        mu::Mat4x4 aim = eulerOff * baseRot;

        mu::Vec3 origin = mu::Vec3(mu::Vec4(0.f, 0.f, 0.f, 1.f) * baseXform);
        const mu::Vec3& off = p.localOffset;
        mu::Vec3 worldPos = origin + mu::Vec3(mu::Vec4(off.x(), off.y(), off.z(), 0.f) * aim);

        if ((p.flags & kPlayVFXFlagGroundSnap) && ctx.groundHeight) {
            worldPos.setComponent(1, ctx.groundHeight(worldPos.x(), worldPos.z()) + off.y());
            if ((p.flags & kPlayVFXFlagGroundAlign) && ctx.groundNormal) {
                aim = aim * mu::Mat4x4(alignQuatYToNormal(
                    ctx.groundNormal(worldPos.x(), worldPos.z())));
            }
        }

        // Particle ground-conform override (Lua particleConform), forwarded to
        // the sampler exactly like ParticleEffect::setGroundBehavior on client.
        const u8t confOrd = (p.flags & kPlayVFXParticleConformMask) >> kPlayVFXParticleConformShift;
        inst.setVfxAnchor(p.vfxId, worldPos, aim, ev.time,
                          static_cast<pg::GroundConform>(confOrd));
        break;
    }

    case SkillEventType::ApplyImpulse: {
        const auto& p = ev.payload.applyImpulse;
        Object* owner = lookupObject(ctx, inst.ownerObjectId);
        if (!owner) break;
        // Transform local direction to world space via owner's orientation
        mu::Mat4x4 orientMat(owner->orient());
        mu::Vec3 impulseJ = mu::Vec3(mu::Vec4(p.dirLocal, 0.f) * orientMat) * p.strength;
        owner->body().applyImpulse(impulseJ, owner->pos());
        break;
    }

    case SkillEventType::CameraShake: {
        // No-op on server (no camera)
        break;
    }

    case SkillEventType::ModifyStat: {
        const auto& p = ev.payload.modifyStat;
        Object* owner = lookupObject(ctx, inst.ownerObjectId);
        if (!owner || p.hpDelta == 0) break;
        owner->setHp(owner->hp() + p.hpDelta);
        break;
    }

    case SkillEventType::SetGroundAnchor: {
        // Register a terrain-snapped frame referenced by subsequent Ground-attach hitboxes.
        // Mirrors the client so authoritative hit positions match prediction.
        const auto& p = ev.payload.setGroundAnchor;
        if (p.anchorId >= SkillInstance::kMaxGroundAnchors) break;
        Object* owner = lookupObject(ctx, inst.ownerObjectId);
        if (!inst.castAnchor.valid) captureCastAnchor(inst, owner);

        const mu::Mat4x4 yawRot = mu::rotateYH(mu::Radian{ inst.castAnchor.yaw });
        const mu::NQuat  yawQuat(mu::quatRotMat(yawRot));
        mu::Vec3 world = inst.castAnchor.pos
                       + mu::Vec3(mu::Vec4(p.localOffset, 0.f) * yawRot);
        mu::NQuat alignQuat{};
        if (ctx.groundHeight) {
            world.setComponent(1, ctx.groundHeight(world.x(), world.z()) + p.localOffset.y());
            if ((p.flags & kGroundAnchorFlagAlign) && ctx.groundNormal)
                alignQuat = alignQuatYToNormal(ctx.groundNormal(world.x(), world.z()));
        }
        inst.groundAnchors[p.anchorId] = { world, yawQuat * alignQuat, true };
        break;
    }

    case SkillEventType::SendGameplayEvent:
    case SkillEventType::SpawnProjectile:
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Hitbox world transform update (bone type only)
// ---------------------------------------------------------------------------

void SkillSystem::updateHitboxes(SkillDispatchContext& ctx) {
    for (AttachedHitbox& hb : hitboxPool_) {
        if (!hb.active) continue;
        if (hb.resolvedAttach.type != AttachType::Bone) continue;

        Object* owner = lookupObject(ctx, hb.ownerObjectId);
        if (!owner) { hb.active = false; continue; }

        mu::Mat4x4 xform = computeAttachTransform(*owner, hb);
        mu::NQuat boneOrient(mu::quatRotMat(xform));
        hb.worldOBBs.resize(hb.localOBBs.size());
        for (int oi = 0; oi < (int)hb.localOBBs.size(); ++oi) {
            hb.worldOBBs[oi].center     = mu::Vec3(mu::Vec4(hb.localOBBs[oi].center, 1.f) * xform);
            hb.worldOBBs[oi].halfExtents = hb.localOBBs[oi].halfExtents;
            hb.worldOBBs[oi].orient      = hb.localOBBs[oi].orient;
            if (hb.applyAttachRotation)
                hb.worldOBBs[oi].orient *= boneOrient;
        }
        hb.worldAABB = unionAABBOfOBBs(hb.worldOBBs, kHitboxAABBMargin);
    }
}

// ---------------------------------------------------------------------------
// VFX particle hitbox source sync (deterministic sampler)
//
// Server counterpart of the client's updateParticleHitboxSources: instead of
// reading a live ParticleSystem pool, evaluate pg::evaluateParticles at the
// VFX-local time. Spawn parameters are keyed by (seed, stream, id), so they
// equal the casting client's particles regardless of tick rates.
// ---------------------------------------------------------------------------

void SkillSystem::updateParticleHitboxSources(SkillDispatchContext& ctx) {
    for (int si = 0; si < static_cast<int>(particleSources_.size()); ++si) {
        ParticleHitboxSource& src = particleSources_[si];
        if (!src.active) continue;

        const SkillInstance& inst = instancePool_.instances[src.instanceIdx];
        const SkillInstance::VfxAnchor* anchor =
            inst.active ? inst.findVfxAnchor(src.vfxId) : nullptr;

        // No config (unmigrated effect) or VFX not started yet: no hitboxes.
        if (!src.gameplayCfg || !src.vdef || !anchor) {
            for (int h : src.hitboxHandles) freeHitbox(h);
            src.hitboxHandles.clear();
            continue;
        }

        const pg::GroundQuery  ground{ ctx.groundHeight, ctx.groundNormal };

        // Resolver over the effect composition: lets the sampler follow
        // sub-emitter chains (chainParent) to root systems recursively. When this
        // source has been re-anchored (its parent projectile was consumed on a
        // non-penetrating hit), force the target system to a root play()-burst so
        // the death-child effect (e.g. explosion) spawns at the recorded impact
        // point instead of the analytic chain's max-range death.
        const SkillAsset::VfxDef* vdef = src.vdef;
        const std::uint32_t effectSeed = src.effectSeed;
        const int            anchorSys  = src.systemIdx;
        const bool           reAnchored = src.consumeAnchor.valid;
        auto resolver = [vdef, effectSeed, anchorSys, reAnchored](int idx) -> pg::SystemRef {
            pg::SystemRef r;
            if (idx < 0 || idx >= static_cast<int>(vdef->systems.size()))
                return r;
            const SkillAsset::VfxSystemDef& s =
                vdef->systems[static_cast<std::size_t>(idx)];
            r.cfg          = s.gameplayCfg.get();
            r.seed         = pg::mixSeed(effectSeed, static_cast<std::uint32_t>(idx));
            r.mode         = (s.playMode == 1) ? pg::PlayMode::Continuous
                                               : pg::PlayMode::Emit;
            r.chainParent  = s.chainParent;
            r.chainOnBirth = s.chainOnBirth;
            if (reAnchored && idx == anchorSys) {
                r.chainParent  = -1;                  // detach from the consumed parent
                r.chainOnBirth = false;
                r.mode         = pg::PlayMode::Emit;   // single play()-style burst at the anchor
            }
            return r;
        };

        const float tReal = reAnchored
            ? (inst.elapsed - src.consumeAnchor.time).count() * 0.001f
            : (inst.elapsed - anchor->startTime).count() * 0.001f;
        const pg::EmitterFrame frame{
            reAnchored ? src.consumeAnchor.pos : anchor->pos,
            anchor->orient
        };
        pg::evaluateSystemParticles(resolver, src.systemIdx, frame, tReal,
                                    ground, anchor->conformOverride,
                                    kMaxGameplayParticles, particleScratch_);

        // Non-penetrating: drop particles already consumed by an authoritative hit
        // so their hitboxes disappear (the projectile stops after impact).
        if (!src.consumedKeys.empty()) {
            particleScratch_.erase(
                std::remove_if(particleScratch_.begin(), particleScratch_.end(),
                    [&src](const pg::ParticleState& p) {
                        const std::uint64_t k =
                            (static_cast<std::uint64_t>(p.stream) << 32) | p.id;
                        return src.consumedKeys.count(k) != 0;
                    }),
                particleScratch_.end());
        }

        // Reuse existing per-particle hitbox handles; only grow/shrink the count.
        const int needed = static_cast<int>(particleScratch_.size());
        while (static_cast<int>(src.hitboxHandles.size()) > needed) {
            freeHitbox(src.hitboxHandles.back());
            src.hitboxHandles.pop_back();
        }
        while (static_cast<int>(src.hitboxHandles.size()) < needed) {
            int hi = allocHitbox();
            if (hi < 0) break;

            AttachedHitbox& hb        = hitboxPool_[hi];
            hb.active                 = true;
            hb.localOBBs              = src.templateOBBs;
            hb.onHit                  = src.onHit;
            hb.targetMask             = src.targetMask;
            hb.ownerObjectId          = src.ownerObjectId;
            hb.instanceIdx            = src.instanceIdx;
            hb.slot                   = src.slot;
            hb.hitGroup               = src.hitGroup;
            hb.hitGroupCooldownMs     = src.hitGroupCooldownMs;
            hb.applyAttachRotation    = src.applyRotation;
            hb.penetrate              = src.penetrate;
            hb.particleSourceIdx      = si;
            hb.resolvedAttach.type    = AttachType::VFXParticle;
            hb.worldOBBs.resize(src.templateOBBs.size());

            src.hitboxHandles.push_back(hi);
        }

        // Refresh per-particle world transforms (mirrors the client math).
        for (int pi = 0; pi < static_cast<int>(src.hitboxHandles.size()); ++pi) {
            AttachedHitbox& hb = hitboxPool_[src.hitboxHandles[pi]];
            const pg::ParticleState& p = particleScratch_[pi];

            // Record the stable spawn key + world pos so a non-penetrating hit can
            // mark this exact particle consumed and anchor its death-child at impact.
            hb.particleStream = p.stream;
            hb.particleId     = p.id;
            hb.particlePos    = p.pos;

            const float sz = src.useParticleSize ? p.sizeNow : 1.f;

            hb.worldOBBs.resize(src.templateOBBs.size());
            if (src.applyRotation) {
                // Replicates buildParticleMeshGeometry rotation:
                // angularAngle3D euler + baseRotation (same as client).
                mu::Mat4x4 particleRot = mu::rotateXH(mu::Radian{ p.angularAngle3D.x() })
                                       * mu::rotateYH(mu::Radian{ p.angularAngle3D.y() })
                                       * mu::rotateZH(mu::Radian{ p.angularAngle3D.z() })
                                       * p.baseRotation;
                mu::NQuat particleOrient(mu::quatRotMat(particleRot));
                for (int oi = 0; oi < static_cast<int>(src.templateOBBs.size()); ++oi) {
                    const OBB& tmpl = src.templateOBBs[oi];
                    mu::Vec3 rotatedCenter = mu::Vec3(mu::Vec4(tmpl.center, 0.f) * particleRot);
                    hb.worldOBBs[oi].center      = p.pos + rotatedCenter;
                    hb.worldOBBs[oi].halfExtents = tmpl.halfExtents * sz;
                    hb.worldOBBs[oi].orient      = tmpl.orient * particleOrient;
                }
            } else {
                // Position follows the particle; orientation is fixed.
                for (int oi = 0; oi < static_cast<int>(src.templateOBBs.size()); ++oi) {
                    const OBB& tmpl = src.templateOBBs[oi];
                    hb.worldOBBs[oi].center      = p.pos + tmpl.center;
                    hb.worldOBBs[oi].halfExtents = tmpl.halfExtents * sz;
                    hb.worldOBBs[oi].orient      = tmpl.orient;
                }
            }
            hb.worldAABB = unionAABBOfOBBs(hb.worldOBBs, kHitboxAABBMargin);
        }
    }
}


// ---------------------------------------------------------------------------
// Attachment resolve and transform
// ---------------------------------------------------------------------------

ResolvedAttach SkillSystem::resolveAttach(const AttachTarget&        attach,
                                          const Object&              owner,
                                          const SkillDispatchContext& /*ctx*/) const {
    ResolvedAttach ra{};
    ra.type = attach.type;

    if (attach.type == AttachType::Bone) {
        if (!owner.model() || owner.model()->skeleton.empty()) { ra.boneIdx = -1; return ra; }
        const auto& nameToIdx = owner.model()->skeleton.nameToIdx;
        auto it = nameToIdx.find(attach.targetName);
        ra.boneIdx = (it != nameToIdx.end()) ? it->second : -1;
        return ra;
    }

    // VFXParticle: no pSystem on server
    return ra;
}

mu::Mat4x4 SkillSystem::computeAttachTransform(const Object& owner,
                                                const AttachedHitbox& hb) const {
    const i32t idx = hb.resolvedAttach.boneIdx;
    if (idx >= 0) {
        const auto& boneXforms = owner.boneWorldXforms();
        if (idx < static_cast<i32t>(boneXforms.size()))
            return boneXforms[static_cast<std::size_t>(idx)];
    }
    // Fallback: owner world transform without bone specificity
    return mu::Mat4x4(owner.orient()) * mu::translate(owner.pos());
}

// ---------------------------------------------------------------------------
// Collision detection
// ---------------------------------------------------------------------------

void SkillSystem::checkHitboxCollisions(SkillDispatchContext& ctx) {
    if (!ctx.objectById) return;

    // 1. Gather damageable targets (once per frame, not per hitbox).
    targetEntries_.clear();
    for (int oi = 0; oi < ctx.objectByIdSize; ++oi) {
        Object* t = ctx.objectById[oi];
        if (!t)                     continue;
        if (!t->canReceiveDamage()) continue;
        if (t->hp() <= 0)           continue;

        const BVH& bvh = t->body().worldBVH();
        AABB aabb = !bvh.empty() ? bvh.nodes[0].bounds
                                 : AABB{ t->pos(), mu::Vec3(1.f, 1.f, 1.f) };
        targetEntries_.push_back({ aabb, t, factionBit(t->faction()) });
    }
    if (targetEntries_.empty()) return;

    // 2. Gather active hitboxes.
    hitboxEntries_.clear();
    for (int hi = 0; hi < (int)hitboxPool_.size(); ++hi) {
        const AttachedHitbox& hb = hitboxPool_[hi];
        if (!hb.active || hb.worldOBBs.empty()) continue;
        hitboxEntries_.push_back({ hb.worldAABB, hi, hb.targetMask });
    }
    if (hitboxEntries_.empty()) return;

    // 3. Object–Hitbox broad phase -> candidate (hitbox, target) pairs.
    skillBroadPhase_.build(hitboxEntries_, targetEntries_);

    // 4. Narrow phase on candidate pairs only.
    const int poolSize = static_cast<int>(instancePool_.instances.size());
    for (const auto& c : skillBroadPhase_.candidates()) {
        const AttachedHitbox& hb = hitboxPool_[c.hitboxIdx];
        if (!hb.active) continue;

        Object*    target   = c.target;
        const i32t targetId = static_cast<i32t>(target->getId());
        if (targetId == hb.ownerObjectId) continue;

        // Hit group cooldown
        if (hb.instanceIdx >= 0 && hb.instanceIdx < poolSize) {
            const SkillInstance& inst = instancePool_.instances[hb.instanceIdx];
            auto git = inst.hitGroups.find(hb.hitGroup);
            if (git != inst.hitGroups.end()) {
                auto tit = git->second.lastHitByTarget.find(targetId);
                if (tit != git->second.lastHitByTarget.end()) {
                    float since = inst.elapsed.count() - tit->second.count();
                    if (hb.hitGroupCooldownMs <= 0.f || since < hb.hitGroupCooldownMs)
                        continue;
                }
            }
        }

        // BVH hit check — capture damageCoeff from the hit leaf node.
        // Per-region damageCoeff is authored for player->monster hits (head =2.0,
        // weak points, etc.). Monster->player hits use FLAT damage: the player BVH
        // carries no meaningful per-region coeffs and some regions are 0, which would
        // null the damage while the knockback impulse still lands — exactly the
        // "looks like a hit but no damage" symptom for certain monsters/parts.
        const bool flatDamage = (target->faction() == Faction::Players);
        const BVH& bvh   = target->body().worldBVH();
        float      coeff = 1.0f;
        bool       hit   = false;
        for (const OBB& obb : hb.worldOBBs) {
            BVHHitResult r = collides(bvh, obb);
            if (r.hit) { coeff = flatDamage ? 1.0f : r.damageCoeff; hit = true; break; }
        }
        if (hit) {
            pendingHits_.push_back({ c.hitboxIdx, targetId, coeff });
            if (hb.instanceIdx >= 0 && hb.instanceIdx < poolSize) {
                SkillInstance& inst = instancePool_.instances[hb.instanceIdx];
                inst.hitGroups[hb.hitGroup].lastHitByTarget[targetId] = inst.elapsed;
            }
        }
    }
}

void SkillSystem::processHitResults(SkillDispatchContext& ctx) {
    if (!ctx.evList) return;

    for (const HitResult& hr : pendingHits_) {
        AttachedHitbox& hb = hitboxPool_[hr.hitboxIdx];
        if (!hb.active) continue;

        const OnHitDef& oh = hb.onHit;

        const SkillInstance* instPtr = nullptr;
        if (hb.instanceIdx >= 0 && hb.instanceIdx < static_cast<int>(instancePool_.instances.size())) {
            instPtr = &instancePool_.instances[hb.instanceIdx];
        }

        // A zero-damage hitbox (e.g. a non-penetrating projectile trigger whose
        // payload is the spawned burst) carries no damage event -- only its consume
        // side effect below.
        if (!ctx.clientPredictionOnly && instPtr && oh.damage != 0) {
            holdEvent((*ctx.evList), EvSkillHit{
                hr.targetObjectId,
                static_cast<int32>(oh.damage * hr.damageCoeff * instPtr->damageScale),
                instPtr->ownerObjectId,
                instPtr->asset ? instPtr->asset->id : 0u
            });
        }

        // (PlayVFX hit effect: no-op on server)

        if (oh.impulseStrength > 0.f) {
            Object* tgt   = lookupObject(ctx, hr.targetObjectId);
            Object* owner = lookupObject(ctx, hb.ownerObjectId);
            if (tgt && owner) {
                mu::Mat4x4 orientMat(owner->orient());
                mu::Vec3 impulseJ = mu::Vec3(mu::Vec4(oh.impulseDirLocal, 0.f) * orientMat)
                                    * oh.impulseStrength;
                tgt->body().applyImpulse(impulseJ, tgt->pos());
                tgt->onHitImpulse();
            }
        }

        // Non-penetrating: mark this source particle consumed (its hitbox stops
        // appearing, so the projectile no longer hits) and re-anchor any direct
        // death-child source (e.g. the explosion) at the impact point + time so the
        // authoritative blast matches the casting client's Death-chain explosion.
        if (!hb.penetrate && hb.particleSourceIdx >= 0
            && hb.particleSourceIdx < static_cast<int>(particleSources_.size())) {
            ParticleHitboxSource& src = particleSources_[hb.particleSourceIdx];
            src.consumedKeys.insert(
                (static_cast<std::uint64_t>(hb.particleStream) << 32) | hb.particleId);

            const Milliseconds now = instPtr ? instPtr->elapsed : Milliseconds{ 0.f };
            for (ParticleHitboxSource& other : particleSources_) {
                if (!other.active || other.instanceIdx != src.instanceIdx) continue;
                if (other.vfxId != src.vfxId || !other.vdef) continue;
                if (other.systemIdx >= other.vdef->systems.size()) continue;
                if (other.vdef->systems[other.systemIdx].chainParent == src.systemIdx
                    && !other.consumeAnchor.valid) {
                    other.consumeAnchor = { hb.particlePos, now, true };
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Pool helpers
// ---------------------------------------------------------------------------

int SkillSystem::allocHitbox() {
    int idx;
    if (!hitboxFreeList_.empty()) {
        idx = hitboxFreeList_.back();
        hitboxFreeList_.pop_back();
        hitboxPool_[idx] = AttachedHitbox{};
    } else {
        idx = static_cast<int>(hitboxPool_.size());
        hitboxPool_.emplace_back();
    }
    return idx;
}

void SkillSystem::freeHitbox(int idx) {
    if (idx < 0 || idx >= (int)hitboxPool_.size()) return;
    if (!hitboxPool_[idx].active) return;  // already free; avoid double free-list entry
    hitboxPool_[idx].active = false;
    hitboxFreeList_.push_back(idx);
}

int SkillSystem::allocParticleSource() {
    int idx;
    if (!particleSourceFreeList_.empty()) {
        idx = particleSourceFreeList_.back();
        particleSourceFreeList_.pop_back();
        particleSources_[idx] = ParticleHitboxSource{};
    } else {
        idx = static_cast<int>(particleSources_.size());
        particleSources_.emplace_back();
    }
    return idx;
}

void SkillSystem::freeParticleSource(int idx) {
    if (idx < 0 || idx >= (int)particleSources_.size()) return;
    ParticleHitboxSource& src = particleSources_[idx];
    if (!src.active) return;
    for (int h : src.hitboxHandles) freeHitbox(h);
    src.hitboxHandles.clear();
    src.active = false;
    particleSourceFreeList_.push_back(idx);
}

// ---------------------------------------------------------------------------
// Object lookup
// ---------------------------------------------------------------------------

Object* SkillSystem::lookupObject(const SkillDispatchContext& ctx, i32t id) const {
    if (!ctx.objectById || id < 0 || id >= ctx.objectByIdSize) return nullptr;
    return ctx.objectById[id];
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

void SkillSystem::collectActiveOBBs(std::vector<OBB>& out) const {
    for (const AttachedHitbox& hb : hitboxPool_) {
        if (!hb.active) continue;
        for (const OBB& obb : hb.worldOBBs)
            out.push_back(obb);
    }
}
