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
                            SkillDispatchContext& ctx) {
    const SkillAsset* asset = findAsset(assetName);
    if (!asset) return -1;
    return startSkill(asset->id, ownerObjectId, ctx);
}

int SkillSystem::startSkill(u32t assetId, i32t ownerObjectId, SkillDispatchContext& ctx) {
    const SkillAsset* asset = nullptr;
    for (const SkillAsset& a : assetRegistry_)
        if (a.id == assetId) { asset = &a; break; }
    if (!asset) return -1;

    int idx = instancePool_.alloc(asset, ownerObjectId);
    if (idx < 0) return -1;

    // Fire t=0 events immediately
    SkillInstance& inst = instancePool_.instances[idx];
    while (inst.nextEventIdx < (int)asset->timeline.size()) {
        if (asset->timeline[inst.nextEventIdx].time > Milliseconds{ 0.f }) break;
        dispatchEvent(asset->timeline[inst.nextEventIdx], inst, ctx);
        ++inst.nextEventIdx;
    }
    return idx;
}

int SkillSystem::startSkill(u32t assetId, i32t ownerObjectId, SkillDispatchContext& ctx,
                            Milliseconds initialElapsed) {
    const SkillAsset* asset = nullptr;
    for (const SkillAsset& a : assetRegistry_)
        if (a.id == assetId) { asset = &a; break; }
    if (!asset) return -1;

    int idx = instancePool_.alloc(asset, ownerObjectId);
    if (idx < 0) return -1;

    SkillInstance& inst = instancePool_.instances[idx];
    inst.elapsed = initialElapsed;

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

            // localOffset is in attach-local space (right=X, up=Y, forward=Z).
            // Vec4 w=1 transforms it as a point, naturally adding the translation.
            const mu::Vec3& off = p.localOffset;
            worldPos    = mu::Vec3(mu::Vec4(off.x(), off.y(), off.z(), 1.f) * baseXform);
            worldOrient = mu::NQuat(mu::quatRotMat(baseXform));
        }
        fx->play(worldPos, worldOrient);
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

    for (const HitResult& hr : pendingHits_) {
        AttachedHitbox& hb = hitboxPool_[hr.hitboxIdx];
        if (!hb.active) continue;

        const OnHitDef& oh = hb.onHit;

        // In online mode server is authoritative for damage; skip local damage event.
        if (!ctx.clientPredictionOnly)
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
    }
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

void SkillSystem::renderDebugHitboxes(DebugBVView& bvView) const {
    for (const AttachedHitbox& hb : hitboxPool_) {
        if (!hb.active) continue;
        for (const OBB& obb : hb.worldOBBs)
            bvView.push(obb, Milliseconds{ 32.f });
    }
}
