#ifndef __skill_skillSystem_HPP
#define __skill_skillSystem_HPP

// Runtime skill system.
// No Lua / sol2 dependency. Assets are pre-compiled by SkillCompiler at startup
// and passed in via registerAssets().
//
// Update loop (call from game.cpp after combatSystem_.update()):
//   skillSystem_.update(dt, ctx);
//
// Starting a skill:
//   skillSystem_.startSkill("SwordSlash", ownerId, ctx);

#include "skillTypes.hpp"
#include "../event.hpp"
#include "../object.hpp"
#include "../particleEffect.hpp"
#include "../camera.hpp"

class Object;
class ParticleEffect;
class Camera;
class DebugBVView;

// ---------------------------------------------------------------------------
// Active hitbox (lives in the flat pool inside SkillSystem)
// ---------------------------------------------------------------------------

// Resolved attach handle (no strings; filled once at SpawnHitbox dispatch).
struct ResolvedAttach {
    AttachType      type    = AttachType::Bone;
    i32t            boneIdx = -1;       // Bone: resolved bone index, -1 = root
    ParticleSystem* pSystem = nullptr;  // VFXParticle: source particle system
};

struct AttachedHitbox {
    std::vector<OBB> worldOBBs;        // rebuilt each frame (bone) or set by particle source (VFXParticle)
    std::vector<OBB> localOBBs;        // OBBs in attachment-local space
    AABB             worldAABB            {};     // union AABB of worldOBBs (broad phase)
    u32t             targetMask           = 0;     // faction bits this hitbox may damage (hostileMask of owner)
    int              particleSourceIdx    = -1;   // -1 = bone hitbox; >= 0 = owned by ParticleHitboxSource
    OnHitDef         onHit;
    ResolvedAttach   resolvedAttach;
    i32t             ownerObjectId        = -1;
    i32t             instanceIdx          = -1;   // owning SkillInstance index
    u8t              slot                 = 0;
    u8t              hitGroup             = 0;    // shared with sibling hitboxes for dedup
    float            hitGroupCooldownMs   = 0.f;  // 0 = hit once; >0 = re-hit after N ms
    bool             active               = false;
    bool             applyAttachRotation  = true; // false = position tracks but orientation ignores attachment
};

// ---------------------------------------------------------------------------
// VFX particle hitbox source
// One source per VFXParticle SpawnHitbox event.
// Each frame, per-particle hitboxes in hitboxPool_ are synchronised to
// match the current activeCount() of the bound ParticleSystem.
// ---------------------------------------------------------------------------

struct ParticleHitboxSource {
    ParticleSystem*  pSystem               = nullptr;
    std::vector<OBB> templateOBBs;         // OBBs in particle-local space (from SkillHitboxDef::localOBBs)
    OnHitDef         onHit;
    u32t             targetMask            = 0;    // faction bits this source's hitboxes may damage
    i32t             ownerObjectId         = -1;
    i32t             instanceIdx           = -1;  // owning SkillInstance index
    u8t              slot                  = 0;
    u8t              hitGroup              = 0;
    float            hitGroupCooldownMs    = 0.f;
    bool             active                = false;
    bool             useParticleSize       = false;
    bool             applyRotation         = true; // false = ignore particle orientation for hitbox OBBs
    std::vector<int> hitboxHandles;        // hitboxPool_ indices, one per active particle
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

    // Per-group hit deduplication state.
    // Keyed by hitGroup id; tracks the last time each target was hit within that group.
    // cooldownMs == 0 means single-hit (never re-hit); >0 means re-hit after cooldown.
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
        // Keep hitGroups entries (and bucket capacity) for reuse; just empty the
        // per-target records. Avoids per-cast unordered_map churn.
        for (auto& [g, st] : hitGroups) st.lastHitByTarget.clear();
    }
};

// ---------------------------------------------------------------------------
// Instance pool (dynamic; index-stable free list + dense active list)
// ---------------------------------------------------------------------------

struct SkillInstancePool {
    std::vector<SkillInstance> instances;   // grows on demand; never shrinks
    std::vector<int>           freeList;     // reusable indices
    std::vector<int>           activeList;   // dense list of active indices

    // Returns index of a freshly initialised slot.
    int alloc(const SkillAsset* asset, i32t ownerObjectId);

    // Marks a slot inactive and recycles it.
    void free(int idx);
};

// ---------------------------------------------------------------------------
// Object–Hitbox broad phase (bipartite sweep-and-prune along X)
// Produces only (hitbox, target) candidate pairs in a single sweep.
// All buffers are reused across frames (cleared, not freed).
// ---------------------------------------------------------------------------

class SkillBroadPhase {
public:
    struct HitboxEntry { AABB aabb; int     hitboxIdx; u32t mask;     };  // mask: faction bits it may hit
    struct TargetEntry { AABB aabb; Object* target;    u32t category; };  // category: target's faction bit
    struct Candidate   { int  hitboxIdx; Object* target; };

