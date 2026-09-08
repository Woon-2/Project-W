#ifndef __particleSystem_HPP
#define __particleSystem_HPP

#include "particleModules.hpp"
#include "groundSampler.hpp"
#include "particleGameplay.hpp"

#include <array>
#include <cstdint>
#include <vector>
#include <random>

class GFX;

// Trail ring buffer entry. simulationSpace == World -> pos is world-space.
// simulationSpace == Local -> pos is emitter-local (applied with localToWorld at draw time).
struct TrailPoint {
    mu::Vec3 pos       = { 0.f, 0.f, 0.f };
    float    spawnTime = 0.f;   // ParticleSystem::systemTime_ at capture
};

struct Particle {
    static constexpr int kMaxTrailSegments = 32;

    mu::Vec3        pos, vel;
    mu::Vec3        motionVelocity = { 0.f, 0.f, 0.f };
    mu::Vec3        emitterPosition = { 0.f, 0.f, 0.f };
    mu::Mat4x4      emitterRotation;
    mu::Mat4x4      emitterTransformRotation;
    float           lifetime, maxLifetime;
    mu::Vec4        startColor;
    ColorGradient   colorOverLifetime;
    float           sizeBegin, sizeEnd, drag;
    float           sizeStart = 1.f;
    mu::Vec3        sizeBegin3D = { 1.f, 1.f, 1.f };
    mu::Vec3        sizeEnd3D   = { 1.f, 1.f, 1.f };
    mu::Vec3        sizeStart3D = { 1.f, 1.f, 1.f };
    float           sizeRandom = 0.f;
    mu::Vec3        gravity;
    float           rotation;

    // Mesh renderer fields
    float           angularVelocity = 0.f;   // rad/sec, local Z axis
    float           angularAngle    = 0.f;   // accumulated rotation (radians)
    mu::Vec3        angularVelocity3D = { 0.f, 0.f, 0.f };
    mu::Vec3        angularAngle3D    = { 0.f, 0.f, 0.f };
    mu::Vec3        rotationRandom3D  = { 0.f, 0.f, 0.f };
    mu::Vec3        velocityRandom3D  = { 0.f, 0.f, 0.f };
    mu::Vec3        orbitalRandom3D   = { 0.f, 0.f, 0.f };
    mu::Vec2        velocityRandomExtra = { 0.f, 0.f };
    mu::Mat4x4      baseRotation;            // fixed 3D orientation set at spawn
    mu::Mat4x4      billboardRotation3D;     // Unity Start Rotation 3D for billboard quads
    mu::Vec3        transformScale = { 1.f, 1.f, 1.f };
    mu::Vec2        custom1Random   = { 0.f, 0.f };
    mu::Vec2        custom2Random   = { 0.f, 0.f };

    // Trail ring buffer. Only valid when trailEnabled is true.
    // Layout: trail[(trailHead - trailCount + 32) & 31] = oldest, ..., trail[(trailHead - 1 + 32) & 31] = newest.
    std::array<TrailPoint, kMaxTrailSegments> trail{};
    std::uint8_t   trailHead     = 0;
    std::uint8_t   trailCount    = 0;
    bool           trailEnabled  = false;
    float          trailLifetime = 0.f;     // sampled per-particle from TrailModule.lifetime[Min|Max]

    // 'active' 필드 없음: pool_[0..activeCount_-1] 이 항상 활성 상태
};

// Event emitted by ParticleSystem::update() for each sub-emitter trigger.
// ParticleEffect reads these after update() and calls emitAt() on child systems.
struct SubEmitterEvent {
    int      configIndex;  // index into config_.subEmitters.subEmitters
    mu::Vec3 pos;
    mu::Vec3 vel;
    mu::Vec4 color;
    float    size;
};

class ParticleSystem {
public:
    static constexpr int kDefaultMaxParticles = 4096;
    static constexpr int kMaxParticles = kDefaultMaxParticles;  // backward-compat alias

    // Configures the system and sizes the pool. Must be called before emit(int).
    void init(const ps::ParticleSystemConfig& config, int maxParticles = kDefaultMaxParticles);

    // Emit particles using the config set by init().
    // sizeScale: 이 emit으로 생성되는 파티클의 크기 배율(1회성, emit 후 1.0으로 복원).
    // 피격 VFX의 hit별 크기 연출(예: 피니셔 타격 큰 혈흔)에 사용.
    void emit(int count, float sizeScale = 1.f);

    // Start/stop continuous emission using the config set by init().
    void startContinuous();

    // Convenience overload: sets config then starts continuous emission.
    void startContinuous(const ps::ParticleSystemConfig& config);

    void update(Seconds dt);
    void render(GFX& gfx) const;
    void stopContinuous();

    // Destroys the particle at compact index `index` (into the particles()
    // array, [0..activeCount()-1]) immediately. Reuses the standard death path:
    // fires Death sub-emitter events at the particle's position (so a chained
    // child effect, e.g. an explosion, spawns) and then swap-removes it.
    // External control hook for the skill system (non-penetrating hitboxes);
    // keeps the VFX/skill boundary to a single public call. Out-of-range = no-op.
    void killParticle(int index);

