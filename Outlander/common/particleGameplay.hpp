#ifndef __particleGameplay_HPP
#define __particleGameplay_HPP

// ---------------------------------------------------------------------------
// Deterministic gameplay-particle core shared by the client and RoomServer.
//
// Purpose: VFXParticle-attached hitboxes must land at identical positions on
// the client (visuals) and the server (authoritative hit detection). Stream
// RNGs (mt19937) cannot guarantee this because the draw order depends on the
// frame-dt partition. This module provides:
//
//   1. DetRng        - stateless counter-based RNG: hash(seed, stream, id,
//                      cursor). Values depend only on the keys, never on
//                      call order across frames.
//   2. GameplayConfig - the gameplay-relevant subset of a Unity particle
//                      system config (emission, shape, main ranges).
//   3. sampleSpawn() - deterministic spawn-parameter sampling. The client
//                      ParticleSystem (deterministic mode) and the server
//                      sampler call this same function, so spawn positions,
//                      directions, sizes, lifetimes and rotations agree by
//                      construction.
//   4. evaluateParticles() - analytic state evaluation at time t (server
//                      side). No per-frame integration, so it is immune to
//                      tick-rate differences. Supports burst + rate emission
//                      of root systems with constant-velocity motion.
//   5. importGameplayConfig() - reads the gameplay subset from the Unity
//                      effect JSON via json:: (simpleJson).
//
// SYNC WARNING: the sampling math mirrors client/particleSystem.cpp
// (sampleCone / sampleCircle / sampleShapeOrigin / sampleShapeDirection /
// spawnParticle) and the import mirrors client/particleImporter.cpp
// (readMainModule / readEmissionModule / readShapeModule / readTransform /
// readSizeModule / readRotationModule). Changes there must be reflected here.
//
// KNOWN LIMITS (logged via GameplayConfig::unsupported):
//   - velocityOverLifetime with curve/random channels (constant-only support)
//   - rotationOverLifetime curve mode (constant angular velocity only)
//   - drag, sub-emitter-driven systems, particle ground collision
// Position error vs the client Euler integration is bounded by one client
// frame for gravity-affected particles; exact for constant-velocity ones.
// ---------------------------------------------------------------------------

#include "mathUtil.hpp"
#include "simpleJson.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pg {

// ---------------------------------------------------------------------------
// Counter-based deterministic RNG
// ---------------------------------------------------------------------------

inline std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

// Mixes a 32-bit salt into a 32-bit seed (system index, vfx id, ...).
inline std::uint32_t mixSeed(std::uint32_t seed, std::uint32_t salt) {
    return static_cast<std::uint32_t>(
        splitmix64((static_cast<std::uint64_t>(seed) << 32) | salt) >> 32);
}

// Stream ids partition the key space so spawn sources never collide.
enum : std::uint32_t {
    kStreamPlayEmit   = 1,  // ParticleEffect::play() -> emit(n) (PlayMode::Emit)
    kStreamRate       = 2,  // continuous rate-over-time spawns
    kStreamBurstMeta  = 3,  // per-(loop,burst,cycle) probability/count draws
    kStreamBurstSpawn = 4,  // per-particle draws of burst spawns
    kStreamChainSpawn = 5,  // sub-emitter chained spawns (parent birth/death)
};

inline std::uint32_t burstKey(int loop, int burstIdx, int cycle) {
    std::uint32_t k = static_cast<std::uint32_t>(loop);
    k = mixSeed(k, static_cast<std::uint32_t>(burstIdx));
    k = mixSeed(k, static_cast<std::uint32_t>(cycle));
    return k;
}

inline std::uint32_t burstParticleId(std::uint32_t key, int indexInBurst) {
    return key * 2654435761u + static_cast<std::uint32_t>(indexInBurst);
}

// All draws for one particle come from one DetRng{seed, stream, id}; the
// cursor advances per draw. The draw order inside sampleSpawn() is therefore
// part of the wire format: both sides run the same code, so it cannot drift.
struct DetRng {
    std::uint32_t seed   = 0;
    std::uint32_t stream = 0;
    std::uint32_t id     = 0;
    std::uint32_t cursor = 0;

    float next01() {
        const std::uint64_t h = splitmix64(
            splitmix64((static_cast<std::uint64_t>(seed) << 32) | stream)
            ^ ((static_cast<std::uint64_t>(id) << 32) | cursor++));
        // Top 24 bits -> [0, 1). Never returns 1.0.
        return static_cast<float>(h >> 40) * (1.0f / 16777216.0f);
    }

    // Mirrors ParticleSystem::randomFloat (swaps when hi < lo).
    float range(float lo, float hi) {
        if (hi < lo) std::swap(lo, hi);
        return lo + (hi - lo) * next01();
    }
};

// ---------------------------------------------------------------------------
// Gameplay config (subset of ps::ParticleSystemConfig; render-free)
// ---------------------------------------------------------------------------

enum class ShapeType { Point, Edge, Cone, Sphere, Hemisphere, Box, Circle };
enum class GroundConform { None, SnapY, SnapAndAlign };
enum class PlayMode { Emit, Continuous };

struct Burst {
    float time           = 0.f;
    int   countMin       = 1;
    int   countMax       = 1;
    int   cycleCount     = 1;
    float repeatInterval = 0.01f;
    float probability    = 1.f;
};

struct GameplayConfig {
    // main
    float    lifetimeMin = 0.5f, lifetimeMax = 1.5f;
    float    speedMin = 1.f, speedMax = 3.f;
    float    startSizeMin = 1.f, startSizeMax = 1.f;
    bool     startSize3DEnabled = false;
    mu::Vec3 startSize3DMin = { 1.f, 1.f, 1.f };
    mu::Vec3 startSize3DMax = { 1.f, 1.f, 1.f };
    float    startRotationMin = 0.f, startRotationMax = 0.f;  // radians (Z)
    bool     startRotation3DEnabled = false;
    mu::Vec3 startRotation3DMin = { 0.f, 0.f, 0.f };          // radians
    mu::Vec3 startRotation3DMax = { 0.f, 0.f, 0.f };
    mu::Vec3 gravity = { 0.f, -9.8f, 0.f };
    float    gravityModifierMin = 0.f, gravityModifierMax = 0.f;
    float    duration        = 5.f;
    bool     looping         = true;
    float    startDelay      = 0.f;
    float    simulationSpeed = 1.f;
    int      maxParticles    = 0;   // 0 = unbounded (guarded by caller)

    // emission
    bool  emissionEnabled = true;
    float emitRate        = 0.f;
    std::vector<Burst> bursts;

    // shape (static part; the dynamic emitter frame is passed at sample time)
    bool      shapeEnabled = true;
    ShapeType type         = ShapeType::Point;
    mu::Vec3  rotationEuler = { 0.f, 0.f, 0.f };  // radians
    mu::Vec3  scale         = { 1.f, 1.f, 1.f };
    mu::Vec3  direction     = { 0.f, 1.f, 0.f };
    float     edgeLength    = 1.f;
    mu::Vec3  edgeDir       = { 1.f, 0.f, 0.f };
    float     coneAngle     = 0.3f;   // radians (half-angle)
    float     coneRadius    = 0.f;
    float     sphereRadius  = 1.f;
    mu::Vec3  boxSize       = { 1.f, 1.f, 1.f };
    float     radiusThickness = 1.f;
    float     arc           = 2.f * 3.14159265f;  // radians
    int       arcMode       = 0;                  // 3 = burst spread (even)
    float     randomPositionAmount = 0.f;
    GroundConform groundConform = GroundConform::None;
    float         groundOffset  = 0.f;

