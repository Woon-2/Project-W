#define MAX_CSM_CASCADES 4
#define DEFERRED_LIGHTING_PASS

// GBuffer debug modes (matches GFX::gBufferDebugMode_)
#define GBUF_DEBUG_NONE       0u
#define GBUF_DEBUG_ALBEDO     1u
#define GBUF_DEBUG_NORMAL     2u
#define GBUF_DEBUG_AO         3u
#define GBUF_DEBUG_ROUGHNESS  4u
#define GBUF_DEBUG_METALLIC   5u
#define GBUF_DEBUG_LIGHTACCUM 6u
#define GBUF_DEBUG_DEPTH      7u

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv  : UV;
};

// Full PerFrameData for the deferred lighting pass.
// Matches PBRDeferredLightingShader::PerFrameData in shader.hpp.
cbuffer PerFrameData : register(b1) {
    // CSM / lighting fields (same layout as PBRShader::PerFrameData)
    float3   globalAmbient;
    float    _pfd0;
    uint     lightCnt;
    uint     cascadeCount;
    uint2    _pfd1;
    int4     idxShadowMap[MAX_CSM_CASCADES];
    float4   cascadeSplitsFarV;
    float4x4 lightVP[MAX_CSM_CASCADES];
    float4   cascadeNormalOffsets;
    // Deferred reconstruction
    float4x4 invView;
    float4x4 invProj;
    // GBuffer bindless SRV indices
    int4     idxGB0;
    int4     idxGB1;
    int4     idxGB2;
    int4     idxGB3;
    int4     idxDepth;
    // Debug
    uint     debugMode;
    uint3    _pad;
}

// gmtxTexturize required by sampleCascadePCF in pbrLighting.hlsli
static float4x4 gmtxTexturize = {
    0.5f,  0.0f, 0.0f, 0.0f,
    0.0f, -0.5f, 0.0f, 0.0f,
    0.0f,  0.0f, 1.0f, 0.0f,
    0.5f,  0.5f, 0.0f, 1.0f
};

#include "pbrLighting.hlsli"

// Fullscreen triangle — no vertex buffer needed.
// Covers the entire screen with 3 vertices driven by SV_VertexID.
VSOutput VSMain(uint vertexID : SV_VertexID) {
    // vertexID 0 → uv(0,0), 1 → uv(2,0), 2 → uv(0,2)
    float2 uv  = float2((vertexID << 1) & 2, vertexID & 2);
    // NDC: uv(0,0) → (-1,+1), uv(2,0) → (+3,+1), uv(0,2) → (-1,-3)
    float4 pos = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);

    VSOutput ret;
    ret.pos = pos;
    ret.uv  = uv;
    return ret;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    float2 uv = input.uv;

    // --- Sample GBuffer ---
    float4 gb0 = sampleBindless(idxGB0, uv);         // albedo.rgb + ao
    float2 gb1 = sampleBindless(idxGB1, uv).rg;      // oct-encoded normalV
    float4 gb2 = sampleBindless(idxGB2, uv);         // lightAccum.rgb + roughness
    float  gb3 = sampleBindless(idxGB3, uv).r;       // metallic
    float  rawDepth = sampleBindless(idxDepth, uv).r; // NDC depth [0,1]

    float3 albedo      = gb0.rgb;
    float  ao          = gb0.a;
    float3 precompLight = gb2.rgb;  // ambient + emissive (pre-computed in geometry pass)
    float  roughness   = gb2.a;
    float  metallic    = gb3;

    // --- Decode view-space normal ---
    float3 normalV = octDecode(gb1);

    // --- Reconstruct view-space position from depth ---
    // NDC.xy from UV: x in [-1,+1], y in [+1,-1] (DX NDC top=+1)
    float4 posNDC = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, rawDepth, 1.0f);
    float4 posV_h = mul(posNDC, invProj);
    float3 posV   = posV_h.xyz / posV_h.w;

    // --- Reconstruct world-space position ---
    float3 posW = mul(float4(posV, 1.0f), invView).xyz;

    // --- Reconstruct world-space normal ---
    // View rotation is orthonormal, so invView 3x3 == transpose(viewMatrix 3x3).
    // normalW = mul(normalV, invView3x3) in row-major convention.
    float3 normalW = normalize(mul(normalV, (float3x3)invView));

    // --- GBuffer Debug Views ---
    if (debugMode == GBUF_DEBUG_ALBEDO) {
        float3 srgb = pow(abs(albedo), 1.0f / 2.2f);
        return float4(srgb, 1.0f);
    }
    if (debugMode == GBUF_DEBUG_NORMAL) {
        return float4(normalV * 0.5f + 0.5f, 1.0f);
    }
    if (debugMode == GBUF_DEBUG_AO) {
        return float4(ao, ao, ao, 1.0f);
    }
    if (debugMode == GBUF_DEBUG_ROUGHNESS) {
        return float4(roughness, roughness, roughness, 1.0f);
    }
    if (debugMode == GBUF_DEBUG_METALLIC) {
        return float4(metallic, metallic, metallic, 1.0f);
    }
    if (debugMode == GBUF_DEBUG_LIGHTACCUM) {
        // Show pre-computed ambient+emissive; apply gamma for display
        float3 srgb = pow(abs(precompLight), 1.0f / 2.2f);
        return float4(saturate(srgb), 1.0f);
    }
    if (debugMode == GBUF_DEBUG_DEPTH) {
        // Raw NDC depth (0=near, 1=far) shown as grayscale
        return float4(rawDepth, rawDepth, rawDepth, 1.0f);
    }

    // --- Full Deferred Lighting ---
    // illuminateFromGBuffer returns direct lighting * shadow (pre-tonemap)
    float3 directLight = illuminateFromGBuffer(posV, posW, normalV, normalW, albedo, roughness, metallic, ao);

    // Add pre-computed ambient + emissive from GB2.rgb
    float3 color = directLight + precompLight;

    // Reinhard tonemapping + gamma correction
    color = color / (color + float3(1.0f, 1.0f, 1.0f));
    color = pow(abs(color), 1.0f / 2.2f);

    return float4(color, 1.0f);
}
