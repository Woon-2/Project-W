#define MAX_CSM_CASCADES 4

struct PerInstanceData {
    float4x4 world;
    float4x4 wvp;
    float4x4 wv;
    float3x3 wvNormal;
    float3x3 worldNormal;
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
};

// GBuffer output: 5 render targets
struct GBufferOutput {
    float4 gb0 : SV_TARGET0;  // Albedo.rgb (linear) + AO.a        | R8G8B8A8_UNORM
    float2 gb1 : SV_TARGET1;  // NormalV oct-encoded (view-space)   | R16G16_FLOAT
    float4 gb2 : SV_TARGET2;  // Emissive.rgb (HDR)                  | R11G11B10_FLOAT
    float2 gb3 : SV_TARGET3;  // Metallic.r + Roughness.g            | R8G8_UNORM
    float  gb4 : SV_TARGET4;  // Linear view-space Z (posV.z)       | R32_FLOAT
};

cbuffer PerDrawcallData : register(b0) {
    Material material;
    uint firstInstanceOffset;
};

// Indirect (Hi-Z occlusion) path only: the command signature supplies the per-group
// instance offset as a root constant (b0, space1), and gVisibleIndices remaps the
// compacted draw instance back to the original perInstanceData slot.
#ifdef HiZCull
cbuffer FirstInstanceOffset : register(b0, space1) {
    uint indirectFirstInstanceOffset;
}
StructuredBuffer<uint> gVisibleIndices : register(t3);
#endif

// Full PerFrameData layout — must match PBRShader::PerFrameData so that
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

static float4x4 gmtxTexturize = {
    0.5f,  0.0f, 0.0f, 0.0f,
    0.0f, -0.5f, 0.0f, 0.0f,
    0.0f,  0.0f, 1.0f, 0.0f,
    0.5f,  0.5f, 0.0f, 1.0f
};

#include "pbrLighting.hlsli"

VSOutput VSMain(
    float3 position : POSITION,
    float3 normal   : NORMAL,
    float3 tangent  : TANGENT,
    float3 bitangent: BITANGENT,
    float2 uv       : UV,
    uint   idxInst  : SV_InstanceID
) {
    VSOutput ret;

#ifdef HiZCull
    uint idx = gVisibleIndices[idxInst + indirectFirstInstanceOffset];
#else
    uint idx = idxInst + firstInstanceOffset;
#endif
    ret.pos        = mul(float4(position, 1.0f), gInstances[idx].wvp);
    ret.posV       = mul(float4(position, 1.0f), gInstances[idx].wv).xyz;
    ret.posW       = mul(float4(position, 1.0f), gInstances[idx].world).xyz;
    ret.normalV    = mul(normal, gInstances[idx].wvNormal);
    ret.normalW    = mul(normal, gInstances[idx].worldNormal);
    if (material.idxNormal.x >= 0) {
        ret.tangentV   = mul(tangent,   gInstances[idx].wvNormal);
        ret.bitangentV = mul(bitangent, gInstances[idx].wvNormal);
    }
    ret.uv = uv;

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
    // Foliage alpha test (tree leaves / billboard grass). cAlphaCutoff == 0 for
    // opaque materials, so this is a no-op for all non-foliage geometry.
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
    // Unity semantics: emission = emissionColor * emissionMap (HDR intensity rides on the
    // color). Multiply (not replace) and pow(2.2) to mirror the albedo sRGB->linear path.
    float3 emissive = material.cEmmisive;
    if (material.idxEmmisive.x >= 0) {
        emissive *= sampleBindless(material.idxEmmisive, input.uv).rgb;
    }
    emissive = pow(abs(emissive), 2.2f);

    // --- Pre-compute emissive (stored in GB2.rgb, HDR) ---
    // GB2.rgb holds emissive only; ambient/IBL is computed in the deferred lighting pass.
    float3 lightAccum = emissive;

    GBufferOutput o;
    o.gb0 = float4(albedo.rgb, ao);
    o.gb1 = octEncode(input.normalV);
    o.gb2 = float4(lightAccum, 0.0f);          // rgb=emissive (HDR), a unused
    o.gb3 = float2(metallic, roughness);       // r=metallic, g=roughness
    o.gb4 = input.posV.z;  // exact linear view-space depth (deferred reconstruction)
    return o;
}
