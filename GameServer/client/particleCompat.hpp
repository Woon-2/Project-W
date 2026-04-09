#pragma once
// Compatibility helpers: convert legacy flat configs to the module-based ParticleSystemConfig.
// Include this header in callers that still construct EmitterConfig / MeshEmitterConfig
// and want to use the new ParticleSystem::init() API without rewriting call sites.
// These functions will be removed once all callers have been migrated.

#include "particleSystem.hpp"      // EmitterConfig, ParticleSystemConfig (via particleModules.hpp)
#include "meshParticleSystem.hpp"  // MeshEmitterConfig

inline ParticleSystemConfig fromEmitterConfig(const EmitterConfig& c) {
    ParticleSystemConfig cfg;

    cfg.main.lifetimeMin        = c.lifetimeMin;
    cfg.main.lifetimeMax        = c.lifetimeMax;
    cfg.main.speedMin           = c.speedMin;
    cfg.main.speedMax           = c.speedMax;
    cfg.main.startColor         = c.startColor;
    cfg.main.startSizeMin       = c.sizeMultiplierMin;
    cfg.main.startSizeMax       = c.sizeMultiplierMax;
    cfg.main.startRotationMin   = c.startRotationMin;
    cfg.main.startRotationMax   = c.startRotationMax;
    cfg.main.gravityModifierMin = c.gravityModifierMin;
    cfg.main.gravityModifierMax = c.gravityModifierMax;
    cfg.main.gravity            = c.gravity;
    cfg.main.drag               = c.drag;

    cfg.emission.emitRate = c.emitRate;

    // Shape: legacy EmitterShape::Point + spread==0 -> Point
    //        legacy EmitterShape::Point + spread>0  -> Cone
    //        legacy EmitterShape::Edge               -> Edge
    cfg.shape.position  = c.position;
    cfg.shape.direction = c.direction;
    if (c.shape == EmitterShape::Edge) {
        cfg.shape.type      = ShapeModule::Type::Edge;
        cfg.shape.edgeLength = c.edgeLength;
        cfg.shape.edgeDir    = c.edgeDir;
    } else if (c.spread > 0.f) {
        cfg.shape.type      = ShapeModule::Type::Cone;
        cfg.shape.coneAngle = c.spread;
    } else {
        cfg.shape.type = ShapeModule::Type::Point;
    }

    cfg.colorOverLifetime.enabled  = true;
    cfg.colorOverLifetime.gradient = c.colorOverLifetime;

    cfg.sizeOverLifetime.enabled   = true;
    cfg.sizeOverLifetime.sizeBegin = c.sizeBegin;
    cfg.sizeOverLifetime.sizeEnd   = c.sizeEnd;

    cfg.renderer.mode     = RendererModule::Mode::Billboard;
    cfg.renderer.pClip    = c.pClip;
    cfg.renderer.renderOrder = c.renderOrder;
    cfg.renderer.material.blendMode = c.additiveBlend
        ? MaterialDescriptor::BlendMode::Additive
        : MaterialDescriptor::BlendMode::Alpha;
    cfg.renderer.material.mainTex = nullptr;  // billboard path uses pClip, not mainTex

    return cfg;
}

inline ParticleSystemConfig fromMeshEmitterConfig(const MeshEmitterConfig& c) {
    ParticleSystemConfig cfg;

    cfg.main.lifetimeMin  = c.lifetimeMin;
    cfg.main.lifetimeMax  = c.lifetimeMax;
    cfg.main.startColor   = c.startColor;
    // MeshEmitterConfig has no speed / gravity / drag fields — defaults are fine

    cfg.emission.emitRate = c.emitRate;

    cfg.shape.type     = ShapeModule::Type::Point;
    cfg.shape.position = c.position;

    cfg.colorOverLifetime.enabled  = true;
    cfg.colorOverLifetime.gradient = c.colorOverLifetime;

    cfg.sizeOverLifetime.enabled   = true;
    cfg.sizeOverLifetime.sizeBegin = c.sizeBegin;
    cfg.sizeOverLifetime.sizeEnd   = c.sizeEnd;

    cfg.renderer.mode             = RendererModule::Mode::Mesh;
    cfg.renderer.pMesh            = c.pMesh;
    cfg.renderer.pSubMesh         = c.pSubMesh;
    cfg.renderer.renderOrder      = c.renderOrder;
    cfg.renderer.angularVelocityMin = c.angularVelocityMin;
    cfg.renderer.angularVelocityMax = c.angularVelocityMax;
    cfg.renderer.material.mainTex   = c.pTex;
    // NOTE: c.rotation (base orientation Mat4x4) is not migrated here.
    //       Phase 2 will add baseRotation to RendererModule and wire it up.

    return cfg;
}
