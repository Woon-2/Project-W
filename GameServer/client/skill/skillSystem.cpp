#include "pch.hpp"
#include "skillSystem.hpp"

#include "../mesh.hpp"       // Bone, Skeleton
#include "../debugBVView.hpp"

// ---------------------------------------------------------------------------
// SkillInstancePool
// ---------------------------------------------------------------------------

int SkillInstancePool::alloc(const SkillAsset* asset, i32t ownerObjectId) {
    for (int i = 0; i < kMaxInstances; ++i) {
        if (!instances[i].active) {
            SkillInstance& inst = instances[i];
            inst.asset         = asset;
            inst.ownerObjectId = ownerObjectId;
            inst.elapsed       = Milliseconds{ 0.f };
            inst.nextEventIdx  = 0;
            inst.active        = true;
            inst.interrupted   = false;
            inst.resetSlots();
            return i;
        }
    }
    return -1;
}

void SkillInstancePool::free(int idx) {
    if (idx >= 0 && idx < kMaxInstances)
        instances[idx].active = false;
}

void SkillInstancePool::compact() {
    // Flag-based reuse; no compaction needed for correctness.
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

void SkillSystem::interruptAll(i32t ownerObjectId, SkillDispatchContext& ctx) {
    for (int i = 0; i < SkillInstancePool::kMaxInstances; ++i) {
        SkillInstance& inst = instancePool_.instances[i];
        if (!inst.active || inst.ownerObjectId != ownerObjectId) continue;
        if (inst.asset && !inst.asset->interruptible)               continue;
        terminateInstance(inst, ctx);
    }
}

bool SkillSystem::hasActiveSkill(i32t ownerObjectId) const {
    for (int i = 0; i < SkillInstancePool::kMaxInstances; ++i) {
        const SkillInstance& inst = instancePool_.instances[i];
        if (inst.active && inst.ownerObjectId == ownerObjectId) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// SkillSystem -- update
// ---------------------------------------------------------------------------

void SkillSystem::update(Milliseconds dt, SkillDispatchContext& ctx) {
    // Pass 1+2: advance timers and fire timeline events
    for (int i = 0; i < SkillInstancePool::kMaxInstances; ++i) {
        SkillInstance& inst = instancePool_.instances[i];
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

    instancePool_.compact();
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
    inst.active = false;
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

        if (def.attach.type == HitboxAttachType::Bone) {
            // Free existing bone hitbox in this slot
            int oldH = inst.getBoneHandle(slot);
            if (oldH >= 0) { freeHitbox(oldH); }

            int hi = allocHitbox();
            if (hi < 0) break;
            inst.setBoneHandle(slot, hi);

            AttachedHitbox& hb = hitboxPool_[hi];
            hb.active             = true;
            hb.particleSourceIdx  = -1;
            hb.obbCount           = def.obbCount;
            for (int i = 0; i < def.obbCount; ++i) hb.localOBBs[i] = def.localOBBs[i];
            hb.onHit              = def.onHit;
            hb.ownerObjectId      = inst.ownerObjectId;
            hb.instanceIdx        = static_cast<i32t>(&inst - instancePool_.instances);
            hb.slot               = static_cast<u8t>(slot);

            if (owner) {
                hb.resolvedAttach = resolveAttach(def.attach, *owner, ctx);
                mu::Mat4x4 xform  = computeAttachTransform(*owner, hb);
                mu::NQuat boneOrient(mu::quatRotMat(xform));
                for (int i = 0; i < hb.obbCount; ++i) {
                    hb.worldOBBs[i].center     = mu::Vec3(mu::Vec4(hb.localOBBs[i].center, 1.f) * xform);
                    hb.worldOBBs[i].halfExtents = hb.localOBBs[i].halfExtents;
                    hb.worldOBBs[i].orient     = hb.localOBBs[i].orient;
                    hb.worldOBBs[i].orient    *= boneOrient;
                }
            } else {
                hb.resolvedAttach.type    = HitboxAttachType::Bone;
                hb.resolvedAttach.boneIdx = -1;
                for (int i = 0; i < hb.obbCount; ++i) hb.worldOBBs[i] = hb.localOBBs[i];
            }
        } else {
            // VFXParticle: create a ParticleHitboxSource
            int oldS = inst.getParticleHandle(slot);
            if (oldS >= 0) freeParticleSource(oldS);

            int si = allocParticleSource();
            inst.setParticleHandle(slot, si);

            ParticleHitboxSource& src = particleSources_[si];
            src.active        = true;
            src.templateOBB   = (def.obbCount > 0) ? def.localOBBs[0] : OBB{};
            src.onHit         = def.onHit;
            src.ownerObjectId = inst.ownerObjectId;
            src.instanceIdx   = static_cast<i32t>(&inst - instancePool_.instances);
            src.slot          = static_cast<u8t>(slot);
            src.hitboxHandles.clear();
            src.hitTargets.clear();

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
        Object* owner = lookupObject(ctx, inst.ownerObjectId);
        if (!owner || !owner->animBlender()) break;
        owner->animBlender()->triggerAttack();
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
            bool hasBoneAttach = (p.attachType == static_cast<u8t>(HitboxAttachType::Bone) &&
                                  p.attachTargetName[0] != '\0');
            if (hasBoneAttach && owner->animBlender() && owner->model()) {
                const auto& bones = *owner->model()->skeleton.bones;
                for (i32t bi = 0; bi < (i32t)bones.size(); ++bi) {
                    if (bones[static_cast<size_t>(bi)].name == std::string_view{ p.attachTargetName }) {
                        const Bone& bone = bones[static_cast<size_t>(bi)];
                        mu::Mat4x4 boneToWorld = bone.toDress
                            * owner->animBlender()->finalXformData()[static_cast<size_t>(bi)]
                            * owner->renderState().world;
                        worldPos    = mu::Vec3(mu::Vec4(0.f, 0.f, 0.f, 1.f) * boneToWorld);
                        worldOrient = mu::NQuat(mu::quatRotMat(boneToWorld));
                        break;
                    }
                }
            } else {
                worldPos    = owner->pos();
                worldOrient = owner->orient();
            }
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
        if (hb.resolvedAttach.type != HitboxAttachType::Bone) continue;

        Object* owner = lookupObject(ctx, hb.ownerObjectId);
        if (!owner) { hb.active = false; continue; }

        mu::Mat4x4 xform = computeAttachTransform(*owner, hb);
        mu::NQuat boneOrient(mu::quatRotMat(xform));
        for (int oi = 0; oi < hb.obbCount; ++oi) {
            hb.worldOBBs[oi].center     = mu::Vec3(mu::Vec4(hb.localOBBs[oi].center, 1.f) * xform);
            hb.worldOBBs[oi].halfExtents = hb.localOBBs[oi].halfExtents;
            hb.worldOBBs[oi].orient     = hb.localOBBs[oi].orient;
            hb.worldOBBs[oi].orient    *= boneOrient;
        }
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

        // Free all hitboxes from the previous frame; recreate for the current particle set.
        for (int h : src.hitboxHandles) freeHitbox(h);
        src.hitboxHandles.clear();

        const int        needed = src.pSystem->activeCount();
        const Particle*  parts  = src.pSystem->particles();

        src.hitboxHandles.reserve(needed);
        for (int pi = 0; pi < needed; ++pi) {
            int hi = allocHitbox();
            if (hi < 0) break;

            AttachedHitbox& hb         = hitboxPool_[hi];
            hb.active                  = true;
            hb.obbCount                = 1;
            hb.localOBBs[0]            = src.templateOBB;
            hb.worldOBBs[0]            = src.templateOBB;
            hb.worldOBBs[0].center     = parts[pi].pos;
            hb.onHit                   = src.onHit;
            hb.ownerObjectId           = src.ownerObjectId;
            hb.instanceIdx             = src.instanceIdx;
            hb.slot                    = src.slot;
            hb.particleSourceIdx       = si;
            hb.resolvedAttach.type     = HitboxAttachType::VFXParticle;
            hb.resolvedAttach.pSystem  = src.pSystem;

            src.hitboxHandles.push_back(hi);
        }
    }
}

// ---------------------------------------------------------------------------
// Attachment resolve / transform
// ---------------------------------------------------------------------------

ResolvedAttach SkillSystem::resolveAttach(const HitboxAttachTarget& attach,
                                          const Object& owner,
                                          const SkillDispatchContext& ctx) const {
    if (attach.type == HitboxAttachType::Bone) {
        ResolvedAttach ra{};
        ra.type = HitboxAttachType::Bone;
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
    ra.type = HitboxAttachType::VFXParticle;
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

    for (int hi = 0; hi < (int)hitboxPool_.size(); ++hi) {
        const AttachedHitbox& hb = hitboxPool_[hi];
        if (!hb.active || hb.obbCount == 0) continue;

        const bool isParticle = (hb.particleSourceIdx >= 0);

        for (int oi = 0; oi < ctx.objectByIdSize; ++oi) {
            Object* target = ctx.objectById[oi];
            if (!target)                             continue;
            if (target->getId() == hb.ownerObjectId) continue;
            if (target->isDead())                    continue;

            // For particle hitboxes, skip targets already hit by this source.
            if (isParticle && hb.particleSourceIdx < (int)particleSources_.size()) {
                const auto& src = particleSources_[hb.particleSourceIdx];
                if (src.hitTargets.count(target->getId())) continue;
            }

            const BVH& bvh = target->worldBVH();
            bool hit = false;
            for (int oi2 = 0; oi2 < hb.obbCount && !hit; ++oi2)
                hit = collides(bvh, hb.worldOBBs[oi2]).hit;

            if (hit)
                pendingHits_.push_back({ hi, target->getId() });
        }
    }
}

void SkillSystem::processHitResults(SkillDispatchContext& ctx) {
    if (!ctx.evList) return;

    for (const HitResult& hr : pendingHits_) {
        AttachedHitbox& hb = hitboxPool_[hr.hitboxIdx];
        if (!hb.active) continue;

        const OnHitDef& oh = hb.onHit;
        const bool isParticle = (hb.particleSourceIdx >= 0);

        if (isParticle) {
            // Register hit target in source so it won't be hit again across frames.
            if (hb.particleSourceIdx < (int)particleSources_.size())
                particleSources_[hb.particleSourceIdx].hitTargets.insert(hr.targetObjectId);
            // Do NOT deactivate the hitbox: particle hitboxes are recreated each frame.
        } else {
            // Bone hitbox: single-hit semantics — deactivate after first hit.
            hb.active = false;
            i32t instIdx = hb.instanceIdx;
            if (instIdx >= 0 && instIdx < SkillInstancePool::kMaxInstances) {
                SkillInstance& inst = instancePool_.instances[instIdx];
                int s = hb.slot;
                if (inst.getBoneHandle(s) == hr.hitboxIdx)
                    inst.setBoneHandle(s, -1);
            }
        }

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
    for (int i = 0; i < (int)hitboxPool_.size(); ++i) {
        if (!hitboxPool_[i].active) {
            hitboxPool_[i] = AttachedHitbox{};
            return i;
        }
    }
    hitboxPool_.emplace_back();
    return static_cast<int>(hitboxPool_.size()) - 1;
}

void SkillSystem::freeHitbox(int idx) {
    if (idx >= 0 && idx < (int)hitboxPool_.size())
        hitboxPool_[idx].active = false;
}

int SkillSystem::allocParticleSource() {
    for (int i = 0; i < (int)particleSources_.size(); ++i) {
        if (!particleSources_[i].active) {
            particleSources_[i] = ParticleHitboxSource{};
            return i;
        }
    }
    particleSources_.emplace_back();
    return static_cast<int>(particleSources_.size()) - 1;
}

void SkillSystem::freeParticleSource(int idx) {
    if (idx < 0 || idx >= (int)particleSources_.size()) return;
    ParticleHitboxSource& src = particleSources_[idx];
    for (int h : src.hitboxHandles) freeHitbox(h);
    src.hitboxHandles.clear();
    src.hitTargets.clear();
    src.active = false;
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
        for (int i = 0; i < hb.obbCount; ++i)
            bvView.push(hb.worldOBBs[i], Milliseconds{ 32.f });
    }
}
