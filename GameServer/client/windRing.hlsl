// windRing.hlsl
// Mesh particle shader for ring-shaped wind/tornado particles.
// Features edge fade: rim alpha based on dot(normalWS, viewDir).
// Pipeline: WindRingPipeline (Mesh mode, CullMode = None, alpha blend).

#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 world;   // row-major (transposed before upload)
    float4   tint;    // ColorOverLifetime * startColor
};

// b0: per-drawcall  (48B, 16B-aligned)
cbuffer PerDrawcallData : register(b0) {
    uint4  idxTex;               // 16B  bindless texture index
    uint   firstInstanceOffset;  // 4B
    float  edgeFadePower;        // 4B   Fresnel exponent
    float  edgeFadeStrength;     // 4B   0 = no fade, 1 = full fade
    float  pad0;                 // 4B
    float3 cameraPosW;           // 12B  world-space camera position
    float  pad1;                 // 4B
};

// b1: per-frame (64B)
cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;  // row-major (transposed before upload)
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : UV;
    uint   instID   : SV_InstanceID;
};

struct PSInput {
    float4 pos     : SV_Position;
    float2 uv      : UV;
    float4 tint    : TINT;
    float3 normalW : NORMALW;
    float3 posW    : POSW;
};

PSInput VSMain(VSInput input) {
    PSInput ret;
    PerInstanceData inst = gInstances[firstInstanceOffset + input.instID];

    float4 worldPos = mul(float4(input.position, 1.0f), inst.world);
    ret.pos    = mul(worldPos, matViewProj);
    ret.uv     = input.uv;
    ret.tint   = inst.tint;
    ret.posW   = worldPos.xyz;
    // Transform normal by rotation part of world matrix (works for uniform scale).
    ret.normalW = normalize(mul(float4(input.normal, 0.0f), inst.world).xyz);
    return ret;
}

float4 PSMain(PSInput input) : SV_TARGET {
    float4 src = sampleBindless(idxTex, input.uv);

    float3 viewDir  = normalize(cameraPosW - input.posW);
    float  NdotV    = saturate(abs(dot(normalize(input.normalW), viewDir)));
    float  edgeFade = pow(NdotV, edgeFadePower);
    float  alpha    = src.a * input.tint.a * lerp(1.0f, edgeFade, edgeFadeStrength);

    return float4(src.rgb * input.tint.rgb, alpha);
}
