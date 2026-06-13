#ifndef __rs_skill_skillTypes_HPP
#define __rs_skill_skillTypes_HPP

// Server-side copy of client/skill/skillTypes.hpp.
// Keep in sync with client/skill/skillTypes.hpp.
// Only difference: include path points to RoomServer's collision.hpp.

#include "../collision.hpp"
#include "particleGameplay.hpp"

#include <memory>

// Client-style type aliases (ServerEngine uses int32/uint8 etc.)
using i32t = int32;
using u8t  = uint8;
using u32t = uint32;
using u16t = uint16;

enum class AttachType : u8t {
    Bone,
    VFXParticle,
    Ground,       // plant at a caster-relative ground point, snapped to terrain, then static
};

struct AttachTarget {
    AttachType  type             = AttachType::Bone;
    std::string targetName;
    u8t         vfxId            = 0;
    int         particleSystemIdx = 0;
    bool        groundAlign      = false;  // Ground attach (per-OBB self snap only): tilt OBBs to the terrain normal
    i32t        groundAnchorRef  = -1;     // Ground attach: >=0 places OBBs in a registered SetGroundAnchor frame (rigid, point impact); -1 = per-OBB self snap (distributed eruption)
};

struct OnHitDef {
    i32t     damage          = 0;
    u8t      hitVfxId        = 0xFF;
    float    impulseStrength = 0.f;
    mu::Vec3 impulseDirLocal = { 0.f, 0.f, 1.f };
};

struct SkillHitboxDef {
    std::vector<OBB> localOBBs;
    AttachTarget     attach;
    OnHitDef         onHit;
    u8t              slot                = 0;
    u8t              hitGroup            = 0;
    float            hitGroupCooldownMs  = 0.f;
    bool             useParticleSize     = false;
    bool             applyAttachRotation = true;
};

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
    SIZE
};

static constexpr u8t kGroundAnchorFlagAlign = 0x01;  // tilt the anchor frame to the terrain normal

// PlayVFX::flags bit definitions (mirrors client; PlayVFX is a no-op on the server).
// bits3-4 = particle ground-collision mode, bits5-6 = particle ground-conform mode
// (client-only visual; unused here but kept for layout parity).
static constexpr u8t kPlayVFXFlagYawOnly     = 0x01;
static constexpr u8t kPlayVFXFlagGroundSnap  = 0x02;
static constexpr u8t kPlayVFXFlagGroundAlign = 0x04;
static constexpr u8t kPlayVFXParticleCollisionShift = 3;
static constexpr u8t kPlayVFXParticleCollisionMask  = 0x18;
static constexpr u8t kPlayVFXParticleConformShift   = 5;
static constexpr u8t kPlayVFXParticleConformMask    = 0x60;

union SkillEventPayload {
    struct SpawnHitbox {
        u8t defIdx;
    } spawnHitbox;

    struct DestroyHitbox {
        u8t slot;
    } destroyHitbox;

    struct PlayAnimation {
        char  clipName[24];
        float blendTime;
    } playAnimation;

    struct PlayVFX {
        mu::Vec3 localOffset;
        mu::Vec3 localEulerDeg;        // attach-local orientation offset (yaw, pitch, roll degrees)
        mu::Vec3 advanceForwardLocal;  // attach-local particle travel direction; zero = derive from orientation
        u8t      vfxId;
        u8t      attachType;
        u8t      attachVfxId;
        u8t      flags;                // bit0 = yawOnly (ground-plane placement)
        char     attachTargetName[16];
    } playVFX;

    struct ModifyStat {
        i32t         hpDelta;
        float        speedMultiplier;
        Milliseconds duration;
    } modifyStat;

    struct ApplyImpulse {
        float    strength;
        mu::Vec3 dirLocal;
    } applyImpulse;

    struct CameraShake {
        float        magnitude;
        Milliseconds duration;
    } cameraShake;

    struct SendGameplayEvent {
        u8t  eventTypeOrdinal;
        i32t param;
    } sendGameplayEvent;

    struct SpawnProjectile {
        u8t  projectileAssetId;
        float speedMps;
    } spawnProjectile;

    struct SetGroundAnchor {
        mu::Vec3 localOffset;  // caster-relative (right, up, forward); snapped to terrain
        u8t      anchorId;     // slot 0..kMaxGroundAnchors-1
        u8t      flags;        // bit0 = align frame to terrain normal
    } setGroundAnchor;

    u8t raw[56];
};

struct TimelineEvent {
    Milliseconds      time;
    SkillEventType    type;
    u8t               pad[3];
    SkillEventPayload payload;
};

struct SkillAsset {
    std::string  name;
    u32t         id            = 0;
    bool         interruptible = true;
    Milliseconds totalDuration { 0.f };

    std::vector<TimelineEvent>  timeline;
    std::vector<SkillHitboxDef> hitboxDefs;
    std::vector<std::string>    vfxNames;

    // Optional per-VFX composition from addVFX(id, path, { systems = ... }).
    // Mirrors the client compiler; index == ParticleEffect system index.
    // Used to rebuild gameplay-relevant particle systems for deterministic
    // VFXParticle hitboxes (see skillSystem.cpp updateParticleHitboxSources).
    struct VfxSystemDef {
        std::string name;          // relativePath inside the effect JSON ("" = code-built)
        u8t         playMode = 0;  // 0 = Emit, 1 = Continuous
        // Sub-emitter chain (lua: parent = N, parentEvent = "Birth"|"Death").
        // chainParent >= 0: spawns derive from that system's particle events
        // (mirrors bindSubEmitter); requires constant gameplay params.
        int         chainParent  = -1;
        bool        chainOnBirth = false;
        pg::VfxSystemOverrides overrides;  // mirrors client-side code cfg tweaks
        // Built once at boot (buildVfxGameplayConfigs in AssetManager): JSON
        // import (if name set) + overrides. Shared by all rooms; nullptr =
        // no gameplay config for this system.
        std::shared_ptr<const pg::GameplayConfig> gameplayCfg;
    };
    struct VfxDef {
        std::string path;                       // e.g. "effects/Foo_ParticleSystems.json"
        std::vector<VfxSystemDef> systems;
    };
    std::vector<VfxDef> vfxDefs;
};

#endif  // __rs_skill_skillTypes_HPP
