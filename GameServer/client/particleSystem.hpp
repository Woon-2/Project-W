#ifndef __particleSystem_HPP
#define __particleSystem_HPP

#include "particleModules.hpp"

#include <vector>
#include <random>

class GFX;

struct Particle {
    mu::Vec3        pos, vel;
    mu::Vec3        motionVelocity = { 0.f, 0.f, 0.f };
    mu::Vec3        emitterPosition = { 0.f, 0.f, 0.f };
    mu::Mat4x4      emitterRotation;
    float           lifetime, maxLifetime;
    mu::Vec4        startColor;
    ColorGradient   colorOverLifetime;
    float           sizeBegin, sizeEnd, drag;
    float           sizeStart = 1.f;
    float           sizeRandom = 0.f;
    mu::Vec3        gravity;
    float           rotation;

    // Mesh renderer fields
    float           angularVelocity = 0.f;   // rad/sec, local Z axis
    float           angularAngle    = 0.f;   // accumulated rotation (radians)
    mu::Vec3        angularVelocity3D = { 0.f, 0.f, 0.f };
    mu::Vec3        angularAngle3D    = { 0.f, 0.f, 0.f };
    mu::Vec3        rotationRandom3D  = { 0.f, 0.f, 0.f };
    mu::Mat4x4      baseRotation;            // fixed 3D orientation set at spawn
    mu::Mat4x4      billboardRotation3D;     // Unity Start Rotation 3D for billboard quads
    mu::Vec3        transformScale = { 1.f, 1.f, 1.f };
    mu::Vec2        custom1Random   = { 0.f, 0.f };
    mu::Vec2        custom2Random   = { 0.f, 0.f };

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
    void emit(int count);

    // Start/stop continuous emission using the config set by init().
    void startContinuous();

    // Convenience overload: sets config then starts continuous emission.
    void startContinuous(const ps::ParticleSystemConfig& config);

    void update(Seconds dt);
    void render(GFX& gfx) const;
    void stopContinuous();

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

    ps::ParticleSystemConfig&       config()       { return config_; }
    const ps::ParticleSystemConfig& config() const { return config_; }

private:
    struct ShapeSample { mu::Vec3 origin; mu::Vec3 dir; };

    float        randomFloat(float lo, float hi);
    void         spawnParticle();
    void         emitScheduledBursts(float prevTime, float currTime);
    ShapeSample  sampleCircle();
    mu::Vec3     sampleShapeOrigin();
    mu::Vec3     sampleShapeDirection(const mu::Vec3& origin);

    // pool_[0..activeCount_-1] 이 항상 활성 파티클 (compact array)
    std::vector<Particle> pool_ = std::vector<Particle>(kDefaultMaxParticles);
    int          activeCount_     = 0;
    int          overwriteCursor_ = 0;
    int          maxParticles_    = kDefaultMaxParticles;
    std::mt19937 rng_{ std::random_device{}() };

    ps::ParticleSystemConfig config_;
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
};

#endif  // __particleSystem_HPP
