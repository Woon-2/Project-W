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

#include <unordered_set>

class Object;
class ParticleEffect;
class Camera;
class DebugBVView;

// ---------------------------------------------------------------------------
// Active hitbox (lives in the flat pool inside SkillSystem)
// ---------------------------------------------------------------------------

// Resolved attach handle (no strings; filled once at SpawnHitbox dispatch).
struct ResolvedAttach {
    HitboxAttachType type    = HitboxAttachType::Bone;
    i32t             boneIdx = -1;         // Bone: resolved index, -1 = root
    ParticleSystem*  pSystem = nullptr;    // VFXParticle: source particle system (unused at hitbox level)
};

struct AttachedHitbox {
    static constexpr int kMaxOBBs = SkillHitboxDef::kMaxOBBs;

    OBB            worldOBBs[kMaxOBBs];   // rebuilt each frame (bone) or set by particle source (VFXParticle)
    OBB            localOBBs[kMaxOBBs];   // OBBs in attachment-local space
    int            obbCount           = 0;
    int            particleSourceIdx  = -1; // -1 = bone hitbox; >= 0 = owned by ParticleHitboxSource
    OnHitDef       onHit;
    ResolvedAttach resolvedAttach;
    i32t           ownerObjectId      = -1;
    i32t           instanceIdx        = -1;  // owning SkillInstance index
    u8t            slot               = 0;
    bool           active             = false;
};

// ---------------------------------------------------------------------------
// VFX particle hitbox source
// One source per VFXParticle SpawnHitbox event.
// Each frame, per-particle hitboxes in hitboxPool_ are synchronised to
// match the current activeCount() of the bound ParticleSystem.
// ---------------------------------------------------------------------------

struct ParticleHitboxSource {
    ParticleSystem*          pSystem       = nullptr;
    OBB                      templateOBB;           // localOBBs[0] from the SkillHitboxDef
    OnHitDef                 onHit;
    i32t                     ownerObjectId = -1;
    i32t                     instanceIdx   = -1;    // owning SkillInstance index
    u8t                      slot          = 0;
    bool                     active        = false;
    std::vector<int>         hitboxHandles;         // hitboxPool_ indices, one per active particle
    std::unordered_set<i32t> hitTargets;            // object IDs already hit (one-hit-per-target)
};

// ---------------------------------------------------------------------------
// Skill instance (one executing skill)
// ---------------------------------------------------------------------------

struct SkillInstance {
    const SkillAsset* asset          = nullptr;
    i32t              ownerObjectId  = -1;
    Milliseconds      elapsed        { 0.f };
    i32t              nextEventIdx   = 0;
    bool              active         = false;
    bool              interrupted    = false;

    // slot -> hitboxPool_ index (bone attach); -1 = empty
    std::vector<int> boneHitboxBySlot;
    // slot -> particleSources_ index (VFXParticle attach); -1 = empty
    std::vector<int> particleSourceBySlot;

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
    }
};

// ---------------------------------------------------------------------------
// Instance pool (static array for zero heap allocation at runtime)
// ---------------------------------------------------------------------------

struct SkillInstancePool {
    static constexpr int kMaxInstances = 128;

    SkillInstance instances[kMaxInstances]{};
    int           count = 0;

    // Returns index of a freshly initialised slot, or -1 if full.
    int alloc(const SkillAsset* asset, i32t ownerObjectId);

    // Marks a slot inactive.
    void free(int idx);

    // No-op: flag-based reuse means no compaction needed.
    void compact();
};

// ---------------------------------------------------------------------------
// Dispatch context (one per frame; groups all external dependencies)
// ---------------------------------------------------------------------------

struct SkillDispatchContext {
    EventList*        evList         = nullptr;

    // Object lookup by ID. Sparse: objectById[id] may be null.
    Object**          objectById     = nullptr;
    int               objectByIdSize = 0;

    // VFX effect instances indexed by vfxId. Pre-allocated at game setup.
    ParticleEffect**  vfxById        = nullptr;
    int               vfxByIdSize    = 0;

    Camera*           camera         = nullptr;
    Timer*            pTimer         = nullptr;
};

// ---------------------------------------------------------------------------
// Skill system
// ---------------------------------------------------------------------------

class SkillSystem {
public:
    // Register compiled assets. Call once at startup before update().
    void registerAssets(std::vector<SkillAsset>&& assets);

    // Returns the asset for the given name, or nullptr.
    const SkillAsset* findAsset(std::string_view name) const;

    // Start executing a skill on the given owner.
    int startSkill(std::string_view assetName, i32t ownerObjectId, SkillDispatchContext& ctx);
    int startSkill(u32t assetId,              i32t ownerObjectId, SkillDispatchContext& ctx);

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

    ResolvedAttach resolveAttach(const HitboxAttachTarget& attach,
                                 const Object&             owner,
                                 const SkillDispatchContext& ctx) const;

    mu::Mat4x4 computeAttachTransform(const Object&         owner,
                                      const AttachedHitbox& hb) const;

    Object* lookupObject(const SkillDispatchContext& ctx, i32t id) const;

    // --- data ---
    std::vector<SkillAsset>           assetRegistry_;   // stable after registerAssets()
    SkillInstancePool                 instancePool_;
    std::vector<AttachedHitbox>       hitboxPool_;       // grows as needed; inactive entries are reused
    std::vector<ParticleHitboxSource> particleSources_;  // one per active VFXParticle SpawnHitbox

    // Pending collisions collected in checkHitboxCollisions(), consumed in processHitResults()
    struct HitResult {
        int  hitboxIdx;
        i32t targetObjectId;
    };
    std::vector<HitResult> pendingHits_;
};

#endif  // __skill_skillSystem_HPP