    // size over lifetime (scalar begin/end path used by hitboxes)
    bool  solEnabled   = false;
    float solSizeBegin = 1.f;
    float solSizeEnd   = 0.f;

    // rotation over lifetime (constant angular velocity path only)
    bool     rolEnabled      = false;
    bool     rolSeparateAxes = false;
    bool     rolUseCurves    = false;   // unsupported when true (see 'unsupported')
    float    angularVelocityMin = 0.f, angularVelocityMax = 0.f;
    mu::Vec3 angularVelocityMin3D = { 0.f, 0.f, 0.f };
    mu::Vec3 angularVelocityMax3D = { 0.f, 0.f, 0.f };

    // velocity over lifetime: constant-linear subset only
    bool     volEnabled       = false;
    bool     volSupported     = true;   // false -> motion ignores the module
    bool     volInWorldSpace  = false;  // false -> linear rotated by emitter orientation
    mu::Vec3 volLinear        = { 0.f, 0.f, 0.f };
    float    volSpeedModifier = 1.f;

    // base placement of this system inside the effect prefab
    // (mirrors ParticleEffect::Entry shapeBase* captured at addSystem()).
    mu::Vec3   baseShapePosition = { 0.f, 0.f, 0.f };
    mu::Mat4x4 baseOrientation   = mu::Mat4x4{};
    mu::Mat4x4 meshBaseRotation  = mu::Mat4x4{};

    // Human-readable list of gameplay-relevant features this config cannot
    // reproduce. Derived from the flags so it stays correct after
    // applyOverrides() (e.g. a volLinear override replaces unsupported JSON
    // velocity curves). Log once at load when non-empty.
    std::string unsupportedSummary() const {
        std::string s;
        auto add = [&](const char* w) {
            if (!s.empty()) s += ", ";
            s += w;
        };
        if (volEnabled && !volSupported) add("velocityOverLifetime (non-constant/orbital)");
        if (rolEnabled && rolUseCurves)  add("rotationOverLifetime curves");
        return s;
    }
};

// ---------------------------------------------------------------------------
// Per-system gameplay overrides, authored in the skill Lua:
//   addVFX(id, path, { systems = { { name=..., mode=..., <overrides> } } })
// They mirror the code-side cfg tweaks the client applies when composing a
// ParticleEffect (game.cpp/onlineGame.cpp), so the server (and the client
// deterministic mode) reproduce the SAME gameplay parameters. Systems built
// entirely in code use an empty `name` (defaults + overrides, no JSON).
// SYNC: keep each skill's overrides equal to its client effect composition.
// ---------------------------------------------------------------------------

struct VfxSystemOverrides {
    bool hasLooping         = false;  bool      looping = false;
    bool hasDuration        = false;  float     duration = 0.f;
    bool hasSpeed           = false;  float     speedMin = 0.f, speedMax = 0.f;
    bool hasLifetime        = false;  float     lifetimeMin = 0.f, lifetimeMax = 0.f;
    bool hasStartSize       = false;  float     startSizeMin = 1.f, startSizeMax = 1.f;
    bool hasMaxParticles    = false;  int       maxParticles = 0;
    bool hasShapeType       = false;  ShapeType shapeType = ShapeType::Point;
    bool hasShapePosition   = false;  mu::Vec3  shapePosition = { 0.f, 0.f, 0.f };
    bool hasShapeEulerDeg   = false;  mu::Vec3  shapeEulerDeg = { 0.f, 0.f, 0.f };
    bool hasMeshEulerDeg    = false;  mu::Vec3  meshEulerDeg  = { 0.f, 0.f, 0.f };
    bool hasDirection       = false;  mu::Vec3  direction = { 0.f, 0.f, 1.f };
    bool hasBoxSize         = false;  mu::Vec3  boxSize = { 1.f, 1.f, 1.f };
    bool hasConeRadius      = false;  float     coneRadius = 0.f;
    bool hasConeAngleDeg    = false;  float     coneAngleDeg = 0.f;
    bool hasRadiusThickness = false;  float     radiusThickness = 1.f;
    bool hasEmitRate        = false;  float     emitRate = 0.f;
    bool hasBursts          = false;  std::vector<Burst> bursts;
    bool hasVolLinear       = false;  mu::Vec3  volLinear = { 0.f, 0.f, 0.f };

    bool any() const {
        return hasLooping || hasDuration || hasSpeed || hasLifetime
            || hasStartSize || hasMaxParticles || hasShapeType
            || hasShapePosition || hasShapeEulerDeg || hasMeshEulerDeg
            || hasDirection || hasBoxSize || hasConeRadius || hasConeAngleDeg
            || hasRadiusThickness || hasEmitRate || hasBursts || hasVolLinear;
    }
};

// ---------------------------------------------------------------------------
// Spawn parameter sampling (shared draw order - DO NOT reorder draws)
// ---------------------------------------------------------------------------

struct SpawnParams {
    mu::Vec3 origin = { 0.f, 0.f, 0.f };   // world space (before ground snap)
    mu::Vec3 dir    = { 0.f, 0.f, 1.f };   // unit travel direction
    float    speed             = 0.f;
    float    lifetime          = 0.f;       // scaled-time seconds
    float    gravityModifier   = 0.f;
    float    rotationZ         = 0.f;
    float    sizeScalar        = 1.f;
    mu::Vec3 size3D            = { 1.f, 1.f, 1.f };
    bool     rotation3DEnabled = false;
    mu::Vec3 rotation3D        = { 0.f, 0.f, 0.f };  // radians
    float    angularVelocityZ  = 0.f;
    mu::Vec3 angularVelocity3D = { 0.f, 0.f, 0.f };
};