    // Spawn particles at worldPos (used by ParticleEffect sub-emitter dispatch).
    void emitAt(int count, mu::Vec3 worldPos,
                mu::Vec3 inheritVel   = {},
                mu::Vec4 inheritColor = { 1.f, 1.f, 1.f, 1.f },
                float    inheritSize  = 1.f);

    const std::vector<SubEmitterEvent>& pendingSubEmitterEvents() const {
        return pendingSubEmitterEvents_;
    }
    void clearPendingSubEmitterEvents() { pendingSubEmitterEvents_.clear(); }

    int activeCount() const { return activeCount_; }

    // Read-only access to the compact active-particle array [0..activeCount()-1].
    const Particle* particles() const { return pool_.data(); }

    ps::ParticleSystemConfig&       config()       { return config_; }
    const ps::ParticleSystemConfig& config() const { return config_; }

    // Bind a terrain query for ground-conform spawn (ShapeModule::groundConform)
    // and ground collision (ParticleCollisionModule). Non-owning; the pointed-to
    // GroundSampler must outlive this system. nullptr disables ground awareness.
    void setGroundSampler(const GroundSampler* g) { ground_ = g; }

    // --- Deterministic gameplay mode (see common/particleGameplay.hpp) ------
    // When a gameplay config AND a seed are set, gameplay-relevant spawn
    // parameters (origin, direction, speed, lifetime, size, rotation) come
    // from pg::sampleSpawn with counter-based RNG keys, so the server can
    // reproduce them exactly. Visual-only draws keep using the legacy rng_.
    void setGameplayConfig(const pg::GameplayConfig& cfg) {
        gameplayCfg_    = cfg;
        hasGameplayCfg_ = true;
    }
    // Called per cast (before play()); also resets the spawn counters.
    void setDeterministicSeed(std::uint32_t seed) {
        detSeed_      = seed;
        detSeedSet_   = true;
        detRateIndex_ = 0;
        detEmitIndex_ = 0;
        detLoopIndex_ = 0;
        detSysTime_   = 0.f;
    }
    bool deterministic() const { return hasGameplayCfg_ && detSeedSet_; }
    const pg::GameplayConfig* gameplayConfig() const {
        return hasGameplayCfg_ ? &gameplayCfg_ : nullptr;
    }

private:
    struct ShapeSample { mu::Vec3 origin; mu::Vec3 dir; };

    float        randomFloat(float lo, float hi);
    float        sampleArcAngle();
    void         spawnParticle();
    // Fires Death sub-emitter events for pool_[i] then swap-removes it.
    // Shared by update()'s lifetime<=0 path and the public killParticle().
    // Does NOT advance the caller's loop index (the swapped-in particle at i
    // must be re-processed).
    void         killParticleAt(int i);
    void         emitScheduledBursts(float prevTime, float currTime);
    void         emitScheduledBurstsDet(float prevTime, float currTime);
    void         emitRateDet();
    ShapeSample  sampleCircle();
    ShapeSample  sampleCone();
    mu::Vec3     sampleShapeOrigin();
    mu::Vec3     sampleShapeDirection(const mu::Vec3& origin);

    // pool_[0..activeCount_-1] 이 항상 활성 파티클 (compact array)
    std::vector<Particle> pool_ = std::vector<Particle>(kDefaultMaxParticles);
    int          activeCount_     = 0;
    int          overwriteCursor_ = 0;
    int          maxParticles_    = kDefaultMaxParticles;
    std::mt19937 rng_{ std::random_device{}() };

    ps::ParticleSystemConfig config_;
    const GroundSampler*      ground_ = nullptr;  // non-owning terrain query (optional)
    std::vector<int>          burstNextCycle_;
    float                    emitAccumNew_   = 0.f;
    bool                     continuousNew_  = false;
    float                    systemTime_     = 0.f;
    float                    delayRemaining_ = 0.f;

    std::vector<SubEmitterEvent> pendingSubEmitterEvents_;
    bool      inheritEmit_      = false;
    mu::Vec3  inheritedVel_     = {};
    mu::Vec4  inheritedColor_   = { 1.f, 1.f, 1.f, 1.f };
    float     inheritedSize_    = 1.f;
    float     emitSizeScale_    = 1.f;   // emit(count, sizeScale) 1회성 크기 배율
    int       shapeEmitIndex_   = 0;
    int       shapeEmitCount_   = 1;

    // Deterministic gameplay mode state. The spawn identity (stream/id) is
    // set immediately before each spawnParticle() call by the det emission
    // paths; emitAt (sub-emitter inherit) spawns always use the legacy path.
    pg::GameplayConfig gameplayCfg_;
    bool          hasGameplayCfg_ = false;
    std::uint32_t detSeed_        = 0;
    bool          detSeedSet_     = false;
    std::uint32_t detRateIndex_   = 0;    // rate stream: monotonic particle index
    std::uint32_t detEmitIndex_   = 0;    // play-emit stream: manual emit counter
    int           detLoopIndex_   = 0;    // looping systems: loop iteration
    float         detSysTime_     = 0.f;  // monotonic scaled time after startDelay
    std::uint32_t detSpawnStream_ = 0;
    std::uint32_t detSpawnId_     = 0;
    bool          detSpawnPending_ = false;
};

#endif  // __particleSystem_HPP
