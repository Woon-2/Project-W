#ifndef __skill_skillTypes_HPP
#define __skill_skillTypes_HPP

// Skill system data types: immutable asset structures and timeline event definitions.
// No Lua or sol2 dependencies here -- pure C++ data.
//
// Layout overview:
//   SkillAsset        -- immutable compiled skill definition (loaded from Lua once at startup)
//     timeline[]      -- TimelineEvent sorted by time ascending
//     hitboxDefs[]    -- SkillHitboxDef, referenced by SpawnHitbox::defIdx
//     vfxNames[]      -- VFX asset paths, referenced by vfxId
//
//   TimelineEvent     -- fixed-size (union payload, no heap allocations)
//   SkillHitboxDef    -- hitbox shape + string-based attach target + on-hit response
//   OnHitDef          -- what to do when hitbox collides

#include "../collision.hpp"

// ---------------------------------------------------------------------------
// Attach type (used by both hitbox definitions and PlayVFX events)
// ---------------------------------------------------------------------------

enum class AttachType : u8t {
    Bone,         // follow a named skeleton bone of the caster
    VFXParticle,  // attach to all active particles of a ParticleSystem within a VFX effect
};

// String-based attach target. Resolved to a runtime handle once at SpawnHitbox dispatch.
struct AttachTarget {
    AttachType  type             = AttachType::Bone;
    std::string targetName;       // bone name (e.g. "Weapon_R"); unused for VFXParticle
    u8t         vfxId            = 0;  // index into SkillDispatchContext::vfxById
    int         particleSystemIdx = 0;  // index into ParticleEffect::system(n)
};

// ---------------------------------------------------------------------------
// On-hit response definition
// ---------------------------------------------------------------------------

struct OnHitDef {
    i32t     damage          = 0;
    u8t      hitVfxId        = 0xFF;  // 0xFF = no VFX
    float    impulseStrength = 0.f;
    mu::Vec3 impulseDirLocal = { 0.f, 0.f, 1.f };  // attacker-local space
};

// ---------------------------------------------------------------------------
// Hitbox definition (lives in SkillAsset, referenced by index from TimelineEvent)
// ---------------------------------------------------------------------------

struct SkillHitboxDef {
    std::vector<OBB> localOBBs;    // OBBs in attachment-local space (bone or particle space)
    AttachTarget     attach;
    OnHitDef         onHit;
    u8t              slot                 = 0;      // DestroyHitbox references this slot
    u8t              hitGroup             = 0;      // hitboxes sharing same group deduplicate hits per instance
    float            hitGroupCooldownMs   = 0.f;    // 0 = hit target only once; >0 = re-hit after N ms
    bool             useParticleSize      = false;  // VFXParticle only: scale halfExtents by particle's current visual size
    bool             applyAttachRotation  = true;   // false = ignore attachment orientation (position still tracks)
};

// ---------------------------------------------------------------------------
// Timeline event types
// ---------------------------------------------------------------------------

enum class SkillEventType : u8t {
    SpawnHitbox,
    DestroyHitbox,
    PlayAnimation,
    PlayVFX,
    SendGameplayEvent,
    ModifyStat,
    SpawnProjectile,
    ApplyImpulse,
    CameraShake,
    SIZE
};

// Fixed-size payload union. All members are trivially copyable (no std::string).
// String data lives in SkillAsset::hitboxDefs / vfxNames, referenced by index.
union SkillEventPayload {
    struct SpawnHitbox {
        u8t defIdx;  // index into SkillAsset::hitboxDefs
    } spawnHitbox;

    struct DestroyHitbox {
        u8t slot;
    } destroyHitbox;

    struct PlayAnimation {
        char  clipName[24];  // null-terminated, max 23 chars
        float blendTime;
    } playAnimation;

    struct PlayVFX {
        mu::Vec3 localOffset;          // attach-local space offset (right, up, forward); zero = at attach origin
        u8t      vfxId;                //  1 byte  [12]
        u8t      attachType;           //  1 byte  [13]: AttachType ordinal; Bone + empty name = caster root
        u8t      attachVfxId;          //  1 byte  [14]
        u8t      pad;                  //  1 byte  [15]
        char     attachTargetName[16]; // 16 bytes [16-31]: bone name, null-terminated; empty = use owner root
        // total: 32 bytes
    } playVFX;

    struct ModifyStat {
        i32t         hpDelta;
        float        speedMultiplier;
        Milliseconds duration;
    } modifyStat;

    struct ApplyImpulse {
        float    strength;
        mu::Vec3 dirLocal;  // attacker-local space; zero = forward
    } applyImpulse;

    struct CameraShake {
        float        magnitude;
        Milliseconds duration;
    } cameraShake;

    struct SendGameplayEvent {
        u8t  eventTypeOrdinal;  // EventType ordinal
        i32t param;
    } sendGameplayEvent;

    struct SpawnProjectile {  // future extension
        u8t  projectileAssetId;
        float speedMps;
    } spawnProjectile;

    u8t raw[32];  // fixes union size at 32 bytes
};

// One entry in the timeline. Keep this small -- it lives in a sorted vector.
struct TimelineEvent {
    Milliseconds      time;    // time from skill start
    SkillEventType    type;
    u8t               pad[3];
    SkillEventPayload payload;
};

// ---------------------------------------------------------------------------
// Skill asset (immutable after load; shared across all instances)
// ---------------------------------------------------------------------------

struct SkillAsset {
    std::string  name;
    u32t         id            = 0;
    bool         interruptible = true;
    Milliseconds totalDuration { 0.f };

    // Sorted by time ascending.
    std::vector<TimelineEvent>  timeline;

    // Hitbox definitions referenced by SpawnHitbox::defIdx.
    // Stored here (not in union) to allow std::string in AttachTarget.
    std::vector<SkillHitboxDef> hitboxDefs;

    // VFX asset paths indexed by vfxId (used in PlayVFX and OnHitDef::hitVfxId).
    std::vector<std::string> vfxNames;
};

#endif  // __skill_skillTypes_HPP
