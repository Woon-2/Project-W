#ifndef __meshParticleSystem_HPP
#define __meshParticleSystem_HPP

#include "gfxUtil.hpp"

#include <array>
#include <random>

struct Mesh;
struct SubMesh;
class GFX;

struct MeshEmitterConfig {
    mu::Vec3       position;
    mu::Mat4x4     rotation;        // fixed orientation applied to every emitted particle
    float          sizeBegin        = 1.f;
    float          sizeEnd          = 0.f;
    float          lifetimeMin      = 0.3f;
    float          lifetimeMax      = 0.5f;
    mu::Vec4       startColor       = { 1.f, 1.f, 1.f, 1.f };
    ColorGradient  colorOverLifetime = ColorGradient::constant({ 1.f, 1.f, 1.f, 1.f });
    float          angularVelocityMin = 0.f;  // radians/sec
    float          angularVelocityMax = 0.f;  // radians/sec
    const Mesh*    pMesh            = nullptr;
    const SubMesh* pSubMesh         = nullptr;
    const Texture* pTex             = nullptr;
    int            renderOrder      = 0;
    float          emitRate         = 0.f;  // particles/sec (0 = manual emit only)
};

struct MeshParticle {
    mu::Vec3    pos;
    float       lifetime, maxLifetime;
    mu::Vec4    startColor;
    ColorGradient colorOverLifetime;
    float       sizeBegin, sizeEnd;
    mu::Mat4x4  rotation;
    float       angularVelocity = 0.f;  // radians/sec, set at spawn
    float       angle           = 0.f;  // accumulated rotation (radians)
    const Mesh*    pMesh    = nullptr;
    const SubMesh* pSubMesh = nullptr;
    const Texture* pTex     = nullptr;
    int         renderOrder = 0;
    // no 'active' field: compact array pool — pool_[0..activeCount_-1] always active
};

class MeshParticleSystem {
public:
    static constexpr int kMaxParticles = 256;

    void emit(const MeshEmitterConfig& config, int count);
    void update(Seconds dt);
    void render(GFX& gfx) const;

    void startContinuous(const MeshEmitterConfig& config);
    void stopContinuous();

    int activeCount() const { return activeCount_; }

private:
    float randomFloat(float lo, float hi);

    std::array<MeshParticle, kMaxParticles> pool_{};
    int           activeCount_     = 0;
    int           overwriteCursor_ = 0;
    std::mt19937  rng_{ std::random_device{}() };

    MeshEmitterConfig continuousConfig_{};
    float             emitAccum_  = 0.f;
    bool              continuous_ = false;
};

#endif  // __meshParticleSystem_HPP
