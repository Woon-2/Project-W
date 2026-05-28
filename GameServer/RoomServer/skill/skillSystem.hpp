#ifndef __rs_skill_skillSystem_HPP
#define __rs_skill_skillSystem_HPP

// Server-side skill system.
// Structure mirrors client/skill/skillSystem.hpp.
// Key differences from client:
//   - ResolvedAttach: no pSystem (no ParticleSystem on server)
//   - SkillDispatchContext: no vfxById / camera / pTimer
//   - EvSkillHit carries attackerId + skillAssetId for server broadcast
//   - PlayVFX / CameraShake / PlayAnimation are no-ops
//   - checkHitboxCollisions uses body().worldBVH() and applies damageCoeff

#include "skillTypes.hpp"
#include "../event.hpp"
#include "../object.hpp"
#include <unordered_map>
#include <vector>

class Object;

// ---------------------------------------------------------------------------
// Resolved attach handle (no strings; filled once at SpawnHitbox dispatch)
// ---------------------------------------------------------------------------

struct ResolvedAttach {
    AttachType type    = AttachType::Bone;
    i32t       boneIdx = -1;
    // VFXParticle: pSystem always null on server
};

// ---------------------------------------------------------------------------
// Active hitbox (lives in the flat pool inside SkillSystem)
// ---------------------------------------------------------------------------

struct AttachedHitbox {
    std::vector<OBB> worldOBBs;
    std::vector<OBB> localOBBs;
    int              particleSourceIdx    = -1;
    OnHitDef         onHit;
    ResolvedAttach   resolvedAttach;
    i32t             ownerObjectId        = -1;
    i32t             instanceIdx          = -1;
    u8t              slot                 = 0;
    u8t              hitGroup             = 0;
    float            hitGroupCooldownMs   = 0.f;
    bool             active               = false;
    bool             applyAttachRotation  = true;
};

// ---------------------------------------------------------------------------
// VFX particle hitbox source
// Structurally mirrors client. pSystem is always null on server,
// so updateParticleHitboxSources() is effectively a no-op.
// ---------------------------------------------------------------------------

struct ParticleHitboxSource {
    // pSystem always null on server (no ParticleSystem)
    std::vector<OBB> templateOBBs;
    OnHitDef         onHit;
    i32t             ownerObjectId         = -1;
    i32t             instanceIdx           = -1;
    u8t              slot                  = 0;
    u8t              hitGroup              = 0;
    float            hitGroupCooldownMs    = 0.f;
    bool             active                = false;
    bool             useParticleSize       = false;
    bool             applyRotation         = true;
    std::vector<int> hitboxHandles;
};

// ---------------------------------------------------------------------------
// Skill instance (one executing skill)
// ---------------------------------------------------------------------------

struct SkillInstance {
    const SkillAsset* asset         = nullptr;
    i32t              ownerObjectId = -1;
    Milliseconds      elapsed       { 0.f };
    i32t              nextEventIdx  = 0;
    bool              active        = false;
    bool              interrupted   = false;

    // slot -> hitboxPool_ index (bone attach); -1 = empty
    std::vector<int> boneHitboxBySlot;
    // slot -> particleSources_ index (VFXParticle attach); -1 = empty
    std::vector<int> particleSourceBySlot;

    struct HitGroupState {
        std::unordered_map<i32t, Milliseconds> lastHitByTarget;
    };
    std::unordered_map<u8t, HitGroupState> hitGroups;

    int  getBoneHandle(int slot) const {
        return (slot < (int)boneHitboxBySlot.size()) ? boneHitboxBySlot[slot] : -1;
    }
    void setBoneHandle(int slot, int val) {
        if (slot >= (int)boneHitboxBySlot.size()) boneHitboxBySlot.resize(slot + 1, -1);
        boneHitboxBySlot[slot] = val;
    }
    int  getParticleHandle(int slot) const {
        return (slot < (int)particleSourceBySlot.size()) ? particleSourceBySlot[slot] : -1;
    }
    void setParticleHandle(int slot, int val) {
        if (slot >= (int)particleSourceBySlot.size()) particleSourceBySlot.resize(slot + 1, -1);
        particleSourceBySlot[slot] = val;
    }
    void resetSlots() {
        boneHitboxBySlot.clear();
        particleSourceBySlot.clear();
        hitGroups.clear();
    }
};

// ---------------------------------------------------------------------------
// Instance pool (static array)
// ---------------------------------------------------------------------------

struct SkillInstancePool {
    static constexpr int kMaxInstances = 128;

    SkillInstance instances[kMaxInstances]{};
    int           count = 0;

    int  alloc(const SkillAsset* asset, i32t ownerObjectId);
    void free(int idx);
    void compact();
};

// ---------------------------------------------------------------------------
// Dispatch context (one per frame)
// ---------------------------------------------------------------------------

struct SkillDispatchContext {
    EventList*       evList         = nullptr;

    // Sparse: objectById[id] is the Object with that ID, or nullptr.
    Object**         objectById     = nullptr;
    int              objectByIdSize = 0;

    // Always false on server (server is authoritative).
    bool             clientPredictionOnly = false;
};

// ---------------------------------------------------------------------------
// Skill system
// ---------------------------------------------------------------------------

class SkillSystem {
public:
    void registerAssets(std::vector<SkillAsset>&& assets);

    const SkillAsset* findAsset(std::string_view name) const;
    const SkillAsset* findAsset(u32t id) const;

    int startSkill(u32t assetId, i32t ownerObjectId, SkillDispatchContext& ctx);
    int startSkill(u32t assetId, i32t ownerObjectId, SkillDispatchContext& ctx,
                   Milliseconds initialElapsed);

    void interruptAll(i32t ownerObjectId, SkillDispatchContext& ctx);
    void update(Milliseconds dt, SkillDispatchContext& ctx);
    bool hasActiveSkill(i32t ownerObjectId) const;

    // Debug: collect world-space OBBs of all active hitboxes.
    void collectActiveOBBs(std::vector<OBB>& out) const;

private:
    void tickInstance(SkillInstance& inst, Milliseconds dt, SkillDispatchContext& ctx);
    void dispatchEvent(const TimelineEvent& ev, SkillInstance& inst, SkillDispatchContext& ctx);
    void updateHitboxes(SkillDispatchContext& ctx);
    void updateParticleHitboxSources(SkillDispatchContext& ctx);
    void checkHitboxCollisions(SkillDispatchContext& ctx);
    void processHitResults(SkillDispatchContext& ctx);

    void terminateInstance(SkillInstance& inst, SkillDispatchContext& ctx);
    void cleanupHitboxes(SkillInstance& inst);

    int  allocHitbox();
    void freeHitbox(int idx);
    int  allocParticleSource();
    void freeParticleSource(int idx);

    ResolvedAttach resolveAttach(const AttachTarget&        attach,
                                 const Object&              owner,
                                 const SkillDispatchContext& ctx) const;

    mu::Mat4x4 computeAttachTransform(const Object& owner, const AttachedHitbox& hb) const;
    Object*    lookupObject(const SkillDispatchContext& ctx, i32t id) const;

    std::vector<SkillAsset>           assetRegistry_;
    SkillInstancePool                 instancePool_;
    std::vector<AttachedHitbox>       hitboxPool_;
    std::vector<ParticleHitboxSource> particleSources_;

    struct HitResult {
        int   hitboxIdx;
        i32t  targetObjectId;
        float damageCoeff = 1.0f;  // from BVH node; applied to oh.damage in processHitResults
    };
    std::vector<HitResult> pendingHits_;
};

#endif  // __rs_skill_skillSystem_HPP
