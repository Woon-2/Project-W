#include "pch.hpp"
#include "skillSystem.hpp"

#include "../mesh.hpp"       // Bone, Skeleton
#include "../debugBVView.hpp"
#include <algorithm>

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

// Margin added to hitbox broad-phase AABBs to prevent false negatives from
// one-tick staleness between transform updates and collision.
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
// normal. Identity when the normal is (near) vertical. Used by AttachType::Ground
// hitboxes and PlayVFX ground-align placement.
static mu::NQuat alignQuatYToNormal(mu::Vec3 n) {
    const mu::Vec3 up{ 0.f, 1.f, 0.f };
    const mu::Vec3 axis = mu::cross(up, n);
    const float    s2   = mu::dot(axis, axis);
    if (s2 < 1e-8f) return mu::NQuat{};
    const float c     = std::clamp(mu::dot(up, n), -1.f, 1.f);
    const float angle = std::acos(c);
    return mu::NQuat(mu::quatRotMat(mu::rotateH(mu::Radian{ angle }, mu::Vec3(mu::normalize(axis)))));
}

// Capture the caster's world XZ + yaw at skill start. AttachType::Ground hitboxes
// place their OBBs relative to this anchor (then snap to terrain), so they stay
// planted where the skill was cast rather than following the caster's bones.
static void captureCastAnchor(SkillInstance& inst, const Object* owner) {
    inst.castAnchor.valid = false;
    if (!owner) return;
    const mu::Mat4x4 w = owner->renderState().world;
    inst.castAnchor.pos = mu::Vec3(mu::Vec4(0.f, 0.f, 0.f, 1.f) * w);
    const mu::Vec3 fwd  = mu::normalize(mu::Vec3(mu::Vec4(0.f, 0.f, 1.f, 0.f) * w));
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
// SkillSystem -- asset management
// ---------------------------------------------------------------------------

void SkillSystem::registerAssets(std::vector<SkillAsset>&& assets) {
    assetRegistry_ = std::move(assets);
    assetRegistry_.shrink_to_fit();
    for (u32t i = 0; i < static_cast<u32t>(assetRegistry_.size()); ++i)
        if (assetRegistry_[i].id == 0)
            assetRegistry_[i].id = i + 1;
}

const SkillAsset* SkillSystem::findAsset(std::string_view name) const {
    for (const SkillAsset& a : assetRegistry_)
        if (a.name == name) return &a;
    return nullptr;
}

const SkillAsset* SkillSystem::findAsset(u32t id) const {
    for (const SkillAsset& a : assetRegistry_)
        if (a.id == id) return &a;
    return nullptr;
}

// ---------------------------------------------------------------------------
// SkillSystem -- lifecycle
// ---------------------------------------------------------------------------

int SkillSystem::startSkill(std::string_view assetName, i32t ownerObjectId,
                            SkillDispatchContext& ctx, u32t seed) {
    const SkillAsset* asset = findAsset(assetName);
    if (!asset) return -1;
    return startSkill(asset->id, ownerObjectId, ctx, seed);
}

int SkillSystem::startSkill(u32t assetId, i32t ownerObjectId, SkillDispatchContext& ctx,
                            u32t seed) {
    const SkillAsset* asset = nullptr;
    for (const SkillAsset& a : assetRegistry_)
        if (a.id == assetId) { asset = &a; break; }
    if (!asset) return -1;

    int idx = instancePool_.alloc(asset, ownerObjectId);
    if (idx < 0) return -1;

    // Fire t=0 events immediately
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
                            Milliseconds initialElapsed, u32t seed) {
    const SkillAsset* asset = nullptr;
    for (const SkillAsset& a : assetRegistry_)
        if (a.id == assetId) { asset = &a; break; }
    if (!asset) return -1;

    int idx = instancePool_.alloc(asset, ownerObjectId);
    if (idx < 0) return -1;

    SkillInstance& inst = instancePool_.instances[idx];
    inst.elapsed = initialElapsed;
    inst.seed    = seed;
    captureCastAnchor(inst, lookupObject(ctx, ownerObjectId));

    // Fire all events that fall at or before initialElapsed
    while (inst.nextEventIdx < (int)asset->timeline.size()) {
        if (asset->timeline[inst.nextEventIdx].time > initialElapsed) break;
        dispatchEvent(asset->timeline[inst.nextEventIdx], inst, ctx);
        ++inst.nextEventIdx;
    }

    if (asset->totalDuration.count() > 0 && initialElapsed >= asset->totalDuration)
        terminateInstance(inst, ctx);

    return idx;
}

void SkillSystem::bindVfxGameplayConfigs(ParticleEffect* const* vfxById, int vfxByIdSize) {
    for (const SkillAsset& asset : assetRegistry_) {
        for (int vfxId = 0; vfxId < static_cast<int>(asset.vfxDefs.size()); ++vfxId) {
            if (vfxId >= vfxByIdSize || !vfxById[vfxId]) continue;
            ParticleEffect* fx = vfxById[vfxId];

            const SkillAsset::VfxDef& vdef = asset.vfxDefs[vfxId];
            const int count = std::min(static_cast<int>(vdef.systems.size()),
                                       fx->systemCount());
            for (int si = 0; si < count; ++si) {
                if (vdef.systems[si].gameplayCfg)
                    fx->system(si).setGameplayConfig(*vdef.systems[si].gameplayCfg);
            }
        }
    }
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
// SkillSystem -- update
// ---------------------------------------------------------------------------

void SkillSystem::update(Milliseconds dt, SkillDispatchContext& ctx) {
    // Pass 1+2: advance timers and fire timeline events.
    // Snapshot active indices: tickInstance() may terminate (mutating activeList).
    instanceScratch_ = instancePool_.activeList;
    for (int idx : instanceScratch_) {
        SkillInstance& inst = instancePool_.instances[idx];
        if (!inst.active) continue;
        tickInstance(inst, dt, ctx);
    }

    // Pass 3a: rebuild bone hitbox world transforms (uses previous frame's animation)
    updateHitboxes(ctx);

    // Pass 3b: synchronise per-particle hitboxes with current particle counts
    updateParticleHitboxSources(ctx);

    // Pass 4: collision detection
    pendingHits_.clear();
    checkHitboxCollisions(ctx);

    // Pass 5: emit hit events into EventList
    processHitResults(ctx);
}

void SkillSystem::tickInstance(SkillInstance& inst, Milliseconds dt,
                               SkillDispatchContext& ctx) {
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
            // Free existing bone hitbox in this slot
            int oldH = inst.getBoneHandle(slot);
            if (oldH >= 0) { freeHitbox(oldH); }

            int hi = allocHitbox();
            if (hi < 0) break;
            inst.setBoneHandle(slot, hi);

            AttachedHitbox& hb = hitboxPool_[hi];
            hb.active                = true;
            hb.particleSourceIdx     = -1;
            hb.localOBBs             = def.localOBBs;
            hb.worldOBBs.resize(def.localOBBs.size());
            hb.onHit                 = def.onHit;
            hb.ownerObjectId         = inst.ownerObjectId;
            hb.instanceIdx           = static_cast<i32t>(&inst - instancePool_.instances.data());
            hb.slot                  = static_cast<u8t>(slot);
            hb.defIdx                = defIdx;
            hb.hitGroup              = def.hitGroup;
            hb.hitGroupCooldownMs    = def.hitGroupCooldownMs;
            hb.applyAttachRotation   = def.applyAttachRotation;

            if (owner) {
                hb.resolvedAttach = resolveAttach(def.attach, *owner, ctx);
                mu::Mat4x4 xform  = computeAttachTransform(*owner, hb);
                mu::NQuat boneOrient(mu::quatRotMat(xform));
                for (int i = 0; i < (int)hb.localOBBs.size(); ++i) {
                    hb.worldOBBs[i].center      = mu::Vec3(mu::Vec4(hb.localOBBs[i].center, 1.f) * xform);
                    hb.worldOBBs[i].halfExtents  = hb.localOBBs[i].halfExtents;
                    hb.worldOBBs[i].orient       = hb.localOBBs[i].orient;
                    hb.worldOBBs[i].orient      *= boneOrient;
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
            hb.defIdx                 = defIdx;
            hb.hitGroup               = def.hitGroup;
            hb.hitGroupCooldownMs     = def.hitGroupCooldownMs;
            hb.applyAttachRotation    = def.applyAttachRotation;
            hb.resolvedAttach.type    = AttachType::Ground;   // static (skipped by updateHitboxes)
            hb.resolvedAttach.boneIdx = -1;
            hb.resolvedAttach.pSystem = nullptr;

            // Caster yaw rotates each OBB's horizontal offset.
            const mu::Mat4x4 yawRot = mu::rotateYH(mu::Radian{ inst.castAnchor.yaw });
            const mu::NQuat  yawQuat(mu::quatRotMat(yawRot));
            const bool       haveGround = ctx.ground && *ctx.ground;

            const int ref = def.attach.groundAnchorRef;
            const bool useRegisteredAnchor =
                ref >= 0 && ref < SkillInstance::kMaxGroundAnchors && inst.groundAnchors[ref].valid;

            if (useRegisteredAnchor) {
                // Point impact: place OBBs in a registered SetGroundAnchor frame. The frame was
                // snapped to the terrain once; this hitbox is a rigid child of it. center is an
                // offset in the anchor frame (right/up/forward), center.y a lift above it. Each
                // SpawnHitbox keeps its own onHit, so a ring of separately-configured hitboxes
                // (e.g. radial knockback per box) shares one coherent impact point.
                const SkillInstance::GroundAnchor& ga = inst.groundAnchors[ref];
                const mu::Mat4x4 frameRot(ga.orient);
                for (int i = 0; i < (int)hb.localOBBs.size(); ++i) {
                    hb.worldOBBs[i].center      = ga.pos
                        + mu::Vec3(mu::Vec4(hb.localOBBs[i].center, 0.f) * frameRot);
                    hb.worldOBBs[i].halfExtents = hb.localOBBs[i].halfExtents;
                    hb.worldOBBs[i].orient      = hb.localOBBs[i].orient * ga.orient;
                }
            } else {
                // Distributed eruption: each OBB snaps to the terrain at its own XZ, so a
                // grid of pillars conforms to slopes independently. center.y becomes the
                // height above each OBB's own snapped surface point.
                for (int i = 0; i < (int)hb.localOBBs.size(); ++i) {
                    mu::Vec3 wc = inst.castAnchor.pos
                                + mu::Vec3(mu::Vec4(hb.localOBBs[i].center, 0.f) * yawRot);
                    if (haveGround)
                        wc.setComponent(1, ctx.ground->height(wc.x(), wc.z()) + hb.localOBBs[i].center.y());
                    hb.worldOBBs[i].center      = wc;
                    hb.worldOBBs[i].halfExtents = hb.localOBBs[i].halfExtents;
                    hb.worldOBBs[i].orient      = hb.localOBBs[i].orient * yawQuat;
                    if (def.attach.groundAlign && haveGround)
                        hb.worldOBBs[i].orient = hb.worldOBBs[i].orient
                                               * alignQuatYToNormal(ctx.ground->normal(wc.x(), wc.z()));
                }
            }
            hb.worldAABB  = unionAABBOfOBBs(hb.worldOBBs, kHitboxAABBMargin);
            hb.targetMask = owner ? hostileMask(owner->faction()) : 0u;
        } else {
            // VFXParticle: create a ParticleHitboxSource
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
            src.hitboxHandles.clear();

            // Resolve pSystem
            if (owner) {
                ResolvedAttach ra = resolveAttach(def.attach, *owner, ctx);
                src.pSystem = ra.pSystem;
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
        // 공격 애니메이션은 trigger* 직접 호출 대신 EventBus로 일원화한다.
        // (standalone/online 모두 ctx.evList == eventList_, 디스패치 루프가 처리)
        holdEvent((*ctx.evList), EvAttack{ inst.ownerObjectId });
        break;
    }

    case SkillEventType::PlayVFX: {
        const auto& p = ev.payload.playVFX;
        if (!ctx.vfxById || p.vfxId >= static_cast<u8t>(ctx.vfxByIdSize)) break;
        ParticleEffect* fx = ctx.vfxById[p.vfxId];
        if (!fx) break;

        // Per-cast deterministic seed: the server derives the identical
        // per-system seeds (mixSeed(mixSeed(inst.seed, vfxId), systemIdx))
        // for its VFXParticle hitbox sampler. Must precede play().
        fx->setDeterministicSeed(pg::mixSeed(inst.seed, p.vfxId));

        // Let the effect's own emitters interact with the terrain (ground-conform
        // spawn, ground collision) wherever it ends up being played.
        fx->setGroundSampler(ctx.ground);

        // Skill-driven particle ground behaviour (packed in flags bits 3-6). Drives
        // the effect's particles (fall-and-die, conform-to-slope) from the Lua, so
        // the Unity-exported effect JSON needs no ground keys.
        {
            const u8t colOrd  = (p.flags & kPlayVFXParticleCollisionMask) >> kPlayVFXParticleCollisionShift;
            const u8t confOrd = (p.flags & kPlayVFXParticleConformMask)   >> kPlayVFXParticleConformShift;
            if (colOrd != 0 || confOrd != 0)
                fx->setGroundBehavior(static_cast<ps::ParticleCollisionModule::Mode>(colOrd),
                                      static_cast<ps::ShapeModule::GroundConform>(confOrd));
        }

        Object* owner = lookupObject(ctx, inst.ownerObjectId);
        mu::Vec3  worldPos{};
        mu::NQuat worldOrient{};

        if (owner) {
            // Start with the owner's world transform as the base.
            // If a non-empty bone name is specified, override with the bone-to-world transform.
            mu::Mat4x4 baseXform = owner->renderState().world;

            bool hasBoneAttach = (p.attachType == static_cast<u8t>(AttachType::Bone) &&
                                  p.attachTargetName[0] != '\0');
            if (hasBoneAttach && owner->animBlender() && owner->model()) {
                const auto& bones = *owner->model()->skeleton.bones;
                for (i32t bi = 0; bi < (i32t)bones.size(); ++bi) {
                    if (bones[static_cast<size_t>(bi)].name == std::string_view{ p.attachTargetName }) {
                        baseXform = bones[static_cast<size_t>(bi)].toDress
                                  * owner->animBlender()->finalXformData()[static_cast<size_t>(bi)]
                                  * owner->renderState().world;
                        break;
                    }
                }
            }

            // Pure-rotation basis from the attach transform. When yawOnly is set, strip
            // pitch/roll so ground-plane effects (forward circles, fans) stay flat
            // regardless of where the caster is looking.
            mu::Mat4x4 baseRot;
            if (p.flags & kPlayVFXFlagYawOnly) {
                mu::Vec3 fwd = mu::normalize(mu::Vec3(mu::Vec4(0.f, 0.f, 1.f, 0.f) * baseXform));
                baseRot = mu::rotateYH(mu::Radian{ std::atan2(fwd.x(), fwd.z()) });
            } else {
                baseRot = mu::Mat4x4(mu::NQuat(mu::quatRotMat(baseXform)));
            }

            // Local orientation offset (yaw, pitch, roll degrees) -- same convention as OBB orient.
            // Aims the effect relative to the caster (e.g. a sector pointing to the side).
            mu::Mat4x4 eulerOff = mu::rotateRPYH(mu::Degree{ p.localEulerDeg.z() },   // roll
                                                 mu::Degree{ p.localEulerDeg.y() },   // pitch
                                                 mu::Degree{ p.localEulerDeg.x() });  // yaw
            mu::Mat4x4 aim = eulerOff * baseRot;  // pure rotation: local euler then caster orientation

            // localOffset is in the aimed frame (right=X, up=Y, forward=Z), added to the attach origin.
            mu::Vec3 origin = mu::Vec3(mu::Vec4(0.f, 0.f, 0.f, 1.f) * baseXform);
            const mu::Vec3& off = p.localOffset;
            worldPos    = origin + mu::Vec3(mu::Vec4(off.x(), off.y(), off.z(), 0.f) * aim);
            worldOrient = mu::NQuat(mu::quatRotMat(aim));

            // Ground placement: drop the effect onto the terrain surface at its XZ
            // (localOffset.y becomes the lift above ground) and optionally tilt to
            // the surface normal. Replaces the legacy hardcoded ground-snap paths.
            if ((p.flags & kPlayVFXFlagGroundSnap) && ctx.ground && *ctx.ground) {
                worldPos.setComponent(1, ctx.ground->height(worldPos.x(), worldPos.z()) + off.y());
                if (p.flags & kPlayVFXFlagGroundAlign) {
                    aim = aim * mu::Mat4x4(alignQuatYToNormal(
                        ctx.ground->normal(worldPos.x(), worldPos.z())));
                    worldOrient = mu::NQuat(mu::quatRotMat(aim));
                }
            }

            // advanceForwardLocal: explicit particle travel direction (4-arg play). Zero = derive from orient.
            const mu::Vec3& adv = p.advanceForwardLocal;
            if (adv.x() == 0.f && adv.y() == 0.f && adv.z() == 0.f) {
                fx->play(worldPos, worldOrient);
            } else {
                mu::Vec3 worldAdvance = mu::normalize(
                    mu::Vec3(mu::Vec4(adv.x(), adv.y(), adv.z(), 0.f) * aim));
                fx->play(worldPos, aim, worldAdvance);
            }
        }
        else {
            fx->play(worldPos, worldOrient);  // no owner resolved: play at world origin
        }
        break;
    }

    case SkillEventType::ApplyImpulse: {
        const auto& p = ev.payload.applyImpulse;
        Object* owner = lookupObject(ctx, inst.ownerObjectId);
        if (!owner) break;
        mu::Vec3 impulseJ = mu::Vec3(mu::Vec4(p.dirLocal, 0.f) * owner->renderState().world)
                            * p.strength;
        owner->body().applyImpulse(impulseJ, owner->pos());
        break;
    }

    case SkillEventType::CameraShake: {
        const auto& p = ev.payload.cameraShake;
        if (ctx.evList)
            holdEvent((*ctx.evList), EvCameraShake{ p.magnitude, p.duration });
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
        // Register a terrain-snapped frame that subsequent Ground-attach hitboxes (and,
        // by intent, the impact VFX) reference as a shared point of impact. Caster-relative
        // offset rotated by cast yaw, snapped to the terrain, optionally tilted to the normal.
        const auto& p = ev.payload.setGroundAnchor;
        if (p.anchorId >= SkillInstance::kMaxGroundAnchors) break;
        Object* owner = lookupObject(ctx, inst.ownerObjectId);
        if (!inst.castAnchor.valid) captureCastAnchor(inst, owner);

        const mu::Mat4x4 yawRot = mu::rotateYH(mu::Radian{ inst.castAnchor.yaw });
        const mu::NQuat  yawQuat(mu::quatRotMat(yawRot));
        mu::Vec3 world = inst.castAnchor.pos
                       + mu::Vec3(mu::Vec4(p.localOffset, 0.f) * yawRot);
        mu::NQuat alignQuat{};
        if (ctx.ground && *ctx.ground) {
            world.setComponent(1, ctx.ground->height(world.x(), world.z()) + p.localOffset.y());
            if (p.flags & kGroundAnchorFlagAlign)
                alignQuat = alignQuatYToNormal(ctx.ground->normal(world.x(), world.z()));
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
// Hitbox world transform (bone type only)
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
            hb.worldOBBs[oi].center      = mu::Vec3(mu::Vec4(hb.localOBBs[oi].center, 1.f) * xform);
            hb.worldOBBs[oi].halfExtents  = hb.localOBBs[oi].halfExtents;
            hb.worldOBBs[oi].orient       = hb.localOBBs[oi].orient;
            if (hb.applyAttachRotation)
                hb.worldOBBs[oi].orient  *= boneOrient;
        }
        hb.worldAABB = unionAABBOfOBBs(hb.worldOBBs, kHitboxAABBMargin);
    }
}

// ---------------------------------------------------------------------------
// VFX particle hitbox source synchronisation
// Runs after updateHitboxes(). Frees and recreates per-particle hitboxes each
// frame so that newly spawned and just-died particles are automatically handled.
// ---------------------------------------------------------------------------

void SkillSystem::updateParticleHitboxSources(SkillDispatchContext& ctx) {
    for (int si = 0; si < (int)particleSources_.size(); ++si) {
        ParticleHitboxSource& src = particleSources_[si];
        if (!src.active || !src.pSystem) continue;

        const int        needed = src.pSystem->activeCount();
        const Particle*  parts  = src.pSystem->particles();

        // Reuse existing per-particle hitbox handles; only grow/shrink the count.
        while ((int)src.hitboxHandles.size() > needed) {
            freeHitbox(src.hitboxHandles.back());
            src.hitboxHandles.pop_back();
        }
        while ((int)src.hitboxHandles.size() < needed) {
            int hi = allocHitbox();
            if (hi < 0) break;

            // Set the source-constant fields once on allocation.
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
            hb.resolvedAttach.pSystem = src.pSystem;
            hb.worldOBBs.resize(src.templateOBBs.size());

            src.hitboxHandles.push_back(hi);
        }

        // Refresh per-particle world transforms (and broad-phase AABB) each frame.
        for (int pi = 0; pi < (int)src.hitboxHandles.size(); ++pi) {
            AttachedHitbox& hb = hitboxPool_[src.hitboxHandles[pi]];
            const Particle& p  = parts[pi];

            // Record this frame's particle index so a non-penetrating hit can
            // destroy the exact source particle via ParticleSystem::killParticle.
            hb.particleLocalIdx = pi;

            float sz = 1.f;
            if (src.useParticleSize) {
                float t = (p.maxLifetime > 0.f)
                          ? (1.f - p.lifetime / p.maxLifetime)
                          : 0.f;
                sz = p.sizeBegin + (p.sizeEnd - p.sizeBegin) * t;
            }

            hb.worldOBBs.resize(src.templateOBBs.size());
            if (src.applyRotation) {
                // Replicate buildParticleMeshGeometry rotation: angularAngle3D euler + baseRotation.
                mu::Mat4x4 particleRot = mu::rotateXH(mu::Radian{ p.angularAngle3D.x() })
                                       * mu::rotateYH(mu::Radian{ p.angularAngle3D.y() })
                                       * mu::rotateZH(mu::Radian{ p.angularAngle3D.z() })
                                       * p.baseRotation;
                mu::NQuat particleOrient(mu::quatRotMat(particleRot));
                for (int oi = 0; oi < (int)src.templateOBBs.size(); ++oi) {
                    const OBB& tmpl = src.templateOBBs[oi];
                    mu::Vec3 rotatedCenter = mu::Vec3(mu::Vec4(tmpl.center, 0.f) * particleRot);
                    hb.worldOBBs[oi].center      = p.pos + rotatedCenter;
                    hb.worldOBBs[oi].halfExtents  = tmpl.halfExtents * sz;
                    hb.worldOBBs[oi].orient       = tmpl.orient * particleOrient;
                }
            } else {
                // Position follows the particle; orientation is fixed (no particle spin).
                for (int oi = 0; oi < (int)src.templateOBBs.size(); ++oi) {
                    const OBB& tmpl = src.templateOBBs[oi];
                    hb.worldOBBs[oi].center      = p.pos + tmpl.center;
                    hb.worldOBBs[oi].halfExtents  = tmpl.halfExtents * sz;
                    hb.worldOBBs[oi].orient       = tmpl.orient;
                }
            }
            hb.worldAABB = unionAABBOfOBBs(hb.worldOBBs, kHitboxAABBMargin);
        }
    }
}

// ---------------------------------------------------------------------------
// Attachment resolve / transform
// ---------------------------------------------------------------------------

ResolvedAttach SkillSystem::resolveAttach(const AttachTarget&        attach,
                                          const Object&              owner,
                                          const SkillDispatchContext& ctx) const {
    if (attach.type == AttachType::Bone) {
        ResolvedAttach ra{};
        ra.type = AttachType::Bone;
        if (!owner.model()) { ra.boneIdx = -1; return ra; }
        const auto& bones = *owner.model()->skeleton.bones;
        for (i32t i = 0; i < (i32t)bones.size(); ++i) {
            if (bones[static_cast<size_t>(i)].name == attach.targetName) {
                ra.boneIdx = i;
                return ra;
            }
        }
        ra.boneIdx = -1;
        return ra;
    }

    // VFXParticle: look up the ParticleSystem from the effect registry.
    ResolvedAttach ra{};
    ra.type = AttachType::VFXParticle;
    if (ctx.vfxById && attach.vfxId < static_cast<u8t>(ctx.vfxByIdSize)) {
        ParticleEffect* fx = ctx.vfxById[attach.vfxId];
        if (fx && attach.particleSystemIdx < fx->systemCount())
            ra.pSystem = &fx->system(attach.particleSystemIdx);
    }
    return ra;
}

mu::Mat4x4 SkillSystem::computeAttachTransform(const Object& owner,
                                                const AttachedHitbox& hb) const {
    const i32t idx        = hb.resolvedAttach.boneIdx;
    const RenderState& rs = owner.renderState();
    if (idx < 0 || !rs.animBlender || !rs.pModel)
        return rs.world;
    const auto& bones = *rs.pModel->skeleton.bones;
    const Bone& bone  = bones[static_cast<size_t>(idx)];
    return bone.toDress
         * rs.animBlender->finalXformData()[static_cast<size_t>(idx)]
         * rs.world;
}

// ---------------------------------------------------------------------------
// Collision detection
// ---------------------------------------------------------------------------

void SkillSystem::checkHitboxCollisions(SkillDispatchContext& ctx) {
    if (!ctx.objectById) return;

    // 1. Gather live targets (once per frame, not per hitbox).
    targetEntries_.clear();
    for (int oi = 0; oi < ctx.objectByIdSize; ++oi) {
        Object* t = ctx.objectById[oi];
        if (!t)           continue;
        if (t->isDead())  continue;

        const BVH& bvh = t->worldBVH();
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
        const i32t targetId = target->getId();
        if (targetId == hb.ownerObjectId) continue;

        // Hit group cooldown (applies uniformly to bone and particle hitboxes).
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

        const BVH& bvh = target->worldBVH();
        bool hit = false;
        for (const OBB& obb : hb.worldOBBs) {
            if (collides(bvh, obb).hit) { hit = true; break; }
        }

        if (hit) {
            pendingHits_.push_back({ c.hitboxIdx, targetId });
            if (hb.instanceIdx >= 0 && hb.instanceIdx < poolSize) {
                SkillInstance& inst = instancePool_.instances[hb.instanceIdx];
                inst.hitGroups[hb.hitGroup].lastHitByTarget[targetId] = inst.elapsed;
            }
        }
    }
}

void SkillSystem::processHitResults(SkillDispatchContext& ctx) {
    if (!ctx.evList) return;

    pendingParticleKills_.clear();

    for (const HitResult& hr : pendingHits_) {
        AttachedHitbox& hb = hitboxPool_[hr.hitboxIdx];
        if (!hb.active) continue;

        const OnHitDef& oh = hb.onHit;

        // In online mode server is authoritative for damage; skip local damage event.
        // A zero-damage hitbox (e.g. a non-penetrating projectile trigger whose payload
        // is the spawned burst) carries no damage event -- only its consume side effect.
        if (!ctx.clientPredictionOnly && oh.damage != 0)
            holdEvent((*ctx.evList), EvSkillHit{ hr.targetObjectId, oh.damage });

        if (oh.hitVfxId != 0xFF && ctx.vfxById &&
            oh.hitVfxId < static_cast<u8t>(ctx.vfxByIdSize)) {
            ParticleEffect* fx = ctx.vfxById[oh.hitVfxId];
            Object* target = lookupObject(ctx, hr.targetObjectId);
            if (fx && target) fx->play(target->pos());
        }

        if (oh.impulseStrength > 0.f) {
            Object* target = lookupObject(ctx, hr.targetObjectId);
            Object* owner  = lookupObject(ctx, hb.ownerObjectId);
            if (target && owner) {
                mu::Vec3 impulseJ =
                    mu::Vec3(mu::Vec4(oh.impulseDirLocal, 0.f) * owner->renderState().world)
                    * oh.impulseStrength;
                target->body().applyImpulse(impulseJ, target->pos());
            }
        }

        // Non-penetrating particle hitbox: queue its source particle for destruction.
        // Deferred so the swap-remove inside killParticle cannot invalidate other
        // particle indices still referenced by pending hits this frame.
        if (!hb.penetrate && hb.particleSourceIdx >= 0 && hb.particleLocalIdx >= 0)
            pendingParticleKills_.emplace_back(hb.particleSourceIdx, hb.particleLocalIdx);
    }

    if (pendingParticleKills_.empty()) return;

    // Group by the *ParticleSystem* (not source index) and destroy unique indices
    // in descending order, so each killParticle's swap-remove leaves the remaining
    // (smaller) indices valid -- correct even if several sources share one system.
    auto sysOf = [this](int srcIdx) -> ParticleSystem* {
        return particleSources_[srcIdx].pSystem;
    };
    std::sort(pendingParticleKills_.begin(), pendingParticleKills_.end(),
              [&](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                  ParticleSystem* pa = sysOf(a.first);
                  ParticleSystem* pb = sysOf(b.first);
                  if (pa != pb) return pa < pb;
                  return a.second > b.second;  // descending particle index within a system
              });
    ParticleSystem* lastSys = nullptr;
    int             lastIdx = -1;
    for (const auto& [srcIdx, pi] : pendingParticleKills_) {
        ParticleSystem* sys = sysOf(srcIdx);
        if (!sys) continue;
        if (sys == lastSys && pi == lastIdx) continue;  // dedupe (same particle hit >1 target)
        lastSys = sys;
        lastIdx = pi;
        sys->killParticle(pi);
        ++debugStats_.particlesDestroyedOnHit;
    }
    pendingParticleKills_.clear();
}

// ---------------------------------------------------------------------------
// Hitbox pool helpers (vector-based; inactive entries are reused)
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
// Debug: push all active hitbox OBBs to a DebugBVView (short TTL for live update)
// ---------------------------------------------------------------------------

void SkillSystem::renderDebugHitboxes(DebugBVView& bvView, int selectedHitboxIdx) const {
    static const mu::Vec4 kDefaultColor  { 0.f, 1.f, 0.f, 1.f };  // green
    static const mu::Vec4 kSelectedColor { 1.f, 0.85f, 0.1f, 1.f }; // amber

    for (int hi = 0; hi < (int)hitboxPool_.size(); ++hi) {
        const AttachedHitbox& hb = hitboxPool_[hi];
        if (!hb.active) continue;
        const mu::Vec4 color = (hi == selectedHitboxIdx) ? kSelectedColor : kDefaultColor;
        for (const OBB& obb : hb.worldOBBs)
            bvView.push(obb, Milliseconds{ 32.f }, BVPipeline::BVModel::Box, color);
    }
}

// ---------------------------------------------------------------------------
// SkillSystem -- editor support
// ---------------------------------------------------------------------------

int SkillSystem::startSkillAsset(const SkillAsset* asset, i32t ownerObjectId,
                                 SkillDispatchContext& ctx) {
    if (!asset) return -1;

    int idx = instancePool_.alloc(asset, ownerObjectId);
    if (idx < 0) return -1;

    // Fire t=0 events immediately (mirrors startSkill).
    SkillInstance& inst = instancePool_.instances[idx];
    captureCastAnchor(inst, lookupObject(ctx, ownerObjectId));
    while (inst.nextEventIdx < (int)asset->timeline.size()) {
        if (asset->timeline[inst.nextEventIdx].time > Milliseconds{ 0.f }) break;
        dispatchEvent(asset->timeline[inst.nextEventIdx], inst, ctx);
        ++inst.nextEventIdx;
    }
    return idx;
}

void SkillSystem::collectActiveHitboxes(std::vector<ActiveHitboxRef>& out) const {
    out.clear();
    for (int hi = 0; hi < (int)hitboxPool_.size(); ++hi) {
        const AttachedHitbox& hb = hitboxPool_[hi];
        if (!hb.active || hb.resolvedAttach.type != AttachType::Bone) continue;
        if (hb.worldOBBs.empty()) continue;
        out.push_back(ActiveHitboxRef{
            hi, hb.ownerObjectId, hb.slot, hb.defIdx, &hb.worldOBBs });
    }
}

int SkillSystem::pickHitbox(mu::Vec3 rayOrigin, mu::Vec3 rayDir) const {
    Ray   ray{ rayOrigin, mu::Vec3(mu::NVec3(rayDir)) };
    int   best   = -1;
    float bestT  = std::numeric_limits<float>::max();

    for (int hi = 0; hi < (int)hitboxPool_.size(); ++hi) {
        const AttachedHitbox& hb = hitboxPool_[hi];
        if (!hb.active) continue;
        for (const OBB& obb : hb.worldOBBs) {
            RayHit rh = RaycastOBB(obb, ray);
            if (rh.hit && rh.t >= 0.f && rh.t < bestT) {
                bestT = rh.t;
                best  = hi;
            }
        }
    }
    return best;
}

void SkillSystem::setHitboxLocalOBBs(int hitboxIdx, const std::vector<OBB>& localOBBs) {
    if (hitboxIdx < 0 || hitboxIdx >= (int)hitboxPool_.size()) return;
    AttachedHitbox& hb = hitboxPool_[hitboxIdx];
    if (!hb.active) return;
    hb.localOBBs = localOBBs;
    hb.worldOBBs.resize(localOBBs.size());
    // worldOBBs are rebuilt by the next updateHitboxes() pass.
}

void SkillSystem::setHitboxOnHit(int hitboxIdx, const OnHitDef& onHit) {
    if (hitboxIdx < 0 || hitboxIdx >= (int)hitboxPool_.size()) return;
    AttachedHitbox& hb = hitboxPool_[hitboxIdx];
    if (!hb.active) return;
    hb.onHit = onHit;
}
