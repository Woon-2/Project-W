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
#include "particleGameplay.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <cstdint>

class Object;

// ---------------------------------------------------------------------------
// Resolved attach handle (no strings; filled once at SpawnHitbox dispatch)
// ---------------------------------------------------------------------------

struct ResolvedAttach {
    AttachType type          = AttachType::Bone;
    i32t       boneIdx       = -1;
    bool       applyAimPitch = true;  // Body: tilt the rest frame by the caster's aim pitch
    // VFXParticle: pSystem always null on server
};

// ---------------------------------------------------------------------------
// Active hitbox (lives in the flat pool inside SkillSystem)
// ---------------------------------------------------------------------------

struct AttachedHitbox {
    std::vector<OBB> worldOBBs;
    std::vector<OBB> localOBBs;
    AABB             worldAABB            {};   // union AABB of worldOBBs (broad phase)
    u32t             targetMask           = 0;   // faction bits this hitbox may damage (hostileMask of owner)
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
    bool             penetrate            = true;  // VFXParticle: false = consume source particle on hit
    // VFXParticle: the source particle's stable spawn key + world pos this frame
    // (set by updateParticleHitboxSources). Used by processHitResults to mark the
    // particle consumed and to anchor a death-child effect (e.g. explosion) at impact.
    std::uint32_t    particleStream       = 0;
    std::uint32_t    particleId           = 0;
    mu::Vec3         particlePos          {};
};

// ---------------------------------------------------------------------------
// VFX particle hitbox source
// Structurally mirrors client. Instead of reading a live ParticleSystem, the
// server evaluates the deterministic gameplay sampler (pg::evaluateParticles)
// with the per-cast seed shared over C_SkillStart, so hitboxes land exactly
// where the casting client renders its particles.
// ---------------------------------------------------------------------------

struct ParticleHitboxSource {
    std::vector<OBB> templateOBBs;
    OnHitDef         onHit;
    u32t             targetMask            = 0;   // faction bits this source's hitboxes may damage
    i32t             ownerObjectId         = -1;
    i32t             instanceIdx           = -1;
    u8t              slot                  = 0;
    u8t              hitGroup              = 0;
    float            hitGroupCooldownMs    = 0.f;
    bool             active                = false;
    bool             useParticleSize       = false;
    bool             applyRotation         = true;
    bool             penetrate             = true;  // false = destroy source particle on first hit
    std::vector<int> hitboxHandles;

    // Non-penetrating: stable spawn keys ((stream<<32)|id) of particles consumed
    // by an authoritative hit. Their hitboxes are skipped from then on, so a
    // consumed projectile stops hitting (server is authoritative for damage).
    std::unordered_set<std::uint64_t> consumedKeys;

    // Set when a parent projectile is consumed (this source is its death-child,
    // e.g. an explosion): re-anchor the child as a root burst at the impact point
    // and time instead of the analytic chain (which would land at max range).
    struct ConsumeAnchor {
        mu::Vec3     pos {};
        Milliseconds time { 0.f };
        bool         valid = false;
    } consumeAnchor;

    // Deterministic sampler binding (set at SpawnHitbox dispatch).
    // gameplayCfg/vdef point into the shared asset registry (boot-built,
    // immutable, program lifetime). The resolver built per frame from vdef
    // lets the sampler follow sub-emitter chains (chainParent) recursively.
    const pg::GameplayConfig*     gameplayCfg = nullptr;  // target system's cfg (gate)
    const SkillAsset::VfxDef*     vdef        = nullptr;
    std::uint32_t                 effectSeed  = 0;  // mixSeed(inst.seed, vfxId)
    u8t                           systemIdx   = 0;
    u8t                           vfxId       = 0;
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

    // Per-cast deterministic seed received via C_SkillStart (mirrors client).
    u32t              seed          = 0;

    // Per-caster damage multiplier applied on top of oh.damage * damageCoeff.
    // Lets a strong caster (e.g. a tactical boss) reuse a baseline monster skill
    // while still hitting for its own (higher) damage. 1.0 = unchanged.
    float             damageScale   = 1.0f;

    // World anchor captured at skill start (caster position + yaw). Used by
    // AttachType::Ground hitboxes to place planted, terrain-snapped OBBs.
    // Mirrors client SkillInstance::castAnchor for identical hit positions.
    struct CastAnchor {
        mu::Vec3 pos   = { 0.f, 0.f, 0.f };
        float    yaw   = 0.f;     // radians, about world up
        bool     valid = false;
    } castAnchor;

    // Registered ground anchors (SetGroundAnchor event). Mirrors client. A terrain-snapped
    // frame that multiple separate Ground-attach hitboxes reference (attach.groundAnchorRef),
    // so a cluster of independently-configured hitboxes shares one impact point.
    static constexpr int kMaxGroundAnchors = 4;
    struct GroundAnchor {
        mu::Vec3  pos    = { 0.f, 0.f, 0.f };
        mu::NQuat orient = {};       // yaw (* terrain align)
        bool      valid  = false;
    };
    GroundAnchor groundAnchors[kMaxGroundAnchors];

    // slot -> hitboxPool_ index (bone attach); -1 = empty
    std::vector<int> boneHitboxBySlot;
    // slot -> particleSources_ index (VFXParticle attach); -1 = empty
    std::vector<int> particleSourceBySlot;

