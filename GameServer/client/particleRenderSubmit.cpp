#include "pch.hpp"
#include "particleRenderSubmit.hpp"
#include "particleRenderMode.hpp"
#include "particleSystem.hpp"
#include "blendCGMeshPipeline.hpp"
#include "twoSidesPipeline.hpp"
#include "trailPipeline.hpp"
#include "gfx.hpp"

void submitParticleDraw(GFX& gfx, const ParticleRenderContext& ctx, const ps::MatUnlit& mat) {
    if (!mat.mainTex) return;

    const auto& rend = ctx.renderer;
    const auto uv = calcParticleTextureSheetUV(ctx.textureSheetAnimation, ctx.t);
    const mu::Vec4 tint = {
        ctx.tint.x() * mat.color.x(),
        ctx.tint.y() * mat.color.y(),
        ctx.tint.z() * mat.color.z(),
        ctx.tint.w() * mat.color.w()
    };

    if (const auto geometry = buildParticleBillboardGeometry(ctx)) {
        gfx.addDrawEvent(BillboardPipeline::DrawEvent{
            .world = geometry->world,
            .pTex = mat.mainTex,
            .uvOffset = uv.offset,
            .uvScale = uv.scale,
            .tint = tint,
            .blend = mat.blend,
            .rotation = geometry->rotation,
            .rotation3D = geometry->rotation3D,
            .stretchAxisAndMode = geometry->stretchAxisAndMode,
            .alignment = rend.alignment,
            .renderOrder = rend.renderOrder,
            .sortMode = rend.sortMode,
            .sortingFudge = rend.sortingFudge,
            .sortPos = ctx.particle.pos,
        });
    } else if (const auto geometry = buildParticleMeshGeometry(ctx)) {
        gfx.addDrawEvent(MeshParticlePipeline::DrawEvent{
            .world = geometry->world,
            .pMesh = geometry->pMesh,
            .pSubMesh = geometry->pSubMesh,
            .pTex = mat.mainTex,
            .tint = tint,
            .renderOrder = rend.renderOrder,
        });
    }
}

void submitParticleDraw(GFX& gfx, const ParticleRenderContext& ctx, const ps::MatSwordSlash& mat) {
    const auto& rend = ctx.renderer;
    if (!mat.mainTex || !mat.emissionTex || !mat.dissolveTex) return;
    const auto geometry = buildParticleMeshGeometry(ctx);
    if (!geometry) return;

    if (!ctx.frameState.swordSlashFrameDataSubmitted) {
        gfx.addFrameData(SwordSlashPipeline::FrameData{ .time = ctx.systemTime });
        ctx.frameState.swordSlashFrameDataSubmitted = true;
    }

    gfx.addDrawEvent(SwordSlashPipeline::DrawEvent{
        .world = geometry->world,
        .tint = ctx.tint,
        .t = ctx.t,
        .custom1 = ctx.custom1,
        .custom2 = ctx.custom2,
        .customDataEnabled = ctx.customDataEnabled,
        .renderOrder = rend.renderOrder,
        .pMesh = geometry->pMesh,
        .pSubMesh = geometry->pSubMesh,
        .pMainTex = mat.mainTex,
        .pEmissionTex = mat.emissionTex,
        .pDissolveTex = mat.dissolveTex,
        .pFlowTex = mat.flowTex,
        .speedMainTexUV = mat.speedMainTexUV,
        .speedDissolveUV = mat.speedDissolveUV,
        .speedFlow = mat.speedFlow,
        .mainTexST = mat.mainTexST,
        .emissionTexST = mat.emissionTexST,
        .dissolveTexST = mat.dissolveTexST,
        .flowTexST = mat.flowTexST,
        .flowPower = mat.flowPower,
        .emission = mat.emission,
        .desaturation = mat.desaturation,
        .remap = mat.remap,
        .addColor = mat.addColor,
        .opacity = mat.opacity,
        .useSmoothDissolve = mat.useSmoothDissolve,
    });
}