namespace detail {

inline mu::Mat4x4 buildEulerRotation(mu::Vec3 eulerRadians) {
    return mu::rotateXH(mu::Radian{ eulerRadians.x() })
         * mu::rotateYH(mu::Radian{ eulerRadians.y() })
         * mu::rotateZH(mu::Radian{ eulerRadians.z() });
}

inline mu::Vec3 rotateVector(mu::Vec3 v, const mu::Mat4x4& rotation) {
    return mu::Vec3(mu::Vec4(v.x(), v.y(), v.z(), 0.f) * rotation);
}

inline mu::Vec3 rotateDirection(mu::Vec3 v, const mu::Mat4x4& rotation) {
    const float lenSq = mu::dot(v, v);
    if (lenSq < 1e-6f)
        return v;
    return mu::Vec3(mu::normalize(rotateVector(v, rotation)));
}

// Mirrors the static sampleConeDirection in client/particleSystem.cpp
// (only the randomPositionAmount offset path; no tangent hint, arc = pi).
inline mu::Vec3 sampleHemisphereDirection(DetRng& rng) {
    const float spread    = 3.14159265f;
    const float cosSpread = std::cos(spread);
    const float cosTheta  = cosSpread + (1.f - cosSpread) * rng.next01();
    const float sinTheta  = std::sqrt(1.f - cosTheta * cosTheta);
    const float phi       = 2.f * 3.14159265f * rng.next01();

    const mu::Vec3 axis{ 0.f, 1.f, 0.f };
    // axis = +Y, |axis.x| < 0.9 -> up = +X; tangent = cross(axis, up) = (0,0,-1)
    const mu::Vec3 tangent   = mu::Vec3(mu::cross(mu::NVec3(axis), mu::NVec3(mu::Vec3{ 1.f, 0.f, 0.f })));
    const mu::Vec3 bitangent = mu::Vec3(mu::cross(mu::NVec3(tangent), mu::NVec3(axis)));

    return sinTheta * std::cos(phi) * tangent
         + sinTheta * std::sin(phi) * bitangent
         + cosTheta * axis;
}

struct ShapeFrame {
    mu::Vec3   position;      // world emitter origin
    mu::Mat4x4 rotation;      // buildEulerRotation(cfg euler) * world orientation
};

inline mu::Vec3 withRandomOffset(const GameplayConfig& g, mu::Vec3 p, DetRng& rng) {
    if (g.randomPositionAmount <= 0.f)
        return p;
    const mu::Vec3 randomDir = sampleHemisphereDirection(rng);
    return p + randomDir * rng.range(0.f, g.randomPositionAmount);
}

inline float sampleArcAngle(const GameplayConfig& g, DetRng& rng,
                            int emitIndex, int emitCount) {
    if (g.arcMode != 3 || emitCount <= 1)
        return rng.range(0.f, g.arc);

    const bool fullCircle = std::abs(g.arc - 2.f * 3.14159265f) < 1e-4f;
    const float denom = fullCircle
                      ? static_cast<float>(emitCount)
                      : static_cast<float>(emitCount - 1);
    return g.arc * static_cast<float>(emitIndex) / std::max(1.f, denom);
}

}  // namespace detail

// Samples everything gameplay-relevant for one particle. The emitter frame
// is the play-time world transform: on the client it is config_.shape
// .position/.orientation (set by ParticleEffect::play), on the server it is
// derived from the PlayVFX anchor + GameplayConfig::baseShapePosition.
inline SpawnParams sampleSpawn(const GameplayConfig& g,
                               const mu::Vec3&   shapeWorldPos,
                               const mu::Mat4x4& shapeWorldOrient,
                               DetRng& rng,
                               int emitIndex, int emitCount) {
    using namespace detail;

    SpawnParams sp;
    const ShapeFrame f{ shapeWorldPos,
                        buildEulerRotation(g.rotationEuler) * shapeWorldOrient };

    // -- shape origin + direction (mirrors spawnParticle shape dispatch) -----
    if (g.shapeEnabled && g.type == ShapeType::Circle) {
        const float angle = sampleArcAngle(g, rng, emitIndex, emitCount);
        const float inner = std::clamp(1.f - g.radiusThickness, 0.f, 1.f);
        const float r     = g.coneRadius * std::sqrt(rng.range(inner * inner, 1.f));
        const mu::Vec3 localDir = { std::cos(angle), std::sin(angle), 0.f };
        sp.origin = withRandomOffset(g, f.position + rotateVector(localDir * r, f.rotation), rng);
        sp.dir    = rotateVector(localDir, f.rotation);
    } else if (g.shapeEnabled && g.type == ShapeType::Cone) {
        const float angle = sampleArcAngle(g, rng, emitIndex, emitCount);
        const float inner = std::clamp(1.f - g.radiusThickness, 0.f, 1.f);
        const float r     = g.coneRadius * std::sqrt(rng.range(inner * inner, 1.f));
        const float sideSin = std::sin(g.coneAngle);
        const float sideCos = std::cos(g.coneAngle);
        const mu::Vec3 localRadial = { std::cos(angle), std::sin(angle), 0.f };
        const mu::Vec3 localDir = {
            localRadial.x() * sideSin,
            localRadial.y() * sideSin,
            sideCos
        };
        sp.origin = withRandomOffset(g, f.position + rotateVector(localRadial * r, f.rotation), rng);
        sp.dir    = rotateDirection(localDir, f.rotation);
    } else if (!g.shapeEnabled) {
        sp.origin = f.position;
        sp.dir    = g.direction;
    } else {
        // generic origin
        switch (g.type) {
        case ShapeType::Point:
            sp.origin = withRandomOffset(g, f.position, rng);
            break;
        case ShapeType::Edge: {
            const float lenSq = mu::dot(g.edgeDir, g.edgeDir);
            if (lenSq < 1e-6f) { sp.origin = f.position; break; }
            const mu::Vec3 dir = rotateDirection(g.edgeDir, f.rotation);
            sp.origin = withRandomOffset(
                g, f.position + dir * rng.range(-g.edgeLength * 0.5f, g.edgeLength * 0.5f), rng);
            break;
        }
        case ShapeType::Sphere: {
            const float theta = 2.f * 3.14159265f * rng.next01();
            const float phi   = std::acos(1.f - 2.f * rng.next01());
            const float spn   = std::sin(phi);
            const float inner = std::clamp(1.f - g.radiusThickness, 0.f, 1.f);
            const float r     = g.sphereRadius * std::cbrt(rng.range(inner * inner * inner, 1.f));
            sp.origin = withRandomOffset(g, f.position + mu::Vec3{
                r * spn * std::cos(theta),
                r * std::cos(phi),
                r * spn * std::sin(theta) }, rng);
            break;
        }
        case ShapeType::Hemisphere: {
            const float theta = 2.f * 3.14159265f * rng.next01();
            const float y     = rng.next01();
            const float spn   = std::sqrt(std::max(0.f, 1.f - y * y));
            const float inner = std::clamp(1.f - g.radiusThickness, 0.f, 1.f);
            const float r     = g.sphereRadius * std::cbrt(rng.range(inner * inner * inner, 1.f));
            sp.origin = withRandomOffset(g, f.position + rotateVector(mu::Vec3{
                r * spn * std::cos(theta),
                r * y,
                r * spn * std::sin(theta) }, f.rotation), rng);
            break;
        }
        case ShapeType::Box:
            sp.origin = withRandomOffset(g, f.position + rotateVector(mu::Vec3{
                rng.range(-g.boxSize.x() * 0.5f, g.boxSize.x() * 0.5f),
                rng.range(-g.boxSize.y() * 0.5f, g.boxSize.y() * 0.5f),
                rng.range(-g.boxSize.z() * 0.5f, g.boxSize.z() * 0.5f) }, f.rotation), rng);
            break;
        default:
            sp.origin = f.position;
            break;
        }
        // generic direction
        switch (g.type) {
        case ShapeType::Point:
        case ShapeType::Edge:
            sp.dir = rotateDirection(g.direction, f.rotation);
            break;
        case ShapeType::Sphere:
        case ShapeType::Hemisphere:
        case ShapeType::Box: {
            const mu::Vec3 outward = sp.origin - f.position;
            const float    len     = std::sqrt(mu::dot(outward, outward));
            sp.dir = (len > 1e-6f) ? outward * (1.f / len) : g.direction;
            break;
        }
        default:
            sp.dir = g.direction;
            break;
        }
    }

    // -- scalar draws (canonical order; both sides use this exact sequence) --
    sp.speed           = rng.range(g.speedMin, g.speedMax);
    sp.lifetime        = rng.range(g.lifetimeMin, g.lifetimeMax);
    sp.gravityModifier = rng.range(g.gravityModifierMin, g.gravityModifierMax);
    sp.rotationZ       = rng.range(g.startRotationMin, g.startRotationMax);

    sp.sizeScalar = rng.range(g.startSizeMin, g.startSizeMax);
    sp.size3D = g.startSize3DEnabled
              ? mu::Vec3{ rng.range(g.startSize3DMin.x(), g.startSize3DMax.x()),
                          rng.range(g.startSize3DMin.y(), g.startSize3DMax.y()),
                          rng.range(g.startSize3DMin.z(), g.startSize3DMax.z()) }
              : mu::Vec3{ sp.sizeScalar, sp.sizeScalar, sp.sizeScalar };

    if (g.rolEnabled) {
        sp.angularVelocityZ = g.rolSeparateAxes
            ? rng.range(g.angularVelocityMin3D.z(), g.angularVelocityMax3D.z())
            : rng.range(g.angularVelocityMin, g.angularVelocityMax);
        if (g.rolSeparateAxes) {
            sp.angularVelocity3D = {
                rng.range(g.angularVelocityMin3D.x(), g.angularVelocityMax3D.x()),
                rng.range(g.angularVelocityMin3D.y(), g.angularVelocityMax3D.y()),
                sp.angularVelocityZ
            };
        } else {
            sp.angularVelocity3D = { 0.f, 0.f, sp.angularVelocityZ };
        }
    }

    sp.rotation3DEnabled = g.startRotation3DEnabled;
    if (g.startRotation3DEnabled) {
        sp.rotation3D = {
            rng.range(g.startRotation3DMin.x(), g.startRotation3DMax.x()),
            rng.range(g.startRotation3DMin.y(), g.startRotation3DMax.y()),
            rng.range(g.startRotation3DMin.z(), g.startRotation3DMax.z())
        };
    }

    return sp;
}

