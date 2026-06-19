// Tonemap resolve pass.
// Reads the HDR scene-color render target (R16G16B16A16_FLOAT) via a bindless
// SRV index and writes the LDR backbuffer (R8G8B8A8_UNORM).
// The tonemap curve here is intentionally identical to the one previously baked
// into pbrDeferredLighting.hlsl (Reinhard + 2.2 gamma), so moving the curve from
// the lighting pass to this resolve pass is a no-op on final image appearance.

#include "bindless.hlsli"

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv  : UV;
};

// PerDrawcallData (b0). Matches TonemapResolveShader::PerDrawcallData in shader.hpp.
// idxSceneColor / idxBloom are bindless indices (idxRange, idxResource, idxInArray, idxSampler).
cbuffer PerDrawcallData : register(b0) {
    int4  idxSceneColor;
    int4  idxBloom;
    float exposure;
    float bloomIntensity;
    uint  debugMode;
    float _pad;
    int4  idxColorGradingLUT;     // 3D LUT bindless index; idxColorGradingLUT.x < 0 => no-op
}

// Fullscreen triangle — no vertex buffer needed.
// Covers the entire screen with 3 vertices driven by SV_VertexID.
// (Same scheme as pbrDeferredLighting.hlsl::VSMain.)
VSOutput VSMain(uint vertexID : SV_VertexID) {
    // vertexID 0 -> uv(0,0), 1 -> uv(2,0), 2 -> uv(0,2)
    float2 uv  = float2((vertexID << 1) & 2, vertexID & 2);
    // NDC: uv(0,0) -> (-1,+1), uv(2,0) -> (+3,+1), uv(0,2) -> (-1,-3)
    float4 pos = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);

    VSOutput ret;
    ret.pos = pos;
    ret.uv  = uv;
    return ret;
}

// ACES Filmic tone mapping curve (Narkowicz 2015 approximation).
// Retains highlight saturation far better than per-channel Reinhard.
float3 acesFilmic(float3 x) {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PSMain(VSOutput input) : SV_TARGET {
    float3 color = sampleBindless(idxSceneColor, input.uv).rgb;

    // GBuffer debug views are already display-encoded by the lighting pass.
    // Pass them through untouched so they are not double tonemapped / gamma'd.
    if (debugMode != 0u) {
        return float4(color, 1.0f);
    }

    // Additive bloom. sampleBindless returns 0 for an invalid index, so this is a
    // no-op until the bloom pass is wired (idxBloom valid, bloomIntensity > 0).
    color += sampleBindless(idxBloom, input.uv).rgb * bloomIntensity;

    // Exposure -> ACES Filmic -> gamma.
    color *= exposure;
    color = acesFilmic(color);
    color = pow(abs(color), 1.0f / 2.2f);

    // LDR color grading via 3D LUT (gamma-corrected sRGB space, so external .cube
    // LUTs authored against an LDR signal line up without extra conversion).
    if (idxColorGradingLUT.x >= 0) {
        color = sampleBindless3D(idxColorGradingLUT, color);
    }

    return float4(color, 1.0f);
}
