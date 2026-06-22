// Lightweight diffuse-only terrain pass for the minimap background cache.
// No normal map, lighting, or shadow — just splat-blended albedo, with a fixed
// alpha=1 output that marks this pixel as "covered by a loaded chunk" (the
// fog-of-war mask consumed by MinimapFogBlurPipeline).

#define MAX_TERRAIN_LAYERS 4

#include "bindless.hlsli"

cbuffer PerDrawcallData : register(b0) {
    float4x4 wvp;

    int4   idxSplatMap;
    int4   idxDiffuse[MAX_TERRAIN_LAYERS];
    float4 tiling[MAX_TERRAIN_LAYERS];   // xy = tileSize, zw = tileOffset
    int    layerCount;
    float3 _pdd0;
};

struct VSOutput {
    float4 posH : SV_Position;
    float2 uv   : UV;
};

VSOutput VSMain(float3 position : POSITION, float2 uv : UV) {
    VSOutput o;
    o.posH = mul(float4(position, 1.f), wvp);
    o.uv   = uv;
    return o;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    float4 splatWeights = sampleBindless(idxSplatMap, input.uv);
    float totalWeight = splatWeights.r + splatWeights.g + splatWeights.b + splatWeights.a;
    if (totalWeight > 0.0001f)
        splatWeights /= totalWeight;
    else
        splatWeights = float4(1.f, 0.f, 0.f, 0.f);

    float weights[4];
    weights[0] = splatWeights.r;
    weights[1] = splatWeights.g;
    weights[2] = splatWeights.b;
    weights[3] = splatWeights.a;

    float3 albedo = float3(0.f, 0.f, 0.f);
    for (int i = 0; i < MAX_TERRAIN_LAYERS; ++i) {
        if (i >= layerCount) break;
        float w = weights[i];
        if (w < 0.0001f) continue;

        float2 layerUV = input.uv * tiling[i].xy + tiling[i].zw;
        float4 diffuseSample = sampleBindless(idxDiffuse[i], layerUV);
        albedo += diffuseSample.rgb * w;
    }

    // Fixed alpha=1: this pixel belongs to a loaded chunk (fog-of-war coverage mask).
    return float4(albedo, 1.f);
}