// ---------------------------------------------------------------------------
// Analytic evaluation at time t (server-side hitbox source)
// ---------------------------------------------------------------------------

struct EmitterFrame {
    mu::Vec3   playPos    = { 0.f, 0.f, 0.f };
    mu::Mat4x4 orientXform = mu::Mat4x4{};
};

struct GroundQuery {
    std::function<float(float, float)>    height;   // (x, z) -> y
    std::function<mu::Vec3(float, float)> normal;   // (x, z) -> unit normal
};

struct ParticleState {
    mu::Vec3   pos;
    float      sizeNow;          // scalar size with sizeOverLifetime applied
    float      ageT;             // 0..1 normalized age
    mu::Vec3   angularAngle3D;   // radians, accumulated rotation
    mu::Mat4x4 baseRotation;     // spawn orientation (startRotation3D * base)
    // Stable spawn identity (counter-PRNG key). Lets the server-authoritative
    // hitbox path track a specific particle across frames (e.g. mark a
    // non-penetrating projectile consumed on hit). Same key on both machines.
    std::uint32_t stream = 0;
    std::uint32_t id     = 0;
};

namespace detail {

// Mirrors alignYToNormalMat in client/particleSystem.cpp.
inline mu::Mat4x4 alignYToNormalMat(mu::Vec3 n) {
    const mu::Vec3 up{ 0.f, 1.f, 0.f };
    const mu::Vec3 axis = mu::cross(up, n);
    const float    s2   = mu::dot(axis, axis);
    if (s2 < 1e-8f)
        return mu::Mat4x4{};
    const float c     = std::clamp(mu::dot(up, n), -1.f, 1.f);
    const float angle = std::acos(c);
    return mu::rotateH(mu::Radian{ angle }, mu::Vec3(mu::normalize(axis)));
}

}  // namespace detail

// One system inside an effect, as seen by the evaluator. chainParent >= 0
// marks a sub-emitter child: its spawns derive from the parent's particle
// births/deaths instead of its own schedule (mirrors ParticleEffect's
// SubEmitterBinding + PendingSubEmitterBurst replay).
struct SystemRef {
    const GameplayConfig* cfg = nullptr;
    std::uint32_t seed        = 0;
    PlayMode      mode        = PlayMode::Continuous;
    int           chainParent  = -1;
    bool          chainOnBirth = false;   // false = Death event
};

// Maps a system index of the effect to its SystemRef (cfg == nullptr -> none).
using SystemResolver = std::function<SystemRef(int)>;

