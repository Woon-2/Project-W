#include "pch.hpp"
#include "particleRenderSubmit.hpp"
#include "particleRenderMode.hpp"
#include "blendCGMeshPipeline.hpp"
#include "twoSidesPipeline.hpp"
#include "gfx.hpp"

void submitParticleDraw(GFX& gfx, const ParticleRenderContext& ctx, const ps::MatUnlit& mat) {
    if (!mat.mainTex) return;

    const auto& rend = ctx.renderer;
    const auto uv = calcParticleTextureSheetUV(ctx.textureSheetAnimation, ctx.t);

    if (const auto geometry = buildParticleBillboardGeometry(ctx)) {
        gfx.addDrawEvent(BillboardPipeline::DrawEvent{
            .world = geometry->world,
            .pTex = mat.mainTex,
            .uvOffset = uv.offset,
            .uvScale = uv.scale,
            .tint = ctx.tint,
            .additive = mat.additive,
            .rotation = geometry->rotation,
            .stretchAxisAndMode = geometry->stretchAxisAndMode,
            .renderOrder = rend.renderOrder,
        });
    } else if (const auto geometry = buildParticleMeshGeometry(ctx)) {
        gfx.addDrawEvent(MeshParticlePipeline::DrawEvent{
            .world = geometry->world,
            .pMesh = geometry->pMesh,
            .pSubMesh = geometry->pSubMesh,
            .pTex = mat.mainTex,
            .tint = ctx.tint,
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
    if (!mat.mainTex || !mat.maskTex) return;
    const auto geometry = buildParticleMeshGeometry(ctx);
    if (!geometry) return;

    if (!ctx.frameState.twoSidesFrameDataSubmitted) {
        gfx.addFrameData(TwoSidesPipeline::FrameData{ .time = ctx.systemTime });
        ctx.frameState.twoSidesFrameDataSubmitted = true;
    }

    gfx.addDrawEvent(TwoSidesPipeline::DrawEvent{
        .world           = geometry->world,
        .tint            = ctx.tint,
        .t               = ctx.t,
        .custom1         = ctx.custom1,
        .custom2         = ctx.custom2,
        .customDataEnabled = ctx.customDataEnabled,
        .renderOrder     = rend.renderOrder,
        .pMesh           = geometry->pMesh,
        .pSubMesh        = geometry->pSubMesh,
        .pMainTex        = mat.mainTex,
        .pMaskTex        = mat.maskTex,
        .pNoiseTex       = mat.noiseTex,
        .mainTexST       = mat.mainTexST,
        .maskTexST       = mat.maskTexST,
        .noiseTexST      = mat.noiseTexST,
        .noiseSpeed      = mat.noiseSpeed,
        .emission        = mat.emission,
        .opacity         = mat.opacity,
        .useBackFresnel  = mat.useBackFresnel,
        .backFresnel     = mat.backFresnel,
    });
}
