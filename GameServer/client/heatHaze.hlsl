// Heat-haze glow pass. Additively writes a tinted, depth-gated glow around each
// boss into SceneColorHDR (R16G16B16A16_FLOAT) BEFORE bloom, so the tint blooms.
// Fullscreen triangle (no vertex buffer); the field is driven entirely by the
// HeatSource array in the b0 constant buffer. The companion refraction warp lives
// in tonemapResolve.hlsl (both share heatField.hlsli).

#include "bindless.hlsli"
#include "heatField.hlsli"

// Matches HeatDistortionShader::PerDrawcallData in shader.hpp.
cbuffer PerDrawcallData : register(b0) {
    HeatSource gHeatSources[HEAT_MAX_SOURCES];
    uint  gHeatCount;
    float gHeatTime;
    float gHeatWarp;
    float gHeatGlow;
    int4  idxGB4;          // linear view-space Z SRV (R32_FLOAT)
}

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv  : UV;
};

VSOutput VSMain(uint vertexID : SV_VertexID) {
    float2 uv  = float2((vertexID << 1) & 2, vertexID & 2);
    float4 pos = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);
    VSOutput o;
    o.pos = pos;
    o.uv  = uv;
    return o;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    float pixelZ = sampleBindless(idxGB4, input.uv).r;

    float2 warp;
    float3 glow;
    evalHeatField(input.uv, pixelZ, gHeatSources, gHeatCount,
                  gHeatTime, gHeatWarp, gHeatGlow, warp, glow);

    // Keep the bloom chain NaN/Inf-safe (see bloom.hlsl srcTap rationale).
    glow = max(glow, 0.0f);
    glow = min(glow, 64000.0f);
    glow = select(isnan(glow), float3(0.0f, 0.0f, 0.0f), glow);

    return float4(glow, 1.0f);   // additive (ONE/ONE) blend configured in the PSO
}