namespace detail {

// A spawn that happened at spawnReal (seconds since play, real time) with
// fully sampled parameters. Kept for chained children even after death.
struct RealizedSpawn {
    float         spawnReal = 0.f;
    std::uint32_t stream = 0, id = 0;
    SpawnParams   sp;
    mu::Vec3      origin;     // world, ground-snapped
    mu::Vec3      spawnVel;   // dir * speed (what Birth events report as evt.vel)
    mu::Vec3      vel;        // spawnVel + vol linear (full analytic motion)
    mu::Vec3      grav;
};

inline std::uint32_t chainKey(std::uint32_t parentStream, std::uint32_t parentId,
                              int burstIdx, int cycle) {
    std::uint32_t k = mixSeed(parentStream, parentId);
    k = mixSeed(k, static_cast<std::uint32_t>(burstIdx));
    k = mixSeed(k, static_cast<std::uint32_t>(cycle));
    return k;
}

constexpr int kChainMaxDepth = 4;
constexpr int kEnumGuard     = 4096;

// Enumerates every spawn of `systemIdx` with spawnReal <= tReal (no liveness
// filter: chained children need dead parents too), recursively resolving
// chained parents.
inline void enumerateRealizedSpawns(const SystemResolver& resolve, int systemIdx,
                                    const EmitterFrame& frame, float tReal,
                                    const GroundQuery& ground,
                                    GroundConform conformOverride,
                                    int depth,
                                    std::vector<RealizedSpawn>& out) {
    if (depth > kChainMaxDepth)
        return;
    const SystemRef S = resolve(systemIdx);
    if (!S.cfg)
        return;
    const GameplayConfig& g = *S.cfg;
    const float sim = g.simulationSpeed;
    if (sim <= 0.f)
        return;

    const mu::Vec3 basePos = frame.playPos
        + rotateVector(g.baseShapePosition, frame.orientXform);
    const mu::Mat4x4 shapeWorldOrient = g.baseOrientation * frame.orientXform;
    const GroundConform conform =
        (conformOverride != GroundConform::None) ? conformOverride : g.groundConform;
    const float lifetimeCap = std::max(g.lifetimeMin, g.lifetimeMax);

    auto realize = [&](std::uint32_t stream, std::uint32_t id,
                       int emitIndex, int emitCount, float spawnReal,
                       const mu::Vec3* posOverride) {
        if (static_cast<int>(out.size()) >= kEnumGuard)
            return;
        // Target system (depth 0): certainly-dead spawns are never needed.
        // Parents (depth > 0) must be kept; their children may outlive them.
        if (depth == 0 && lifetimeCap > 0.f
            && (tReal - spawnReal) * sim >= lifetimeCap)
            return;
        DetRng rng{ S.seed, stream, id, 0 };
        // Chained children spawn via emitAt(): shape.position is REPLACED by
        // the event position (base offset not added), orientation kept.
        const mu::Vec3 shapePos = posOverride ? *posOverride : basePos;
        RealizedSpawn rs;
        rs.sp = sampleSpawn(g, shapePos, shapeWorldOrient, rng, emitIndex, emitCount);
        rs.spawnReal = spawnReal;
        rs.stream = stream;
        rs.id     = id;
        rs.origin = rs.sp.origin;
        if (conform != GroundConform::None && ground.height)
            rs.origin.setComponent(1, ground.height(rs.origin.x(), rs.origin.z())
                                      + g.groundOffset);
        rs.spawnVel = rs.sp.dir * rs.sp.speed;
        rs.vel = rs.spawnVel;
        if (g.volEnabled && g.volSupported) {
            // Mirrors calcVelocityOverLifetime: local-space linear velocity is
            // rotated by the emitter orientation (emitterTransformRotation).
            const mu::Vec3 volWorld = g.volInWorldSpace
                ? g.volLinear
                : rotateVector(g.volLinear, shapeWorldOrient);
            rs.vel += volWorld * g.volSpeedModifier;
        }
        rs.grav = g.gravity * rs.sp.gravityModifier;
        out.push_back(rs);
    };

    // -- chained child: spawns derive from parent births/deaths --------------
    if (S.chainParent >= 0 && S.chainParent != systemIdx) {
        const SystemRef P = resolve(S.chainParent);
        if (!P.cfg || P.cfg->simulationSpeed <= 0.f)
            return;

        std::vector<RealizedSpawn> parentSpawns;
        enumerateRealizedSpawns(resolve, S.chainParent, frame, tReal, ground,
                                conformOverride, depth + 1, parentSpawns);

        for (const RealizedSpawn& prs : parentSpawns) {
            // Event time/pos: Birth = at parent spawn; Death = end of life.
            const float parentLifeReal = prs.sp.lifetime / P.cfg->simulationSpeed;
            const float eventReal = S.chainOnBirth ? prs.spawnReal
                                                   : prs.spawnReal + parentLifeReal;
            if (eventReal > tReal)
                continue;

            mu::Vec3 deathPos{};
            if (!S.chainOnBirth) {
                const float a = prs.sp.lifetime;  // parent-scaled age at death
                deathPos = prs.origin + prs.vel * a + prs.grav * (0.5f * a * a);
            }

            // Replay the child's burst entries from the event time (mirrors
            // PendingSubEmitterBurst; due times are child-scaled; Birth events
            // advance the spawn point along the parent's spawn velocity).
            for (int bi = 0; bi < static_cast<int>(g.bursts.size()); ++bi) {
                const Burst& b = g.bursts[bi];
                for (int c = 0; c < b.cycleCount; ++c) {
                    const float due = b.time + b.repeatInterval * static_cast<float>(c);
                    const float spawnReal = eventReal + due / sim;
                    if (spawnReal > tReal)
                        break;
                    const mu::Vec3 pos = S.chainOnBirth
                        ? prs.origin + prs.spawnVel * due
                        : deathPos;
                    // NOTE: the client rolls probability/count on a visual rng;
                    // they only diverge for authored randomness (probability<1
                    // or countMin!=countMax) which chained gameplay systems
                    // should avoid. Count uses the deterministic draw.
                    const std::uint32_t key = chainKey(prs.stream, prs.id, bi, c);
                    int count = b.countMin;
                    if (b.countMin != b.countMax) {
                        DetRng meta{ S.seed, kStreamBurstMeta, key, 0 };
                        const int lo = std::min(b.countMin, b.countMax);
                        const int hi = std::max(b.countMin, b.countMax);
                        count = lo + static_cast<int>(std::floor(meta.next01()
                                  * static_cast<float>(hi - lo + 1)));
                    }
                    for (int i = 0; i < count; ++i)
                        realize(kStreamChainSpawn, burstParticleId(key, i),
                                i, count, spawnReal, &pos);
                }
            }
        }
        return;
    }

    // -- root system: own schedule -------------------------------------------
    const float tScaled = tReal * sim;

    if (S.mode == PlayMode::Emit) {
        // ParticleEffect::play() calls emit(1) immediately; startDelay does
        // not apply to manual emission.
        realize(kStreamPlayEmit, 0, 0, 1, 0.f, nullptr);
        return;
    }

    const float tSys = tScaled - g.startDelay;
    if (tSys <= 0.f || !g.emissionEnabled)
        return;
    auto sysToReal = [&](float spawnSys) { return (spawnSys + g.startDelay) / sim; };

    // Emission stops at duration unless looping (continuous flag clears).
    const bool  bounded    = (!g.looping && g.duration > 0.f);
    const float emitWindow = bounded ? std::min(tSys, g.duration) : tSys;

    // -- rate over time: k-th spawn at (k+1)/rate, ids monotonic over loops --
    if (g.emitRate > 0.f) {
        const int total = static_cast<int>(std::floor(g.emitRate * emitWindow));
        for (int k = 0; k < total; ++k) {
            const float spawnSys = static_cast<float>(k + 1) / g.emitRate;
            realize(kStreamRate, static_cast<std::uint32_t>(k), 0, 1,
                    sysToReal(spawnSys), nullptr);
        }
    }

    // -- scheduled bursts (per loop cycle) -----------------------------------
    const bool loops    = g.looping && g.duration > 0.f;
    const int  lastLoop = loops ? static_cast<int>(std::floor(tSys / g.duration)) : 0;
    for (int l = 0; l <= lastLoop; ++l) {
        const float loopBase = static_cast<float>(l) * (loops ? g.duration : 0.f);

        for (int bi = 0; bi < static_cast<int>(g.bursts.size()); ++bi) {
            const Burst& b = g.bursts[bi];
            for (int c = 0; c < b.cycleCount; ++c) {
                const float due = b.time + b.repeatInterval * static_cast<float>(c);
                if (g.duration > 0.f && due >= g.duration)
                    break;  // client clamps the burst window to [0, duration)
                const float absDue = loopBase + due;
                if (absDue >= emitWindow)
                    break;

                const std::uint32_t key = burstKey(l, bi, c);
                DetRng meta{ S.seed, kStreamBurstMeta, key, 0 };
                if (meta.next01() > b.probability)
                    continue;
                const int lo = std::min(b.countMin, b.countMax);
                const int hi = std::max(b.countMin, b.countMax);
                const int count = (lo >= hi)
                    ? lo
                    : lo + static_cast<int>(std::floor(meta.next01()
                            * static_cast<float>(hi - lo + 1)));

                for (int i = 0; i < count; ++i)
                    realize(kStreamBurstSpawn, burstParticleId(key, i),
                            i, count, sysToReal(absDue), nullptr);
            }
        }
    }
}

}  // namespace detail

