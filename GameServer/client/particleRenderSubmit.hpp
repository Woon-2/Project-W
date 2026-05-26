#ifndef __particleRenderSubmit_HPP
#define __particleRenderSubmit_HPP

#include "particleModules.hpp"

class GFX;
struct Particle;

struct ParticleRenderFrameState {
    bool swordSlashFrameDataSubmitted = false;
    bool smokeBlendCGFrameDataSubmitted = false;
    bool blendCGMeshFrameDataSubmitted = false;
    bool twoSidesFrameDataSubmitted = false;
    bool piercingFrameDataSubmitted = false;
};

struct ParticleRenderContext {
    const ps::RendererModule& renderer;
    const ps::TextureSheetAnimationModule& textureSheetAnimation;
    const Particle& particle;
    ParticleRenderFrameState& frameState;
    float systemTime = 0.f;
    float t = 0.f;
    float size = 1.f;
    mu::Vec3 size3D = { 1.f, 1.f, 1.f };
    mu::Vec4 tint = { 1.f, 1.f, 1.f, 1.f };
    mu::Vec2 custom1 = { 0.f, 0.f };
    mu::Vec2 custom2 = { 0.f, 0.f };
    bool customDataEnabled = false;
};

void submitParticleDraw(GFX& gfx, const ParticleRenderContext& ctx, const ps::MatUnlit& mat);
void submitParticleDraw(GFX& gfx, const ParticleRenderContext& ctx, const ps::MatSwordSlash& mat);
void submitParticleDraw(GFX& gfx, const ParticleRenderContext& ctx, const ps::MatSmokeBlendCG& mat);
void submitParticleDraw(GFX& gfx, const ParticleRenderContext& ctx, const ps::MatTwoSides& mat);
void submitParticleDraw(GFX& gfx, const ParticleRenderContext& ctx, const ps::MatWindRing& mat);
void submitParticleDraw(GFX& gfx, const ParticleRenderContext& ctx, const ps::MatPiercing& mat);

// Submits a TrailPipeline draw call for the single particle in `ctx`.
// Independent of the renderer mode — coexists with the primary submitParticleDraw.
// No-op when the trail module is disabled, the particle has no trail, or there
// are fewer than 2 captured vertices.
void submitParticleTrail(GFX& gfx, const ParticleRenderContext& ctx, const ps::TrailModule& trail);

#endif  // __particleRenderSubmit_HPP
