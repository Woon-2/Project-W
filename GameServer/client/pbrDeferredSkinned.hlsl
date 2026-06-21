#define MAX_CSM_CASCADES 4

// Energy-orb absorption ripple knobs. RIPPLE_LIFE must match Object::kBodyRippleLife
// (object.cpp) so the CPU evicts a ripple exactly when this time-fade hits zero.
#define MAX_ABSORB_RIPPLES 4
#define RIPPLE_SPEED       3.0f   // ring expansion speed (m/s)
#define RIPPLE_WIDTH       0.35f  // gaussian ring band thickness (m)
#define RIPPLE_LIFE        1.0f   // ripple lifetime (s)
#define RIPPLE_MAX_RADIUS  2.2f   // spatial cutoff radius (m)

struct PerInstanceData {
    float4x4 world;
    float4x4 wvp;
    float4x4 wv;
    float3x3 wvNormal;
    float3x3 worldNormal;
    uint rootBoneOffset;
    int bakedClipId;
    int bakedClipFrame;
    int padding;
    // Absorption ripples — layout must match PBRDeferredSkinnedGBufferShader::PerInstanceData.
    float4 ripplePosAge[MAX_ABSORB_RIPPLES];          // xyz = contact world pos, w = age (sec)
    float4 rippleColorIntensity[MAX_ABSORB_RIPPLES];  // rgb = HDR color, w = intensity
    uint   rippleCount;
    uint3  ripplePad;
};

struct Material {
    int4 idxAlbedo;
    int4 idxMetallicSmoothness;
    int4 idxNormal;
    int4 idxEmmisive;
    int4 idxAmbientOcclusion;

    float4 cAlbedo;
    float cRoughness;
    float cMetallic;
    float cAOStrength;
    float cAlphaCutoff;   // foliage alpha-test threshold (0 = opaque)
    float3 cEmmisive;
    float padding1;
};

struct VSOutput {
    float4 pos        : SV_Position;
    float3 posV       : POSITION_V;
    float3 posW       : POSITION_W;
    float3 normalV    : NORMAL_V;
    float3 normalW    : NORMAL_W;
    float3 tangentV   : TANGENT_V;
    float3 bitangentV : BITANGENT_V;
    float2 uv         : UV;
    nointerpolation uint instIdx : INSTIDX;  // gInstances index (for per-instance ripples in PS)
};

// GBuffer output: 5 render targets
struct GBufferOutput {
    float4 gb0 : SV_TARGET0;  // Albedo.rgb (linear) + AO.a        | R8G8B8A8_UNORM
    float2 gb1 : SV_TARGET1;  // NormalV oct-encoded (view-space)   | R16G16_FLOAT
    float4 gb2 : SV_TARGET2;  // LightAccum.rgb + Roughness.a       | R8G8B8A8_UNORM
    float  gb3 : SV_TARGET3;  // Metallic                           | R8_UNORM
    float  gb4 : SV_TARGET4;  // Linear view-space Z (posV.z)       | R32_FLOAT
};

cbuffer PerDrawcallData : register(b0) {
    Material material;
};

cbuffer FirstInstanceOffset : register(b0, space1) {
    uint firstInstanceOffset;
}

// Full PerFrameData layout — must match PBRSkinnedShader::PerFrameData so that
// pbrLighting.hlsli (included below) compiles successfully.
cbuffer PerFrameData : register(b1) {
    float3   globalAmbient;
    float    padding0;
    uint     lightCnt;
    uint     cascadeCount;
    uint2    padding1;
    int4     idxShadowMap[MAX_CSM_CASCADES];
    float4   cascadeSplitsFarV;
    float4x4 lightVP[MAX_CSM_CASCADES];
    float4   cascadeNormalOffsets;
    // Camera world position. Unused by this GBuffer geometry pass, but required so the
    // shared pbrLighting.hlsli (which references camPos for camera-relative shadow space)
    // compiles. Kept layout-identical to the other PBR PerFrameData mirrors.
    float3   camPos;
    float    _padCam;
}

StructuredBuffer<PerInstanceData> gInstances : register(t0);
StructuredBuffer<float4x4>        gBoneData  : register(t2);
#ifdef HiZCull
StructuredBuffer<uint> gVisibleIndices : register(t3);
#endif

