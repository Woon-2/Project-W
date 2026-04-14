#define MAX_TERRAIN_LAYERS 4

cbuffer PerDrawcallData : register(b0) {
    float4x4 wvp;
    float4x4 world;
    float4x4 wv;

    int4   idxSplatMap;
    int4   idxDiffuse[MAX_TERRAIN_LAYERS];
    int4   idxNormal [MAX_TERRAIN_LAYERS];
    float4 tiling            [MAX_TERRAIN_LAYERS]; // xy = tileSize, zw = tileOffset
    float4 metallicRoughness [MAX_TERRAIN_LAYERS]; // x = metallic, y = roughness
    int    layerCount;
    int    hasAnyNormal;
    float2 _pdd0;
};

cbuffer PerFrameData : register(b1) {
    float3 globalAmbient;
    float  _pfd0;
};

#include "bindless.hlsli"

// Oct encoding: unit normal (view-space) -> float2 [0,1]
float2 octEncode(float3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    if (n.z < 0.0f) n.xy = (1.0f - abs(n.yx)) * sign(n.xy);
    return n.xy * 0.5f + 0.5f;
}

// ---------------------------------------------------------------------------
// Vertex shader
// ---------------------------------------------------------------------------

struct VSOutput {
    float4 posH       : SV_Position;
    float3 normalV    : NORMAL_V;
    float3 tangentV   : TANGENT_V;
    float3 bitangentV : BITANGENT_V;
    float2 uv         : UV;
};

VSOutput VSMain(
    float3 position  : POSITION,
    float3 normal    : NORMAL,
    float3 tangent   : TANGENT,
    float3 bitangent : BITANGENT,
    float2 uv        : UV
) {
    VSOutput ret;
    ret.posH    = mul(float4(position, 1.f), wvp);
    ret.normalV = normalize(mul(normal, (float3x3)wv));
    if (hasAnyNormal) {
        ret.tangentV   = normalize(mul(tangent,   (float3x3)wv));
        ret.bitangentV = normalize(mul(bitangent, (float3x3)wv));
    }
    ret.uv = uv;
    return ret;
}

// ---------------------------------------------------------------------------
// GBuffer output: matches pbrDeferred.hlsl GBufferOutput layout
// ---------------------------------------------------------------------------

struct GBufferOutput {
    float4 gb0 : SV_TARGET0; // Albedo.rgb + AO.a        | R8G8B8A8_UNORM
    float2 gb1 : SV_TARGET1; // NormalV oct-encoded       | R16G16_FLOAT
    float4 gb2 : SV_TARGET2; // LightAccum.rgb + Roughness.a | R8G8B8A8_UNORM
    float  gb3 : SV_TARGET3; // Metallic                  | R8_UNORM
};

// ---------------------------------------------------------------------------
// Pixel shader
// ---------------------------------------------------------------------------

GBufferOutput PSMain(VSOutput input) {
    // 1. Sample and normalize splat weights.
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

    // 2. Blend albedo and tangent-space normals across layers.
    float3 albedo          = float3(0.f, 0.f, 0.f);
    float3 blendedNormalTS = float3(0.f, 0.f, 0.f);

    [unroll]
    for (int i = 0; i < MAX_TERRAIN_LAYERS; ++i) {
        if (i >= layerCount) break;
        float w = weights[i];
        if (w < 0.0001f) continue;

        float2 layerUV = input.uv * tiling[i].xy + tiling[i].zw;

        // Diffuse: sample and convert sRGB -> linear before blending.
        float4 diffuseSample = sampleBindless(idxDiffuse[i], layerUV);
        albedo += pow(abs(diffuseSample.rgb), 2.2f) * w;

        // Normal map (Unity DXT5nm: X stored in Alpha, Y in Green).
        if (idxNormal[i].x >= 0) {
            float4 nmSample = sampleBindless(idxNormal[i], layerUV);
            float3 nTS;
            nTS.xy = nmSample.ag * 2.f - 1.f;
            nTS.z  = sqrt(max(0.f, 1.f - dot(nTS.xy, nTS.xy)));
            blendedNormalTS += nTS * w;
        } else {
            blendedNormalTS += float3(0.f, 0.f, 1.f) * w;
        }
    }

    // 3. Reconstruct view-space shading normal from blended tangent-space normal.
    float3 vertNormalV = normalize(input.normalV);
    float3 shadingNormalV;
    if (hasAnyNormal) {
        float3   tangentV   = normalize(input.tangentV);
        float3   bitangentV = normalize(input.bitangentV);
        float3x3 tbn        = float3x3(tangentV, bitangentV, vertNormalV);
        shadingNormalV = normalize(mul(blendedNormalTS, tbn));
    } else {
        shadingNormalV = vertNormalV;
    }

    // 4. Blend per-layer metallic/roughness by splat weights.
    float roughness = 0.f;
    float metallic  = 0.f;
    [unroll]
    for (int mi = 0; mi < MAX_TERRAIN_LAYERS; ++mi) {
        if (mi >= layerCount) break;
        roughness += metallicRoughness[mi].y * weights[mi];
        metallic  += metallicRoughness[mi].x * weights[mi];
    }

    // 5. Pack GBuffer.
    // Terrain has no AO map and no emissive; lightAccum = ambient contribution only.
    const float ao        = 0.0f;
    float3 lightAccum     = globalAmbient * albedo;

    GBufferOutput o;
    o.gb0 = float4(albedo, ao);
    o.gb1 = octEncode(shadingNormalV);
    o.gb2 = float4(lightAccum, roughness);
    o.gb3 = metallic;
    return o;
}
