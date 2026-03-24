#ifndef __particleSystem_HPP
#define __particleSystem_HPP

#include "spriteAnimation.hpp"

#include <array>
#include <random>

struct EmitterConfig {
    mu::Vec3                   position;
    mu::Vec3                   direction   = {0, 1, 0};  // world-space emit axis
    float                      spread      = 0.3f;       // cone half-angle (radians)
    float                      speedMin    = 1.f;
    float                      speedMax    = 3.f;
    float                      lifetimeMin = 0.5f;
    float                      lifetimeMax = 1.5f;
    mu::Vec3                   tintBegin   = {1.f, 1.f, 1.f};
    mu::Vec3                   tintEnd     = {1.f, 1.f, 1.f};
    float                      sizeBegin   = 1.f;
    float                      sizeEnd     = 0.f;
    float                      drag        = 0.f;
    mu::Vec3                   gravity     = {0.f, -9.8f, 0.f};
    float                      startRotationMin = 0.f;   // radians
    float                      startRotationMax = 0.f;
    const SpriteAnimationClip* pClip       = nullptr;
    bool                       additiveBlend = true;
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
    bool            active;
};

class ParticleSystem {
public:
    static constexpr int kMaxParticles = 4096;

    void emit(const EmitterConfig& config, int count);
    void update(Seconds dt);
    void render(GFX& gfx);

private:
    std::array<Particle, kMaxParticles> pool_{};
    int          cursor_ = 0;
    std::mt19937 rng_{ std::random_device{}() };
};

#endif  // __particleSystem_HPP
