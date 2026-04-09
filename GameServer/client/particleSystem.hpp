#ifndef __particleSystem_HPP
#define __particleSystem_HPP

#include "spriteAnimation.hpp"
#include "particlePayload.hpp"
#include "particleModules.hpp"

#include <vector>
#include <random>

enum class EmitterShape { Point, Edge };

struct EmitterConfig {
    mu::Vec3                   position;
    mu::Vec3                   direction   = {0, 1, 0};  // world-space emit axis
    float                      spread      = 0.3f;       // cone half-angle (radians), 흩뿌리는 방식을 원한다면 이 옵션을.
    float                      speedMin    = 1.f;
    float                      speedMax    = 3.f;
    float                      lifetimeMin = 0.5f;
    float                      lifetimeMax = 1.5f;
    mu::Vec4                   startColor          = {1.f, 1.f, 1.f, 1.f};
    ColorGradient              colorOverLifetime   = ColorGradient::constant({1.f, 1.f, 1.f, 1.f});
    float                      sizeBegin        = 1.f;
    float                      sizeEnd          = 1.f;
    float                      sizeMultiplierMin = 1.f;  // 파티클 크기 배율 min (Unity Start Size)
    float                      sizeMultiplierMax = 1.f;  // 파티클 크기 배율 max
    float                      drag        = 0.f;
    mu::Vec3                   gravity     = {0.f, -9.8f, 0.f};
    float                      gravityModifierMin = 1.f;
    float                      gravityModifierMax = 1.f;
    float                      startRotationMin = 0.f;   // radians
    float                      startRotationMax = 0.f;
    const SpriteAnimationClip* pClip       = nullptr;
    EmitterShape               shape        = EmitterShape::Point;
    float                      edgeLength   = 1.f;
    mu::Vec3                   edgeDir      = {1.f, 0.f, 0.f};
    bool                       additiveBlend = true;
    int                        renderOrder  = 0;         // 낮을수록 먼저 렌더 (같은 blend 모드 내 정렬 기준)
    float                      emitRate     = 0.f;       // particles/sec (0이면 수동 emit)
};

class GFX;

struct Particle {
    mu::Vec3        pos, vel;
    float           lifetime, maxLifetime;
    mu::Vec4        startColor;
    ColorGradient   colorOverLifetime;
    float           sizeBegin, sizeEnd, drag;
    mu::Vec3        gravity;
    SpriteAnimation anim;
    float           rotation;
    bool            additive;
    bool            hasAnim = false;   // true if anim was initialised with a clip

    // Mesh renderer fields (unused for billboard; updated in update() when set)
    float           angularVelocity = 0.f;   // rad/sec, local Z axis
    float           angularAngle    = 0.f;   // accumulated rotation (radians)
    mu::Mat4x4      baseRotation;            // fixed orientation set at spawn; Phase 2 renders it

    // Semantic payload — filled by updatePayload() each frame; consumed by renderer backend.
    // Phase 1: payload is populated but the old render path (SpriteAnimation::render) still runs.
    // Phase 2: old render path removed; only payload used.
    ParticleSemanticPayload payload;

    // 'active' 필드 없음: pool_[0..activeCount_-1] 이 항상 활성 상태
};

class ParticleSystem {
public:
    static constexpr int kDefaultMaxParticles = 4096;
    static constexpr int kMaxParticles = kDefaultMaxParticles;  // backward-compat alias

    // ── New module-based API ─────────────────────────────────────────────────
    // Configures the system and sizes the pool.  Must be called before emit(int).
    void init(const ParticleSystemConfig& config, int maxParticles = kDefaultMaxParticles);

    // Emit particles using the config set by init().
    void emit(int count);

    // Start/stop continuous emission using the config set by init().
    void startContinuous();

    // Convenience overload: sets config (equivalent to init) then starts continuous.
    void startContinuous(const ParticleSystemConfig& config);

    // ── Legacy API (kept for backward compatibility) ─────────────────────────
    void emit(const EmitterConfig& config, int count);
    void startContinuous(const EmitterConfig& config);

    // ── Shared API ───────────────────────────────────────────────────────────
    void update(Seconds dt);
    void render(GFX& gfx) const;
    void stopContinuous();

    int activeCount() const { return activeCount_; }

private:
    float    randomFloat(float lo, float hi);

    // New-path helpers
    void     spawnParticle();
    mu::Vec3 sampleShapeOrigin();
    mu::Vec3 sampleShapeDirection(const mu::Vec3& origin);
    void     updatePayload(Particle& p, float t) const;

    // pool_[0..activeCount_-1] 이 항상 활성 파티클 (compact array)
    std::vector<Particle> pool_ = std::vector<Particle>(kDefaultMaxParticles);
    int          activeCount_     = 0;
    int          overwriteCursor_ = 0;   // full-pool 덮어쓰기용 round-robin 커서
    int          maxParticles_    = kDefaultMaxParticles;
    std::mt19937 rng_{ std::random_device{}() };

    // Legacy continuous path
    EmitterConfig continuousConfig_{};
    float         emitAccum_  = 0.f;
    bool          continuous_ = false;

    // New continuous path (uses config_)
    ParticleSystemConfig config_;
    float                emitAccumNew_  = 0.f;
    bool                 continuousNew_ = false;
};

#endif  // __particleSystem_HPP