void submitParticleDraw(GFX& gfx, const ParticleRenderContext& ctx, const ps::MatSmokeBlendCG& mat) {
    const auto& rend = ctx.renderer;
    if (!mat.mainTex) return;

    if (const auto geometry = buildParticleBillboardGeometry(ctx)) {
        if (!ctx.frameState.smokeBlendCGFrameDataSubmitted) {
            gfx.addFrameData(SmokeBlendCGPipeline::FrameData{ .time = ctx.systemTime });
            ctx.frameState.smokeBlendCGFrameDataSubmitted = true;
        }

        const auto uv = calcParticleTextureSheetUV(ctx.textureSheetAnimation, ctx.t);
        gfx.addDrawEvent(SmokeBlendCGPipeline::DrawEvent{
            .world = geometry->world,
            .tint = ctx.tint,
            .rotation = geometry->rotation,
            .rotation3D = geometry->rotation3D,
            .stretchAxisAndMode = geometry->stretchAxisAndMode,
            .renderOrder = rend.renderOrder,
            .pMainTex = mat.mainTex,
            .pNoiseTex = mat.noiseTex,
            .pFlowTex = mat.flowTex,
            .pMaskTex = mat.maskTex,
            .uvOffset = uv.offset,
            .uvScale = uv.scale,
            .mainTexST = mat.mainTexST,
            .noiseTexST = mat.noiseTexST,
            .flowTexST = mat.flowTexST,
            .maskTexST = mat.maskTexST,
            .speedMainTexUVNoiseZW = mat.speedMainTexUVNoiseZW,
            .distortionSpeedXYPowerZ = mat.distortionSpeedXYPowerZ,
            .color = mat.color,
            .emission = mat.emission,
            .opacity = mat.opacity,
            .textureOpacity = mat.textureOpacity,
            .multiplyTexture = mat.multiplyTexture,
            .useOnlyColor = mat.useOnlyColor,
            .useFresnel = mat.useFresnel,
            .fresnelPower = mat.fresnelPower,
            .fresnelScale = mat.fresnelScale,
            .useCenterGlow = mat.useCenterGlow,
            .useDepth = mat.useDepth,
            .depthPower = mat.depthPower,
        });
    } else if (const auto geometry = buildParticleMeshGeometry(ctx)) {
        if (!ctx.frameState.blendCGMeshFrameDataSubmitted) {
            gfx.addFrameData(BlendCGMeshPipeline::FrameData{ .time = ctx.systemTime });
            ctx.frameState.blendCGMeshFrameDataSubmitted = true;
        }

        gfx.addDrawEvent(BlendCGMeshPipeline::DrawEvent{
            .world = geometry->world,
            .tint = ctx.tint,
            .t = ctx.t,
            .custom1 = ctx.custom1,
            .custom2 = ctx.custom2,
            .customDataEnabled = ctx.customDataEnabled,
            .renderOrder = rend.renderOrder,
            .pMesh = geometry->pMesh,
            .pSubMesh = geometry->pSubMesh,
            .pMainTex = mat.mainTex,
            .pNoiseTex = mat.noiseTex,
            .pFlowTex = mat.flowTex,
            .pMaskTex = mat.maskTex,
            .mainTexST = mat.mainTexST,
            .noiseTexST = mat.noiseTexST,
            .flowTexST = mat.flowTexST,
            .maskTexST = mat.maskTexST,
            .speedMainTexUVNoiseZW = mat.speedMainTexUVNoiseZW,
            .distortionSpeedXYPowerZ = mat.distortionSpeedXYPowerZ,
            .color = mat.color,
            .emission = mat.emission,
            .opacity = mat.opacity,
            .textureOpacity = mat.textureOpacity,
            .multiplyTexture = mat.multiplyTexture,
            .useOnlyColor = mat.useOnlyColor,
            .useFresnel = mat.useFresnel,
            .fresnelPower = mat.fresnelPower,
            .fresnelScale = mat.fresnelScale,
            .useCenterGlow = mat.useCenterGlow,
            .useDepth = mat.useDepth,
            .depthPower = mat.depthPower,
        });
    }
}