// Evaluates the alive particle set of one system (root or chained child) at
// tRealSeconds since ParticleEffect::play(). Schedules and per-particle draws
// use the same keys as the client deterministic mode, so each particle's
// parameters match exactly; positions follow the analytic constant-velocity +
// gravity model (bounded by one client frame vs the client Euler integration).
inline void evaluateSystemParticles(const SystemResolver& resolve, int systemIdx,
                                    const EmitterFrame& frame,
                                    float tRealSeconds,
                                    const GroundQuery& ground,
                                    GroundConform conformOverride,
                                    int maxParticlesGuard,
                                    std::vector<ParticleState>& out) {
    using namespace detail;
    out.clear();
    if (tRealSeconds < 0.f)
        return;
    const SystemRef S = resolve(systemIdx);
    if (!S.cfg || S.cfg->simulationSpeed <= 0.f)
        return;
    const GameplayConfig& g = *S.cfg;

    std::vector<RealizedSpawn> spawns;
    enumerateRealizedSpawns(resolve, systemIdx, frame, tRealSeconds, ground,
                            conformOverride, 0, spawns);

    const mu::Mat4x4 meshRotWorld = g.meshBaseRotation * frame.orientXform;
    const GroundConform conform =
        (conformOverride != GroundConform::None) ? conformOverride : g.groundConform;
    const int guard = (g.maxParticles > 0)
                    ? std::min(g.maxParticles, maxParticlesGuard)
                    : maxParticlesGuard;

    for (const RealizedSpawn& rs : spawns) {
        if (static_cast<int>(out.size()) >= guard)
            break;
        const float ageScaled = (tRealSeconds - rs.spawnReal) * g.simulationSpeed;
        if (ageScaled < 0.f || rs.sp.lifetime <= 0.f || ageScaled >= rs.sp.lifetime)
            continue;

        ParticleState st;
        st.stream = rs.stream;
        st.id     = rs.id;
        st.pos  = rs.origin + rs.vel * ageScaled
                + rs.grav * (0.5f * ageScaled * ageScaled);
        st.ageT = ageScaled / rs.sp.lifetime;

        // sizeNow mirrors the client hitbox path:
        //   sizeMult = max-component of size3D (or the scalar draw)
        //   sz = sizeBegin + (sizeEnd - sizeBegin) * ageT
        const float sizeMult = g.startSize3DEnabled
            ? std::max(rs.sp.size3D.x(), std::max(rs.sp.size3D.y(), rs.sp.size3D.z()))
            : rs.sp.sizeScalar;
        const float sizeBegin = g.solEnabled ? g.solSizeBegin * sizeMult : sizeMult;
        const float sizeEnd   = g.solEnabled ? g.solSizeEnd   * sizeMult : sizeMult;
        st.sizeNow = sizeBegin + (sizeEnd - sizeBegin) * st.ageT;

        st.angularAngle3D = rs.sp.angularVelocity3D * ageScaled;

        st.baseRotation = rs.sp.rotation3DEnabled
            ? buildEulerRotation(rs.sp.rotation3D) * meshRotWorld
            : meshRotWorld;
        if (conform == GroundConform::SnapAndAlign && ground.normal)
            st.baseRotation = st.baseRotation
                            * alignYToNormalMat(ground.normal(rs.origin.x(), rs.origin.z()));

        out.push_back(st);
    }
}

// Single-root-system convenience wrapper (no chains).
inline void evaluateParticles(const GameplayConfig& g,
                              std::uint32_t  systemSeed,
                              PlayMode       mode,
                              const EmitterFrame& frame,
                              float          tRealSeconds,
                              const GroundQuery&  ground,
                              GroundConform  conformOverride,
                              int            maxParticlesGuard,
                              std::vector<ParticleState>& out) {
    auto resolve = [&](int idx) -> SystemRef {
        SystemRef r;
        if (idx == 0) { r.cfg = &g; r.seed = systemSeed; r.mode = mode; }
        return r;
    };
    evaluateSystemParticles(resolve, 0, frame, tRealSeconds, ground,
                            conformOverride, maxParticlesGuard, out);
}

// ---------------------------------------------------------------------------
// Gameplay config import from the Unity effect JSON
// (mirrors client/particleImporter.cpp for the gameplay subset)
// ---------------------------------------------------------------------------

namespace detail {

constexpr float kDegToRad = 3.14159265358979323846f / 180.f;

inline bool readTextFile(const std::filesystem::path& path, std::string& out) {
    auto ifs = std::ifstream(path, std::ios::binary);
    if (!ifs)
        return false;
    ifs.seekg(0, std::ios::end);
    const auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(size));
    if (!out.empty())
        ifs.read(out.data(), static_cast<std::streamsize>(out.size()));
    return ifs.good() || ifs.eof();
}

inline const json::Value* find(const json::Value* v, std::string_view key) {
    return v ? v->find(key) : nullptr;
}

inline std::string readString(const json::Value* o, std::string_view key,
                              std::string fallback = {}) {
    const auto* v = find(o, key);
    return v && v->isString() ? v->asString() : std::move(fallback);
}

inline float readFloat(const json::Value* o, std::string_view key, float fallback = 0.f) {
    const auto* v = find(o, key);
    return v && v->isNumber() ? static_cast<float>(v->asNumber()) : fallback;
}

inline int readInt(const json::Value* o, std::string_view key, int fallback = 0) {
    return static_cast<int>(std::lround(readFloat(o, key, static_cast<float>(fallback))));
}

inline bool readBool(const json::Value* o, std::string_view key, bool fallback = false) {
    const auto* v = find(o, key);
    return v && v->isBool() ? v->asBool() : fallback;
}

inline mu::Vec3 readVec3(const json::Value* o, std::string_view key,
                         mu::Vec3 fallback = { 0.f, 0.f, 0.f }) {
    const auto* v = find(o, key);
    if (!v || !v->isObject())
        return fallback;
    return { readFloat(v, "x"), readFloat(v, "y"), readFloat(v, "z") };
}

// Endpoint range of a MinMaxCurve. Mirrors readCurveRange in
// client/particleImporter.cpp: Curve endpoints evaluate at t=0/1, which the
// client FloatCurve clamps to the first/last key value.
inline std::pair<float, float> readCurveRange(const json::Value* v) {
    if (!v || !v->isObject())
        return { 0.f, 0.f };

    const std::string mode = readString(v, "mode");
    const float constant    = readFloat(v, "constant");
    const float constantMin = readFloat(v, "constantMin");
    const float constantMax = readFloat(v, "constantMax", constant);
    const float mult        = readFloat(v, "curveMultiplier", 1.f);

    auto curveEndpoints = [&](std::string_view curveKey) -> std::pair<float, float> {
        const auto* curve = find(v, curveKey);
        const auto* keys  = curve ? curve->find("keys") : nullptr;
        if (!keys || !keys->isArray() || keys->asArray().empty())
            return { 0.f, 0.f };
        const auto& arr = keys->asArray();
        return { readFloat(&arr.front(), "value"), readFloat(&arr.back(), "value") };
    };

    if (mode == "TwoConstants")
        return { constantMin, constantMax };
    if (mode == "Curve") {
        const auto [a, b] = curveEndpoints("curve");
        return { a * mult, b * mult };
    }
    if (mode == "TwoCurves") {
        const auto [a, _unusedA] = curveEndpoints("curveMin");
        const auto [_unusedB, b] = curveEndpoints("curveMax");
        return { a * mult, b * mult };
    }
    return { constant, constant };
}

inline std::pair<float, float> orderedRange(std::pair<float, float> r) {
    if (r.first > r.second)
        std::swap(r.first, r.second);
    return r;
}

inline ShapeType readShapeType(std::string_view s) {
    if (s == "Edge")       return ShapeType::Edge;
    if (s == "Cone")       return ShapeType::Cone;
    if (s == "Sphere")     return ShapeType::Sphere;
    if (s == "Hemisphere") return ShapeType::Hemisphere;
    if (s == "Box")        return ShapeType::Box;
    if (s == "Circle")     return ShapeType::Circle;
    return ShapeType::Point;
}

inline mu::Mat4x4 eulerDegreesToMat(mu::Vec3 degrees) {
    return mu::rotateXH(mu::Degree{ degrees.x() })
         * mu::rotateYH(mu::Degree{ degrees.y() })
         * mu::rotateZH(mu::Degree{ degrees.z() });
}

