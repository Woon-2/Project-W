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
#include "particleGameplay.hpp"

#include <memory>

// ---------------------------------------------------------------------------
// Attach type (used by both hitbox definitions and PlayVFX events)
// ---------------------------------------------------------------------------

enum class AttachType : u8t {
    Bone,         // follow a named skeleton bone of the caster
    VFXParticle,  // attach to all active particles of a ParticleSystem within a VFX effect
    Ground,       // plant at a caster-relative ground point, snapped to terrain, then static
};

// String-based attach target. Resolved to a runtime handle once at SpawnHitbox dispatch.
struct AttachTarget {
    AttachType  type             = AttachType::Bone;
    std::string targetName;       // bone name (e.g. "Weapon_R"); unused for VFXParticle
    u8t         vfxId            = 0;  // index into SkillDispatchContext::vfxById
    int         particleSystemIdx = 0;  // index into ParticleEffect::system(n)
    bool        groundAlign      = false;  // Ground attach (per-OBB self snap only): tilt OBBs to the terrain normal
    i32t        groundAnchorRef  = -1;     // Ground attach: >=0 places OBBs in a registered SetGroundAnchor frame (rigid, point impact); -1 = per-OBB self snap (distributed eruption)
};

// ---------------------------------------------------------------------------
// On-hit response definition
// ---------------------------------------------------------------------------

struct OnHitDef {
    i32t     damage          = 0;
    u8t      hitVfxId        = 0xFF;  // 0xFF = no VFX
    float    hitVfxScale     = 1.f;   // 피격 VFX 크기 배율(피니셔 등 hit별 연출)
    float    impulseStrength = 0.f;
    mu::Vec3 impulseDirLocal = { 0.f, 0.f, 1.f };  // attacker-local space
};

// ---------------------------------------------------------------------------
// Hitbox definition (lives in SkillAsset, referenced by index from TimelineEvent)
// ---------------------------------------------------------------------------

struct SkillHitboxDef {
    std::vector<OBB> localOBBs;    // OBBs in attachment-local space (bone or particle space)
    // Authoring Euler angles (degrees, yaw/pitch/roll) for each entry in localOBBs.
    // Populated by the Lua compiler so the in-game skill editor can present and round-trip
    // rotation exactly as written in the .lua source. Parallel to localOBBs (same size).
    // Not used by the runtime collision path.
    std::vector<mu::Vec3> localOBBEulerDeg;
    AttachTarget     attach;
    OnHitDef         onHit;
    u8t              slot                 = 0;      // DestroyHitbox references this slot
    u8t              hitGroup             = 0;      // hitboxes sharing same group deduplicate hits per instance
    float            hitGroupCooldownMs   = 0.f;    // 0 = hit target only once; >0 = re-hit after N ms
    bool             useParticleSize      = false;  // VFXParticle only: scale halfExtents by particle's current visual size
    bool             applyAttachRotation  = true;   // false = ignore attachment orientation (position still tracks)
    // VFXParticle only: false = non-penetrating -- the source particle is destroyed on first hit
    // (ParticleSystem::killParticle), so the projectile/effect stops after impact. true (default) =
    // penetrating (particle persists; multi-hit governed by hitGroupCooldownMs). No-op for Bone/Ground.
    bool             penetrate            = true;
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
    SetGroundAnchor,
    PlaySound,
    SIZE
};

// SetGroundAnchor::flags bit definitions.
static constexpr u8t kGroundAnchorFlagAlign = 0x01;  // tilt the anchor frame to the terrain normal

// PlayVFX::flags bit definitions.
//   bit0      : yawOnly      -- place effect on ground plane (ignore caster pitch/roll)
//   bit1      : groundSnap   -- snap final worldPos.y to terrain (localOffset.y = lift)
//   bit2      : groundAlign  -- orient effect to the terrain normal (keeps yaw)
//   bits3-4   : particle ground-collision mode (ps::ParticleCollisionModule::Mode ordinal)
//   bits5-6   : particle ground-conform   mode (ps::ShapeModule::GroundConform ordinal)
// The particle mode fields drive the *played effect's particles* (fall-and-die,
// conform-to-slope) from the skill Lua, so the effect JSON needs no ground keys.
static constexpr u8t kPlayVFXFlagYawOnly     = 0x01;
static constexpr u8t kPlayVFXFlagGroundSnap  = 0x02;
static constexpr u8t kPlayVFXFlagGroundAlign = 0x04;

