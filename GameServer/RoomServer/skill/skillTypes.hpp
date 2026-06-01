#ifndef __rs_skill_skillTypes_HPP
#define __rs_skill_skillTypes_HPP

// Server-side copy of client/skill/skillTypes.hpp.
// Keep in sync with client/skill/skillTypes.hpp.
// Only difference: include path points to RoomServer's collision.hpp.

#include "../collision.hpp"

// Client-style type aliases (ServerEngine uses int32/uint8 etc.)
using i32t = int32;
using u8t  = uint8;
using u32t = uint32;
using u16t = uint16;

enum class AttachType : u8t {
    Bone,
    VFXParticle,
};

struct AttachTarget {
    AttachType  type             = AttachType::Bone;
    std::string targetName;
    u8t         vfxId            = 0;
    int         particleSystemIdx = 0;
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
    SIZE
};

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
        u8t      vfxId;
        u8t      attachType;
        u8t      attachVfxId;
        u8t      pad;
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

    u8t raw[32];
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
};

#endif  // __rs_skill_skillTypes_HPP