inline float propFloatOf(const json::Value* propsArray, std::string_view path,
                         float fallback) {
    if (!propsArray || !propsArray->isArray())
        return fallback;
    for (const auto& prop : propsArray->asArray()) {
        if (readString(&prop, "path") == path) {
            const std::string value = readString(&prop, "value");
            char* end = nullptr;
            const float f = std::strtof(value.c_str(), &end);
            return end == value.c_str() ? fallback : f;
        }
    }
    return fallback;
}

inline bool propBoolOf(const json::Value* propsArray, std::string_view path,
                       bool fallback) {
    if (!propsArray || !propsArray->isArray())
        return fallback;
    for (const auto& prop : propsArray->asArray()) {
        if (readString(&prop, "path") == path) {
            const std::string value = readString(&prop, "value");
            return value == "True" || value == "true" || value == "1";
        }
    }
    return fallback;
}

}  // namespace detail

// Reads and parses an effect JSON into a DOM. Callers importing several
// systems from the same file should parse once and use the DOM overload.
inline bool loadEffectJson(const std::filesystem::path& jsonPath,
                           json::Value& outRoot,
                           std::string* error = nullptr) {
    using namespace detail;

    std::string text;
    if (!readTextFile(jsonPath, text)) {
        if (error) *error = "failed to read " + jsonPath.string();
        return false;
    }
    std::string parseError;
    if (!json::parse(text, outRoot, &parseError)) {
        if (error) *error = "failed to parse " + jsonPath.string() + ": " + parseError;
        return false;
    }
    return true;
}

// Loads the gameplay subset of one system (identified by relativePath, e.g.
// "Crystals front attack") from a parsed effect JSON DOM.
inline bool importGameplayConfig(const json::Value& root,
                                 std::string_view relativePath,
                                 GameplayConfig& out,
                                 std::string* error = nullptr) {
    using namespace detail;

    const auto* systems = root.find("systems");
    if (!systems || !systems->isArray()) {
        if (error) *error = "effect JSON has no systems array";
        return false;
    }

    const json::Value* system = nullptr;
    for (const auto& s : systems->asArray()) {
        const std::string rel = readString(&s, "relativePath", readString(&s, "name"));
        if (rel == relativePath) { system = &s; break; }
    }
    if (!system) {
        if (error) *error = "effect JSON has no system \""
                          + std::string(relativePath) + "\"";
        return false;
    }

    const auto* summary = find(system, "summary");
    if (!summary) {
        if (error) *error = "system \"" + std::string(relativePath) + "\" has no summary";
        return false;
    }
    const auto* props = find(system, "particleSystemSerializedProperties");

    GameplayConfig g;

    // -- main (mirrors readMainModule) ---------------------------------------
    const auto* main = find(summary, "main");
    {
        auto [lifeMin, lifeMax] = orderedRange(readCurveRange(find(main, "startLifetime")));
        auto [spdMin, spdMax]   = orderedRange(readCurveRange(find(main, "startSpeed")));
        auto [szMin, szMax]     = orderedRange(readCurveRange(find(main, "startSize")));
        auto [grvMin, grvMax]   = orderedRange(readCurveRange(find(main, "gravityModifier")));
        auto [rotMin, rotMax]   = orderedRange(readCurveRange(find(main, "startRotation")));

        g.lifetimeMin = lifeMin;   g.lifetimeMax = lifeMax;
        g.speedMin = spdMin;       g.speedMax = spdMax;
        g.startSizeMin = szMin;    g.startSizeMax = szMax;
        g.gravityModifierMin = grvMin;
        g.gravityModifierMax = grvMax;
        g.startRotationMin = rotMin;
        g.startRotationMax = rotMax;

        g.startSize3DEnabled = readBool(main, "startSize3D", false);
        if (g.startSize3DEnabled) {
            auto [xMin, xMax] = orderedRange(readCurveRange(find(main, "startSizeX")));
            auto [yMin, yMax] = orderedRange(readCurveRange(find(main, "startSizeY")));
            auto [zMin, zMax] = orderedRange(readCurveRange(find(main, "startSizeZ")));
            g.startSize3DMin = { xMin, yMin, zMin };
            g.startSize3DMax = { xMax, yMax, zMax };
        } else {
            g.startSize3DMin = { szMin, szMin, szMin };
            g.startSize3DMax = { szMax, szMax, szMax };
        }

        g.startRotation3DEnabled = readBool(main, "startRotation3D", false);
        if (g.startRotation3DEnabled) {
            auto [xMin, xMax] = orderedRange(readCurveRange(find(main, "startRotationX")));
            auto [yMin, yMax] = orderedRange(readCurveRange(find(main, "startRotationY")));
            auto [zMin, zMax] = orderedRange(readCurveRange(find(main, "startRotationZ")));
            g.startRotation3DMin = { xMin, yMin, zMin };
            g.startRotation3DMax = { xMax, yMax, zMax };
        }

        g.duration        = readFloat(main, "duration", g.duration);
        g.looping         = readBool(main, "loop", g.looping);
        g.startDelay      = readCurveRange(find(main, "startDelay")).first;
        g.simulationSpeed = readFloat(main, "simulationSpeed", g.simulationSpeed);
        g.maxParticles    = readInt(main, "maxParticles", g.maxParticles);
    }

    // -- emission (mirrors readEmissionModule) -------------------------------
    const auto* emission = find(summary, "emission");
    if (emission) {
        g.emissionEnabled = readBool(emission, "enabled", g.emissionEnabled);
        g.emitRate = readCurveRange(find(emission, "rateOverTime")).second;

        const auto* bursts = find(emission, "bursts");
        if (bursts && bursts->isArray()) {
            for (const auto& b : bursts->asArray()) {
                auto [cntMin, cntMax] = orderedRange(readCurveRange(find(&b, "count")));
                g.bursts.push_back(Burst{
                    .time           = readFloat(&b, "time"),
                    .countMin       = static_cast<int>(std::lround(cntMin)),
                    .countMax       = static_cast<int>(std::lround(cntMax)),
                    .cycleCount     = readInt(&b, "cycleCount", 1),
                    .repeatInterval = readFloat(&b, "repeatInterval", 0.01f),
                    .probability    = readFloat(&b, "probability", 1.f)
                });
            }
        }
    }

    // -- shape + transform (mirrors readShapeModule + readTransform) ---------
    const auto* shape = find(summary, "shape");
    mu::Vec3 shapePosition = { 0.f, 0.f, 0.f };
    if (shape) {
        g.shapeEnabled = readBool(shape, "enabled", g.shapeEnabled);
        g.type = readShapeType(readString(shape, "shapeType"));
        shapePosition = readVec3(shape, "position", shapePosition);
        g.rotationEuler = readVec3(shape, "rotation") * kDegToRad;
        g.scale = readVec3(shape, "scale", g.scale);
        g.coneAngle = readFloat(shape, "angle", 25.f) * kDegToRad;
        g.coneRadius = readFloat(shape, "radius", g.coneRadius);
        g.sphereRadius = readFloat(shape, "radius", g.sphereRadius);
        g.radiusThickness = readFloat(shape, "radiusThickness", g.radiusThickness);
        g.arc = readFloat(shape, "arc", 360.f) * kDegToRad;
        g.arcMode = static_cast<int>(propFloatOf(props, "ShapeModule.arc.mode",
                                                 static_cast<float>(g.arcMode)));
        g.randomPositionAmount = readFloat(shape, "randomPositionAmount",
                                           g.randomPositionAmount);
        if (g.type == ShapeType::Box)
            g.boxSize = g.scale;
    }

    const auto* transform = find(system, "transform");
    const bool isRoot = relativePath.find('/') == std::string_view::npos
                     && relativePath.find('\\') == std::string_view::npos;
    if (transform) {
        if (!isRoot)
            shapePosition += readVec3(transform, "localPosition");
        g.meshBaseRotation = eulerDegreesToMat(readVec3(transform, "localEulerAngles"));
        g.baseOrientation  = g.meshBaseRotation;
        g.scale *= readVec3(transform, "localScale", { 1.f, 1.f, 1.f });
    }
    g.baseShapePosition = shapePosition;

    // -- sizeOverLifetime scalar range (mirrors readSizeModule) ---------------
    const auto* size = find(summary, "sizeOverLifetime");
    if (size) {
        g.solEnabled = readBool(size, "enabled", g.solEnabled);
        const auto range = readCurveRange(find(size, "size"));
        g.solSizeBegin = range.first;
        g.solSizeEnd   = range.second;
    }

    // -- rotationOverLifetime (mirrors readRotationModule) --------------------
    const auto* rotation = find(summary, "rotationOverLifetime");
    if (rotation) {
        g.rolEnabled = readBool(rotation, "enabled", g.rolEnabled);
        g.rolSeparateAxes = readBool(rotation, "separateAxes", g.rolSeparateAxes);
        // The client importer sets useCurves = enabled; the client runtime
        // then integrates curves per frame, which this module cannot
        // reproduce exactly. Constant curves degenerate to the constant
        // angular-velocity path, so only flag non-constant data.
        if (g.rolEnabled) {
            auto curveLike = [&](std::string_view key) {
                const std::string mode = readString(find(rotation, key), "mode");
                return mode == "Curve" || mode == "TwoCurves";
            };
            if (curveLike("x") || curveLike("y") || curveLike("z"))
                g.rolUseCurves = true;
            if (!g.rolSeparateAxes) {
                auto [aMin, aMax] = orderedRange(readCurveRange(find(rotation, "z")));
                g.angularVelocityMin = aMin;
                g.angularVelocityMax = aMax;
            } else {
                auto [xMin, xMax] = orderedRange(readCurveRange(find(rotation, "x")));
                auto [yMin, yMax] = orderedRange(readCurveRange(find(rotation, "y")));
                auto [zMin, zMax] = orderedRange(readCurveRange(find(rotation, "z")));
                g.angularVelocityMin3D = { xMin, yMin, zMin };
                g.angularVelocityMax3D = { xMax, yMax, zMax };
            }
        }
    }

    // -- velocityOverLifetime: constant-linear subset only --------------------
    g.volEnabled = propBoolOf(props, "VelocityModule.enabled", false);
    if (g.volEnabled) {
        auto channelConst = [&](std::string_view base, float fallback) -> std::pair<bool, float> {
            const std::string b(base);
            const int state = static_cast<int>(
                propFloatOf(props, b + ".minMaxState", 0.f));
            const float scalar = propFloatOf(props, b + ".scalar", fallback);
            return { state == 0, scalar };  // Constant mode only
        };
        auto [xOk, x] = channelConst("VelocityModule.x", 0.f);
        auto [yOk, y] = channelConst("VelocityModule.y", 0.f);
        auto [zOk, z] = channelConst("VelocityModule.z", 0.f);
        auto [oxOk, ox] = channelConst("VelocityModule.orbitalX", 0.f);
        auto [oyOk, oy] = channelConst("VelocityModule.orbitalY", 0.f);
        auto [ozOk, oz] = channelConst("VelocityModule.orbitalZ", 0.f);
        auto [rOk,  r]  = channelConst("VelocityModule.radial", 0.f);
        auto [smOk, sm] = channelConst("VelocityModule.speedModifier", 1.f);

        g.volLinear = { x, y, z };
        g.volSpeedModifier = sm;
        g.volInWorldSpace = propBoolOf(props, "VelocityModule.inWorldSpace", false);
        const bool orbitalZero = oxOk && oyOk && ozOk
                               && ox == 0.f && oy == 0.f && oz == 0.f
                               && rOk && r == 0.f;
        g.volSupported = xOk && yOk && zOk && smOk && orbitalZero;
    }

    out = std::move(g);
    return true;
}

