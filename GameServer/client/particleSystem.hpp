#ifndef __particleSystem_HPP
#define __particleSystem_HPP

#include "spriteAnimation.hpp"

#include <array>
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
    mu::Vec3                   tintBegin   = {1.f, 1.f, 1.f};
    mu::Vec3                   tintEnd     = {1.f, 1.f, 1.f};
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
    float                      emitRate     = 0.f;       // particles/sec (0이면 수동 emit)
};

class GFX;

struct Particle {
    mu::Vec3        pos, vel;
    float           lifetime, maxLifetime;
    mu::Vec3        tintBegin, tintEnd;
    float           sizeBegin, sizeEnd, drag;
    mu::Vec3        gravity;
    SpriteAnimation anim;
    float           rotation;
    bool            additive;
    // 'active' 필드 없음: pool_[0..activeCount_-1] 이 항상 활성 상태
};

class ParticleSystem {
public:
    static constexpr int kMaxParticles = 4096;

    void emit(const EmitterConfig& config, int count);
    void update(Seconds dt);
    void render(GFX& gfx) const;

    void startContinuous(const EmitterConfig& config);
    void stopContinuous();

    int activeCount() const { return activeCount_; }

private:
    float randomFloat(float lo, float hi);

    // pool_[0..activeCount_-1] 이 항상 활성 파티클 (compact array)
    std::array<Particle, kMaxParticles> pool_{};
    int           activeCount_     = 0;
    int           overwriteCursor_ = 0;   // full-pool 덮어쓰기용 round-robin 커서
    std::mt19937  rng_{ std::random_device{}() };

    EmitterConfig continuousConfig_{};
    float         emitAccum_  = 0.f;
    bool          continuous_ = false;
};

#endif  // __particleSystem_HPP