    void build(const std::vector<HitboxEntry>& hitboxes,
               const std::vector<TargetEntry>& targets);

    const std::vector<Candidate>& candidates() const { return candidates_; }

private:
    struct Endpoint { float x; int idx; bool isMax; bool isHitbox; };

    std::vector<Endpoint>  endpoints_;
    std::vector<int>       activeHitboxes_;
    std::vector<int>       activeTargets_;
    std::vector<Candidate> candidates_;

    static bool overlapYZ(const AABB& a, const AABB& b);
};

// ---------------------------------------------------------------------------
// Dispatch context (one per frame; groups all external dependencies)
// ---------------------------------------------------------------------------

struct SkillDispatchContext {
    EventList*       evList         = nullptr;

    // Object lookup by ID. Sparse: objectById[id] may be null.
    Object**         objectById     = nullptr;
    int              objectByIdSize = 0;

    // VFX effect instances indexed by vfxId. Pre-allocated at game setup.
    ParticleEffect** vfxById        = nullptr;
    int              vfxByIdSize    = 0;

    Camera*          camera         = nullptr;
    Timer*           pTimer         = nullptr;

    // Online mode: server is authoritative for damage.
    // When true, processHitResults skips EvSkillHit but still applies VFX + impulse.
    bool             clientPredictionOnly = false;
};

// ---------------------------------------------------------------------------
// Skill system
// ---------------------------------------------------------------------------

class SkillSystem {
public:
    // Register compiled assets. Call once at startup before update().
    void registerAssets(std::vector<SkillAsset>&& assets);

    // Returns the asset for the given name or numeric ID, or nullptr.
    const SkillAsset* findAsset(std::string_view name) const;
    const SkillAsset* findAsset(u32t id) const;

    // Start executing a skill on the given owner.
    int startSkill(std::string_view assetName, i32t ownerObjectId, SkillDispatchContext& ctx);
    int startSkill(u32t assetId,              i32t ownerObjectId, SkillDispatchContext& ctx);
    // Overload for late-joiners: starts the skill with an initial elapsed time offset
    // (used by remote clients receiving S_SkillStart mid-skill).
    int startSkill(u32t assetId, i32t ownerObjectId, SkillDispatchContext& ctx,
                   Milliseconds initialElapsed);

    // Interrupt all interruptible skills owned by the given object.
    void interruptAll(i32t ownerObjectId, SkillDispatchContext& ctx);

    // Main update. Call after combatSystem_.update(), before event processing.
    void update(Milliseconds dt, SkillDispatchContext& ctx);

    // True if any skill is currently active for the given owner.
    bool hasActiveSkill(i32t ownerObjectId) const;

    // Debug: push all active hitbox OBBs to a DebugBVView (call each frame in update).
    void renderDebugHitboxes(DebugBVView& bvView) const;

private:
    // --- per-frame passes ---
    void tickInstance(SkillInstance& inst, Milliseconds dt, SkillDispatchContext& ctx);
    void dispatchEvent(const TimelineEvent& ev, SkillInstance& inst, SkillDispatchContext& ctx);
    void updateHitboxes(SkillDispatchContext& ctx);
    void updateParticleHitboxSources(SkillDispatchContext& ctx);
    void checkHitboxCollisions(SkillDispatchContext& ctx);
    void processHitResults(SkillDispatchContext& ctx);

    // --- helpers ---
    void terminateInstance(SkillInstance& inst, SkillDispatchContext& ctx);
    void cleanupHitboxes(SkillInstance& inst);

    int  allocHitbox();
    void freeHitbox(int idx);
    int  allocParticleSource();
    void freeParticleSource(int idx);

    ResolvedAttach resolveAttach(const AttachTarget&        attach,
                                 const Object&              owner,
                                 const SkillDispatchContext& ctx) const;

    mu::Mat4x4 computeAttachTransform(const Object&         owner,
                                      const AttachedHitbox& hb) const;

    Object* lookupObject(const SkillDispatchContext& ctx, i32t id) const;

    // --- data ---
    std::vector<SkillAsset>           assetRegistry_;  // stable after registerAssets()
    SkillInstancePool                 instancePool_;
    std::vector<AttachedHitbox>       hitboxPool_;      // grows as needed; inactive entries are reused
    std::vector<int>                  hitboxFreeList_;
    std::vector<ParticleHitboxSource> particleSources_; // one per active VFXParticle SpawnHitbox
    std::vector<int>                  particleSourceFreeList_;

    // Reused per-frame scratch buffers (cleared, not freed).
    SkillBroadPhase                           skillBroadPhase_;
    std::vector<SkillBroadPhase::HitboxEntry>  hitboxEntries_;
    std::vector<SkillBroadPhase::TargetEntry>  targetEntries_;
    std::vector<int>                           instanceScratch_;  // snapshot of activeList for safe iteration

    struct HitResult {
        int  hitboxIdx;
        i32t targetObjectId;
    };
    std::vector<HitResult> pendingHits_;
};

#endif  // __skill_skillSystem_HPP