// Convenience: file-path variant (one parse per call; prefer the DOM overload
// when importing several systems from the same file).
inline bool importGameplayConfig(const std::filesystem::path& jsonPath,
                                 std::string_view relativePath,
                                 GameplayConfig& out,
                                 std::string* error = nullptr) {
    json::Value root;
    if (!loadEffectJson(jsonPath, root, error))
        return false;
    if (!importGameplayConfig(root, relativePath, out, error)) {
        if (error) *error = jsonPath.string() + ": " + *error;
        return false;
    }
    return true;
}

// Applies the Lua-authored per-system overrides on top of an imported (or
// default-constructed, for code-built systems) gameplay config. Mirrors the
// client-side cfg tweaks done while composing the ParticleEffect.
inline void applyOverrides(GameplayConfig& g, const VfxSystemOverrides& o) {
    using namespace detail;

    if (o.hasLooping)      g.looping  = o.looping;
    if (o.hasDuration)     g.duration = o.duration;
    if (o.hasSpeed)        { g.speedMin = o.speedMin; g.speedMax = o.speedMax; }
    if (o.hasLifetime)     { g.lifetimeMin = o.lifetimeMin; g.lifetimeMax = o.lifetimeMax; }
    if (o.hasStartSize) {
        g.startSizeMin = o.startSizeMin;
        g.startSizeMax = o.startSizeMax;
        g.startSize3DEnabled = false;
        g.startSize3DMin = { o.startSizeMin, o.startSizeMin, o.startSizeMin };
        g.startSize3DMax = { o.startSizeMax, o.startSizeMax, o.startSizeMax };
    }
    if (o.hasMaxParticles)    g.maxParticles = o.maxParticles;
    if (o.hasShapeType)       { g.type = o.shapeType; g.shapeEnabled = true; }
    if (o.hasShapePosition)   g.baseShapePosition = o.shapePosition;
    if (o.hasShapeEulerDeg)   g.baseOrientation = eulerDegreesToMat(o.shapeEulerDeg);
    if (o.hasMeshEulerDeg)    g.meshBaseRotation = eulerDegreesToMat(o.meshEulerDeg);
    if (o.hasDirection)       g.direction = o.direction;
    if (o.hasBoxSize)         g.boxSize = o.boxSize;
    if (o.hasConeRadius)      g.coneRadius = o.coneRadius;
    if (o.hasConeAngleDeg)    g.coneAngle = o.coneAngleDeg * kDegToRad;
    if (o.hasRadiusThickness) g.radiusThickness = o.radiusThickness;
    if (o.hasEmitRate)        g.emitRate = o.emitRate;
    if (o.hasBursts)          { g.bursts = o.bursts; g.emissionEnabled = true; }
    if (o.hasVolLinear) {
        g.volEnabled       = true;
        g.volSupported     = true;
        g.volInWorldSpace  = false;   // mirrors the client local-space stab velocity
        g.volLinear        = o.volLinear;
        g.volSpeedModifier = 1.f;
    }
}

}  // namespace pg

#endif  // __particleGameplay_HPP
