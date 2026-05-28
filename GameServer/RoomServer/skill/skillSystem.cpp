#include "rspch.hpp"
#include "skillSystem.hpp"
#include "../Model.hpp"
#include "../collision.hpp"
#include <algorithm>
#include <string_view>

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
    // Flag-based reuse; no compaction needed.
}

// ---------------------------------------------------------------------------
// Asset management
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
// Lifecycle
// ---------------------------------------------------------------------------

int SkillSystem::startSkill(u32t assetId, i32t ownerObjectId, SkillDispatchContext& ctx) {
    const SkillAsset* asset = findAsset(assetId);
    if (!asset) return -1;

    int idx = instancePool_.alloc(asset, ownerObjectId);
    if (idx < 0) return -1;

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
    const SkillAsset* asset = findAsset(assetId);
    if (!asset) return -1;

    int idx = instancePool_.alloc(asset, ownerObjectId);
    if (idx < 0) return -1;

    SkillInstance& inst = instancePool_.instances[idx];
    inst.elapsed = initialElapsed;

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
// Update (5 passes — mirrors client)
// ---------------------------------------------------------------------------

void SkillSystem::update(Milliseconds dt, SkillDispatchContext& ctx) {
    auto cnt = 0;
    for (int i = 0; i < SkillInstancePool::kMaxInstances; ++i) {
        SkillInstance& inst = instancePool_.instances[i];
        if (!inst.active) continue;
        tickInstance(inst, dt, ctx);
        ++cnt;
    }
    std::cout << "Skill Instance Count: " << cnt << '\n';

    updateHitboxes(ctx);
    updateParticleHitboxSources(ctx);

    pendingHits_.clear();
    checkHitboxCollisions(ctx);

    processHitResults(ctx);

    instancePool_.compact();
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
            hb.instanceIdx            = static_cast<i32t>(&inst - instancePool_.instances);
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
        } else {
            // VFXParticle: create source entry; pSystem always null on server
            int oldS = inst.getParticleHandle(slot);
            if (oldS >= 0) freeParticleSource(oldS);

            int si = allocParticleSource();
            inst.setParticleHandle(slot, si);

            ParticleHitboxSource& src = particleSources_[si];
            src.active             = true;
            src.templateOBBs       = def.localOBBs;
            src.onHit              = def.onHit;
            src.ownerObjectId      = inst.ownerObjectId;
            src.instanceIdx        = static_cast<i32t>(&inst - instancePool_.instances);
            src.slot               = static_cast<u8t>(slot);
            src.hitGroup           = def.hitGroup;
            src.hitGroupCooldownMs = def.hitGroupCooldownMs;
            src.useParticleSize    = def.useParticleSize;
            src.applyRotation      = def.applyAttachRotation;
            src.hitboxHandles.clear();
            // src.pSystem = nullptr (not present; VFXParticle hitboxes are no-ops on server)
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
        // No-op on server (no ParticleEffect registry)
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
    }
}

// ---------------------------------------------------------------------------
// VFX particle hitbox source sync (no-op on server: pSystem always null)
// ---------------------------------------------------------------------------

void SkillSystem::updateParticleHitboxSources(SkillDispatchContext& ctx) {
    for (ParticleHitboxSource& src : particleSources_) {
        if (!src.active) continue;
        // No ParticleSystem on server; release any stale hitbox handles and leave empty
        for (int h : src.hitboxHandles) freeHitbox(h);
        src.hitboxHandles.clear();
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

    for (int hi = 0; hi < (int)hitboxPool_.size(); ++hi) {
        const AttachedHitbox& hb = hitboxPool_[hi];
        if (!hb.active || hb.worldOBBs.empty()) continue;

        for (int oi = 0; oi < ctx.objectByIdSize; ++oi) {
            Object* target = ctx.objectById[oi];
            if (!target)                                               continue;
            if (static_cast<i32t>(target->getId()) == hb.ownerObjectId) continue;
            if (!target->canReceiveDamage())                           continue;
            if (target->hp() <= 0)                                     continue;

            // Hit group cooldown
            if (hb.instanceIdx >= 0 && hb.instanceIdx < SkillInstancePool::kMaxInstances) {
                const SkillInstance& inst = instancePool_.instances[hb.instanceIdx];
                auto git = inst.hitGroups.find(hb.hitGroup);
                if (git != inst.hitGroups.end()) {
                    auto tit = git->second.lastHitByTarget.find(static_cast<i32t>(target->getId()));
                    if (tit != git->second.lastHitByTarget.end()) {
                        float since = inst.elapsed.count() - tit->second.count();
                        if (hb.hitGroupCooldownMs <= 0.f || since < hb.hitGroupCooldownMs)
                            continue;
                    }
                }
            }

            // BVH hit check — capture damageCoeff from the hit leaf node
            const BVH& bvh      = target->body().worldBVH();
            float      coeff    = 1.0f;
            bool       hit      = false;
            for (const OBB& obb : hb.worldOBBs) {
                BVHHitResult r = collides(bvh, obb);
                if (r.hit) { coeff = r.damageCoeff; hit = true; break; }
            }
            if (hit) {
                pendingHits_.push_back({ hi, static_cast<i32t>(target->getId()), coeff });
                if (hb.instanceIdx >= 0 && hb.instanceIdx < SkillInstancePool::kMaxInstances) {
                    SkillInstance& inst = instancePool_.instances[hb.instanceIdx];
                    inst.hitGroups[hb.hitGroup].lastHitByTarget[static_cast<i32t>(target->getId())] = inst.elapsed;
                }
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
        if (hb.instanceIdx >= 0 && hb.instanceIdx < SkillInstancePool::kMaxInstances) {
            instPtr = &instancePool_.instances[hb.instanceIdx];
        }

        if (!ctx.clientPredictionOnly && instPtr) {
            holdEvent((*ctx.evList), EvSkillHit{
                hr.targetObjectId,
                static_cast<int32>(oh.damage * hr.damageCoeff),
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
    }
}

// ---------------------------------------------------------------------------
// Pool helpers
// ---------------------------------------------------------------------------

int SkillSystem::allocHitbox() {
    for (int i = 0; i < (int)hitboxPool_.size(); ++i) {
        if (!hitboxPool_[i].active) { hitboxPool_[i] = AttachedHitbox{}; return i; }
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
        if (!particleSources_[i].active) { particleSources_[i] = ParticleHitboxSource{}; return i; }
    }
    particleSources_.emplace_back();
    return static_cast<int>(particleSources_.size()) - 1;
}

void SkillSystem::freeParticleSource(int idx) {
    if (idx < 0 || idx >= (int)particleSources_.size()) return;
    ParticleHitboxSource& src = particleSources_[idx];
    for (int h : src.hitboxHandles) freeHitbox(h);
    src.hitboxHandles.clear();
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
// Debug
// ---------------------------------------------------------------------------

void SkillSystem::collectActiveOBBs(std::vector<OBB>& out) const {
    for (const AttachedHitbox& hb : hitboxPool_) {
        if (!hb.active) continue;
        for (const OBB& obb : hb.worldOBBs)
            out.push_back(obb);
    }
}