static float4x4 gmtxTexturize = {
    0.5f,  0.0f, 0.0f, 0.0f,
    0.0f, -0.5f, 0.0f, 0.0f,
    0.0f,  0.0f, 1.0f, 0.0f,
    0.5f,  0.5f, 0.0f, 1.0f
};

#include "pbrLighting.hlsli"

float4x4 loadBakedMatrix(int clipIdx, int frameIdx, int boneIdx) {
    float4x4 M;
    int vIdx = boneIdx * 4;
    // as a texture stores float4 values, we need to load 4x4 matrix as 4 float4 values.
    M[0] = loadBindless(
        int4(IDX_RANGE_TEXTURE2D, clipIdx, 0, 0),
        int2(frameIdx, vIdx + 0), 0
    );
    M[1] = loadBindless(
        int4(IDX_RANGE_TEXTURE2D, clipIdx, 0, 0),
        int2(frameIdx, vIdx + 1), 0
    );
    M[2] = loadBindless(
        int4(IDX_RANGE_TEXTURE2D, clipIdx, 0, 0),
        int2(frameIdx, vIdx + 2), 0
    );
    M[3] = loadBindless(
        int4(IDX_RANGE_TEXTURE2D, clipIdx, 0, 0),
        int2(frameIdx, vIdx + 3), 0
    );

    return M;
}

float4x4 blendBakedTransform(int clipIdx, int frameIdx, uint4 boneIndices, float4 boneWeights)
{
    return
        loadBakedMatrix(clipIdx, frameIdx, boneIndices.x) * boneWeights.x +
        loadBakedMatrix(clipIdx, frameIdx, boneIndices.y) * boneWeights.y +
        loadBakedMatrix(clipIdx, frameIdx, boneIndices.z) * boneWeights.z +
        loadBakedMatrix(clipIdx, frameIdx, boneIndices.w) * boneWeights.w;
}

float4x4 blendBoneTransform(uint rootBoneOffset, uint4 boneIndices, float4 boneWeights) {
    return
        gBoneData[rootBoneOffset + boneIndices.x] * boneWeights.x +
        gBoneData[rootBoneOffset + boneIndices.y] * boneWeights.y +
        gBoneData[rootBoneOffset + boneIndices.z] * boneWeights.z +
        gBoneData[rootBoneOffset + boneIndices.w] * boneWeights.w;
}

VSOutput VSMain(
    float3 position   : POSITION,
    float3 normal     : NORMAL,
    float3 tangent    : TANGENT,
    float3 bitangent  : BITANGENT,
    float2 uv         : UV,
    int4   boneIndices: BONE_INDICES,
    float4 boneWeights: BONE_WEIGHTS,
    uint   idxInst    : SV_InstanceID
) {
    VSOutput ret;

#ifdef HiZCull
    uint idx = gVisibleIndices[idxInst + firstInstanceOffset];
#else
    uint idx = idxInst + firstInstanceOffset;
#endif
    float4x4 anim;
    if (gInstances[idx].bakedClipId == -1) {
        anim = blendBoneTransform(
            gInstances[idx].rootBoneOffset,
            (uint4)boneIndices, boneWeights
        );
    }
    else {
        anim = blendBakedTransform(
            gInstances[idx].bakedClipId,
            gInstances[idx].bakedClipFrame,
            (uint4)boneIndices, boneWeights
        );
    }

    float4 animPos    = mul(float4(position, 1.0f), anim);
    float4 animNormal = mul(float4(normal,   0.0f), anim);

    ret.pos     = mul(animPos, gInstances[idx].wvp);
    ret.posV    = mul(animPos, gInstances[idx].wv).xyz;
    ret.posW    = mul(animPos, gInstances[idx].world).xyz;
    ret.normalV = mul(animNormal.xyz, gInstances[idx].wvNormal);
    ret.normalW = mul(animNormal.xyz, gInstances[idx].worldNormal);
    if (material.idxNormal.x >= 0) {
        float4 animTangent   = mul(float4(tangent,   0.0f), anim);
        float4 animBitangent = mul(float4(bitangent, 0.0f), anim);
        ret.tangentV   = mul(animTangent.xyz,   gInstances[idx].wvNormal);
        ret.bitangentV = mul(animBitangent.xyz, gInstances[idx].wvNormal);
    }
    ret.uv = uv;
    ret.instIdx = idx;

    return ret;
}