void submitParticleDraw(GFX& gfx, const ParticleRenderContext& ctx, const ps::MatTwoSides& mat) {
    const auto& rend = ctx.renderer;
    if (!mat.mainTex) return;
    const auto geometry = buildParticleMeshGeometry(ctx);
    if (!geometry) return;

    if (!ctx.frameState.twoSidesFrameDataSubmitted) {
        gfx.addFrameData(TwoSidesPipeline::FrameData{ .time = ctx.systemTime });
        ctx.frameState.twoSidesFrameDataSubmitted = true;
    }

    gfx.addDrawEvent(TwoSidesPipeline::DrawEvent{
        .world                = geometry->world,
        .tint                 = ctx.tint,
        .t                    = ctx.t,
        .custom1              = ctx.custom1,
        .custom2              = ctx.custom2,
        .customDataEnabled    = ctx.customDataEnabled,
        .renderOrder          = rend.renderOrder,
        .pMesh                = geometry->pMesh,
        .pSubMesh             = geometry->pSubMesh,
        .pMainTex             = mat.mainTex,
        .pMaskTex             = mat.maskTex,
        .pNoiseTex            = mat.noiseTex,
        .mainTexST            = mat.mainTexST,
        .maskTexST            = mat.maskTexST,
        .noiseTexST           = mat.noiseTexST,
        .texSpeed             = mat.texSpeed,
        .emission             = mat.emission,
        .opacity              = mat.opacity,
        .useFresnel           = mat.useFresnel,
        .fresnelPower         = mat.fresnelPower,
        .frontFacesColor      = mat.frontFacesColor,
        .backFacesColor       = mat.backFacesColor,
        .fresnelColor         = mat.fresnelColor,
        .fresnelEmission      = mat.fresnelEmission,
        .useBackFresnel       = mat.useBackFresnel,
        .backFresnel          = mat.backFresnel,
        .backFresnelEmission  = mat.backFresnelEmission,
        .backFresnelColor     = mat.backFresnelColor,
    });
}

void submitParticleTrail(GFX& gfx, const ParticleRenderContext& ctx, const ps::TrailModule& trail) {
    if (!trail.enabled) return;
    if (trail.mode != ps::TrailModule::Mode::Particles) return;

    const Particle& p = ctx.particle;
    if (!p.trailEnabled) return;
    if (p.trailCount < 2) return;          // need at least one segment
    if (!trail.material.mainTex) return;   // no texture -> nothing to sample

    constexpr int kCap = Particle::kMaxTrailSegments;

    // Walk the ring buffer from oldest (tail) to newest (head-1).
    // Build TrailVertexCPU array with age and cumulative arc length.
    std::vector<TrailPipeline::TrailVertexCPU> vertices;
    vertices.reserve(p.trailCount);

    float accumDist = 0.f;
    mu::Vec3 prevPos{};

    for (int i = 0; i < p.trailCount; ++i) {
        const int idx = (p.trailHead - p.trailCount + i + kCap) % kCap;
        const TrailPoint& tp = p.trail[idx];

        if (i > 0) {
            const mu::Vec3 d = tp.pos - prevPos;
            accumDist += std::sqrt(mu::dot(d, d));
        }
        prevPos = tp.pos;

        TrailPipeline::TrailVertexCPU v{};
        v.pos            = tp.pos;
        v.age            = std::max(0.f, ctx.systemTime - tp.spawnTime);
        v.cumulativeDist = accumDist;
        vertices.push_back(v);
    }

    const mu::Vec4 baseColor = trail.inheritParticleColor ? ctx.tint
                                                          : mu::Vec4{ 1.f, 1.f, 1.f, 1.f };
    const float widthMult = trail.widthMultiplier * (trail.sizeAffectsWidth ? ctx.size : 1.f);
    const u32t textureMode = (trail.textureMode == ps::TrailModule::TextureMode::Tile) ? 1u : 0u;

    gfx.addDrawEvent(TrailPipeline::DrawEvent{
        .vertices             = std::move(vertices),
        .localToWorld         = mu::Mat4x4{},  // identity; Particle::pos already world-space
        .baseColor            = baseColor,
        .widthStart           = trail.widthOverTrailStart,
        .widthEnd             = trail.widthOverTrailEnd,
        .widthMultiplier      = widthMult,
        .tileLength           = trail.tileLength,
        .textureMode          = textureMode,
        .inheritParticleColor = trail.inheritParticleColor ? 1u : 0u,
        .trailLifetime        = p.trailLifetime,
        .currentSystemTime    = ctx.systemTime,
        .pMainTex             = trail.material.mainTex,
        .additive             = trail.material.blend == ps::BlendMode::Additive,
        .renderOrder          = ctx.renderer.renderOrder,
    });
}