static constexpr u8t kPlayVFXParticleCollisionShift = 3;
static constexpr u8t kPlayVFXParticleCollisionMask  = 0x18;  // bits 3-4
static constexpr u8t kPlayVFXParticleConformShift   = 5;
static constexpr u8t kPlayVFXParticleConformMask    = 0x60;  // bits 5-6

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
        char  clipName[32];  // null-terminated, max 31 chars (e.g. "Combat_Defend_AttackSpecial" = 27)
        float blendTime;
        u8t   attackIndex;   // selects which attack clip the AnimBlender plays (index into attackClips_)
    } playAnimation;

    struct PlayVFX {
        mu::Vec3 localOffset;          // 12 bytes [0-11]:  attach-local offset (right, up, forward); zero = at attach origin
        mu::Vec3 localEulerDeg;        // 12 bytes [12-23]: attach-local orientation offset (yaw, pitch, roll degrees); zero = no offset
        mu::Vec3 advanceForwardLocal;  // 12 bytes [24-35]: attach-local particle travel direction; zero = derive from orientation
        u8t      vfxId;                //  1 byte  [36]
        u8t      attachType;           //  1 byte  [37]: AttachType ordinal; Bone + empty name = caster root
        u8t      attachVfxId;          //  1 byte  [38]
        u8t      flags;                //  1 byte  [39]: bit0 = yawOnly (place on ground plane, ignore caster pitch/roll)
        char     attachTargetName[16]; // 16 bytes [40-55]: bone name, null-terminated; empty = use owner root
        // total: 56 bytes
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

    struct PlaySound {
        char soundName[24];  // sound-catalog logical name, null-terminated (max 23 chars)
        u16t maxDurationMs;  // 0 = play to the file's natural end; else fade-stop this many ms after start
        u16t fadeMs;         // fade-out length applied when maxDurationMs elapses (0 = hard cut)
        float volume;        // 0..1 playback gain (clamped), scales the catalog default; 1 = default
    } playSound;

    struct SendGameplayEvent {
        u8t  eventTypeOrdinal;  // EventType ordinal
        i32t param;
    } sendGameplayEvent;

    struct SpawnProjectile {  // future extension
        u8t  projectileAssetId;
        float speedMps;
    } spawnProjectile;

    struct SetGroundAnchor {
        mu::Vec3 localOffset;  // 12 bytes: caster-relative (right, up, forward); snapped to terrain
        u8t      anchorId;     //  1 byte:  slot 0..kMaxGroundAnchors-1
        u8t      flags;        //  1 byte:  bit0 = align frame to terrain normal
    } setGroundAnchor;

    u8t raw[56];  // fixes union size at 56 bytes (PlayVFX is the largest member)
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

    // --- Stack-charge / loadout metadata (set from skill Lua; see skillChargeSystem.md) ---
    // weaponType ordinal mirrors PlayerWeaponType (protocol.hpp):
    //   Katana(sword)=0, SpearHook(spear)=1, CrystalWand(wand)=2, HeavyArrow(bow)=3; 0xFF = no loadout.
    u8t          weaponType  = 0xFFu;
    int          loadoutSlot = -1;     // 0..2 dial slot; -1 = not a selectable loadout skill
    bool         isBasic     = false;  // left-click basic attack: skips charge/cooldown gate
    float        chargeCost  = 0.f;    // charge points required per cast (selectable skills only)
    Milliseconds cooldown { 0.f };     // cast cooldown (>= totalDuration + margin)

    // Sorted by time ascending.
    std::vector<TimelineEvent>  timeline;

    // Hitbox definitions referenced by SpawnHitbox::defIdx.
    // Stored here (not in union) to allow std::string in AttachTarget.
    std::vector<SkillHitboxDef> hitboxDefs;

    // VFX asset paths indexed by vfxId (used in PlayVFX and OnHitDef::hitVfxId).
    std::vector<std::string> vfxNames;

    // Optional per-VFX composition from addVFX(id, path, { systems = ... }).
    // Lets the server (and the client deterministic mode) rebuild the
    // gameplay-relevant particle systems of an effect for deterministic
    // VFXParticle hitboxes. Indexed by vfxId; entries without composition
    // info have an empty path.
    struct VfxSystemDef {
        std::string name;          // relativePath inside the effect JSON ("" = code-built)
        u8t         playMode = 0;  // 0 = Emit, 1 = Continuous
        // Sub-emitter chain (lua: parent = N, parentEvent = "Birth"|"Death").
        // chainParent >= 0: spawns derive from that system's particle events
        // (mirrors bindSubEmitter); requires constant gameplay params.
        int         chainParent  = -1;
        bool        chainOnBirth = false;
        pg::VfxSystemOverrides overrides;  // mirrors client-side code cfg tweaks
        // Built once after compile (buildVfxGameplayConfigs): JSON import (if
        // name set) + overrides. nullptr = no gameplay config for this system.
        std::shared_ptr<const pg::GameplayConfig> gameplayCfg;
    };
    struct VfxDef {
        std::string path;                       // e.g. "effects/Foo_ParticleSystems.json"
        std::vector<VfxSystemDef> systems;      // index == ParticleEffect system index
    };
    std::vector<VfxDef> vfxDefs;
};

#endif  // __skill_skillTypes_HPP
