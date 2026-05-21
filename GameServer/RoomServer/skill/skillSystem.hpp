#ifndef __rs_skill_skillSystem_HPP
#define __rs_skill_skillSystem_HPP

#include "skillTypes.hpp"
#include <unordered_set>
#include <unordered_map>
#include <limits>

// ---------------------------------------------------------------------------
// Server-side skill system.
// Runs hitbox collision only; no animation/VFX/camera. BoneAttach is
// approximated using player position + a bone-height table.
// ---------------------------------------------------------------------------

struct SkillHitResult {
    i32t attackerId;
    i32t targetId;
    i32t damage;
    u32t skillAssetId;
};

struct ServerSkillTarget {
    i32t     id;
    AABB     worldAABB;   // target bounding box in world space
    int      hp;
};

struct ServerSkillOwner {
    i32t     id;
    mu::Vec3 pos;
    mu::NQuat orient;
};

// ---------------------------------------------------------------------------

struct HitGroupState {
    std::unordered_map<i32t, Milliseconds> lastHitByTarget;
};

struct ServerSkillInstance {
    const SkillAsset* asset      = nullptr;
    i32t              ownerId    = -1;
    Milliseconds      elapsed    { 0.f };
    int               nextEvIdx  = 0;
    bool              active     = false;

    static constexpr int kMaxSlots = 8;
    struct HitSlot {
        bool live   = false;
        u8t  defIdx = 0;
    } slots[kMaxSlots]{};

    std::unordered_map<u8t, HitGroupState> hitGroups;
};

// ---------------------------------------------------------------------------

class ServerSkillSystem {
public:
    void registerAssets(std::vector<SkillAsset>&& assets);

    const SkillAsset* findAsset(u32t id) const;

    // Start a skill. initialElapsed allows lag-compensated remote starts.
    // Returns instance index or -1 on failure.
    int startSkill(u32t assetId, i32t ownerId,
                   Milliseconds initialElapsed = Milliseconds{ 0.f });

    // Per-frame update. Caller provides current owner states and all valid targets.
    void update(Milliseconds dt,
                const std::vector<ServerSkillOwner>& owners,
                const std::vector<ServerSkillTarget>& targets,
                std::vector<SkillHitResult>& outHits);

private:
    std::vector<SkillAsset> assetRegistry_;

    static constexpr int kMaxInstances = 64;
    ServerSkillInstance   instances_[kMaxInstances]{};
    int                   instanceCount_ = 0;

    int  allocInstance(const SkillAsset* asset, i32t ownerId);
    void terminateInstance(ServerSkillInstance& inst);

    void tickInstance(ServerSkillInstance& inst, Milliseconds dt,
                      const std::vector<ServerSkillOwner>& owners,
                      const std::vector<ServerSkillTarget>& targets,
                      std::vector<SkillHitResult>& outHits);

    void checkCollisions(ServerSkillInstance& inst,
                         const std::vector<ServerSkillOwner>& owners,
                         const std::vector<ServerSkillTarget>& targets,
                         std::vector<SkillHitResult>& outHits);

    // Approximate the world-space AABB for a hitbox given owner's pos/orient.
    // Server has no animation state so bone positions are approximated by a
    // height table derived from the bone name.
    AABB approximateHitboxAABB(const SkillHitboxDef& def,
                                const mu::Vec3& ownerPos,
                                const mu::NQuat& ownerOrient) const;

    static float approxBoneHeight(std::string_view boneName);
};

#endif  // __rs_skill_skillSystem_HPP
