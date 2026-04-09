#pragma once
#include "gfxUtil.hpp"

// Per-particle simulation data with semantic field names.
// The simulation core fills this each frame; the renderer backend reads it.
struct ParticleSemanticPayload {
    mu::Vec4 color;      // vertex color = startColor * colorOverLifetime(t)
    mu::Vec4 uvFrame;    // xy = sprite-sheet uvOffset, zw = uvScale  (primary UV)
    mu::Vec2 uv1;        // secondary UV (xy); zero-initialised if unused
    mu::Vec2 custom0;    // custom data slot 0
    mu::Vec2 custom1;    // custom data slot 1
};

// GPU-side layout that maps 1:1 to PerInstanceData in the particle shaders.
// Populated by the renderer backend via packPayload(); never touched by simulation.
struct ParticleGPUPayload {
    mu::Vec4 color;   // float4
    mu::Vec4 uv0;     // float4 — layout controlled by packing flags below
    mu::Vec4 uv1;     // float4
};

// Converts a semantic payload into the GPU layout.
//   packCustom0InUV0zw: place custom0 in uv0.zw instead of uvFrame.zw
//   packCustom1InUV1zw: place custom1 in uv1.zw (uv1.xy always carries uv1)
inline ParticleGPUPayload packPayload(
    const ParticleSemanticPayload& s,
    bool packCustom0InUV0zw,
    bool packCustom1InUV1zw) noexcept
{
    ParticleGPUPayload g;
    g.color = s.color;

    g.uv0 = packCustom0InUV0zw
        ? mu::Vec4{ s.uvFrame.x(), s.uvFrame.y(), s.custom0.x(), s.custom0.y() }
        : mu::Vec4{ s.uvFrame.x(), s.uvFrame.y(), s.uvFrame.z(), s.uvFrame.w() };

    g.uv1 = packCustom1InUV1zw
        ? mu::Vec4{ s.uv1.x(), s.uv1.y(), s.custom1.x(), s.custom1.y() }
        : mu::Vec4{ s.uv1.x(), s.uv1.y(), 0.f, 0.f };

    return g;
}

// One element in the draw-submission array passed to IParticleRenderer::flush().
struct ParticleDrawData {
    mu::Vec3   worldPos;
    float      size;
    float      rotation;     // billboard z-rotation (radians)
    mu::Mat4x4 worldMat;     // mesh renderer: full world matrix; billboard: ignored
    ParticleSemanticPayload payload;
    int        renderOrder;
    bool       additive;
};