GBufferOutput PSMain(VSOutput input) {
    input.normalV = normalize(input.normalV);

    if (material.idxNormal.x >= 0) {
        input.tangentV   = normalize(input.tangentV);
        input.bitangentV = normalize(input.bitangentV);

        float3 normalTS = sampleBindless(material.idxNormal, input.uv).rgb;
        normalTS = normalTS * 2.0f - 1.0f;

        float3x3 TBN = float3x3(input.tangentV, input.bitangentV, input.normalV);
        input.normalV = normalize(mul(normalTS, TBN));
    }

    // --- Albedo (sRGB -> linear) ---
    float4 albedo = material.cAlbedo;
    if (material.idxAlbedo.x >= 0) {
        albedo = sampleBindless(material.idxAlbedo, input.uv);
    }
    // Foliage alpha test for skinned meshes. cAlphaCutoff == 0 for opaque
    // materials, so this is a no-op outside alpha-clipped materials.
    clip(albedo.a - material.cAlphaCutoff);
    albedo.rgb = pow(abs(albedo.rgb), 2.2f);

    // --- Roughness / Metallic ---
    float roughness = material.cRoughness;
    float metallic  = material.cMetallic;
    if (material.idxMetallicSmoothness.x >= 0) {
        float4 ms = sampleBindless(material.idxMetallicSmoothness, input.uv);
        metallic  = ms.r;
        roughness = 1.0f - ms.a;
    }

    // --- AO ---
    float ao = 0.0f;
    if (material.idxAmbientOcclusion.x >= 0) {
        ao = material.cAOStrength * sampleBindless(material.idxAmbientOcclusion, input.uv).r;
    }

    // --- Emissive ---
    float3 emissive = material.cEmmisive;
    if (material.idxEmmisive.x >= 0) {
        emissive = sampleBindless(material.idxEmmisive, input.uv).rgb;
    }

    // --- Pre-compute emissive (stored in GB2.rgb) ---
    // GB2.rgb holds emissive only; ambient/IBL is computed in the deferred lighting
    // pass (matches pbrDeferred.hlsl). Baking globalAmbient here too would double the
    // ambient on skinned meshes (constant ambient in GB2 + IBL added by the lighting pass).
    float3 lightAccum = emissive;

    // --- Energy-orb absorption ripple (additive emissive ring across the body) ---
    // Each absorbed orb spawns an expanding ring of its HDR color. Only the local
    // player carries ripples (rippleCount>0); everything else skips the loop. Stored
    // in GB2 emissive -> the deferred lighting pass promotes it to HDR and bloom
    // picks it up. GB2 is UNORM so the ring peak saturates to a bright hue.
    uint rippleCount = gInstances[input.instIdx].rippleCount;
    [loop]
    for (uint ri = 0u; ri < rippleCount; ++ri) {
        float4 pa = gInstances[input.instIdx].ripplePosAge[ri];
        float4 ci = gInstances[input.instIdx].rippleColorIntensity[ri];
        float dist  = length(input.posW - pa.xyz);
        float ringR = pa.w * RIPPLE_SPEED;
        float d     = (dist - ringR) / RIPPLE_WIDTH;
        float band  = exp(-d * d);   // gaussian ring (squared directly: avoids pow(neg) NaN)
        float tFade = saturate(1.0f - pa.w / RIPPLE_LIFE);
        float sFade = saturate(1.0f - dist / RIPPLE_MAX_RADIUS);
        lightAccum += ci.rgb * (ci.w * band * tFade * sFade);
    }

    GBufferOutput o;
    o.gb0 = float4(albedo.rgb, ao);
    o.gb1 = octEncode(input.normalV);
    o.gb2 = float4(lightAccum, roughness);
    o.gb3 = metallic;
    o.gb4 = input.posV.z;  // exact linear view-space depth (deferred reconstruction)
    return o;
}