    // PlayVFX anchors: world transform + timeline start time per vfxId.
    // Consumed by the deterministic particle hitbox sampler. startTime uses
    // the timeline event time (not dispatch time) so elapsedMs fast-forward
    // stays consistent with the casting client's local playback.
    struct VfxAnchor {
        mu::Vec3          pos    = { 0.f, 0.f, 0.f };
        mu::Mat4x4        orient = mu::Mat4x4{};
        Milliseconds      startTime { 0.f };
        pg::GroundConform conformOverride = pg::GroundConform::None;
        u8t               vfxId  = 0;
        bool              valid  = false;
    };
    std::vector<VfxAnchor> vfxAnchors;

    const VfxAnchor* findVfxAnchor(u8t vfxId) const {
        for (const VfxAnchor& a : vfxAnchors)
            if (a.valid && a.vfxId == vfxId) return &a;
        return nullptr;
    }
    void setVfxAnchor(u8t vfxId, const mu::Vec3& pos, const mu::Mat4x4& orient,
                      Milliseconds startTime, pg::GroundConform conform) {
        for (VfxAnchor& a : vfxAnchors) {
            if (a.vfxId == vfxId) {
                a = VfxAnchor{ pos, orient, startTime, conform, vfxId, true };
                return;
            }
        }
        vfxAnchors.push_back(VfxAnchor{ pos, orient, startTime, conform, vfxId, true });
    }

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
        vfxAnchors.clear();
        for (auto& a : groundAnchors) a.valid = false;
        // Keep hitGroups entries (and their bucket capacity) for reuse; just
        // empty the per-target records. Avoids per-cast unordered_map churn.
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

    int  alloc(const SkillAsset* asset, i32t ownerObjectId);
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
// Dispatch context (one per frame)
// ---------------------------------------------------------------------------

struct SkillDispatchContext {
    EventList*       evList         = nullptr;

    // Sparse: objectById[id] is the Object with that ID, or nullptr.
    Object**         objectById     = nullptr;
    int              objectByIdSize = 0;

    // Terrain query for AttachType::Ground hitboxes. Bound by Room to
    // groundHeightAtWorld / worldTerrain_->normalAtWorld. Empty = no terrain.
    std::function<float(float, float)>    groundHeight;
    std::function<mu::Vec3(float, float)> groundNormal;

    // Always false on server (server is authoritative).
    bool             clientPredictionOnly = false;
};

// ---------------------------------------------------------------------------
// Skill system
// ---------------------------------------------------------------------------

class SkillSystem {
public:
    // 부팅 시 1회 컴파일된 공유 레지스트리를 참조로 바인딩(소유하지 않음).
    // 레지스트리는 AssetManager가 소유하며 프로그램 수명 동안 주소가 안정적이어야
    // 한다(SkillInstance::asset이 그 원소를 가리킨다).
    void bindRegistry(const std::vector<SkillAsset>* registry);

    const SkillAsset* findAsset(std::string_view name) const;
    const SkillAsset* findAsset(u32t id) const;

    // `seed` is the caster-generated per-cast seed (C_SkillStart::skillSeed);
    // it drives the deterministic VFXParticle hitbox sampler.
    int startSkill(u32t assetId, i32t ownerObjectId, SkillDispatchContext& ctx,
                   u32t seed = 0);
    // `anchorPosOverride`: when non-null, the cast anchor is planted at this world position
    // instead of the caster's own (yaw still comes from the caster). Lets an NPC drop a
    // Ground-attach skill under a distant target. Null = every existing skill's behaviour.
    int startSkill(u32t assetId, i32t ownerObjectId, SkillDispatchContext& ctx,
                   Milliseconds initialElapsed, u32t seed = 0, float damageScale = 1.0f,
                   const mu::Vec3* anchorPosOverride = nullptr);

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

    const std::vector<SkillAsset>*    assetRegistry_ = nullptr;   // 공유, 비소유
    SkillInstancePool                 instancePool_;
    std::vector<AttachedHitbox>       hitboxPool_;
    std::vector<int>                  hitboxFreeList_;
    std::vector<ParticleHitboxSource> particleSources_;
    std::vector<int>                  particleSourceFreeList_;

    // Deterministic particle hitbox support. Gameplay configs live in the
    // shared asset registry (built once at boot); only evaluation scratch
    // state is per-room.
    static constexpr int kMaxGameplayParticles = 1024;
    std::vector<pg::ParticleState> particleScratch_;

    // Reused per-frame scratch buffers (cleared, not freed).
    SkillBroadPhase                          skillBroadPhase_;
    std::vector<SkillBroadPhase::HitboxEntry> hitboxEntries_;
    std::vector<SkillBroadPhase::TargetEntry> targetEntries_;
    std::vector<int>                          instanceScratch_;  // snapshot of activeList for safe iteration

    struct HitResult {
        int   hitboxIdx;
        i32t  targetObjectId;
        float damageCoeff = 1.0f;  // from BVH node; applied to oh.damage in processHitResults
    };
    std::vector<HitResult> pendingHits_;
};

#endif  // __rs_skill_skillSystem_HPP
