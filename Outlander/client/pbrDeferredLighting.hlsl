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
#define GBUF_DEBUG_IBL_DIFFUSE  8u
#define GBUF_DEBUG_IBL_SPECULAR 9u
#define GBUF_DEBUG_BRDF         10u
// CSM cascade shadow-map visualization (raw light-space depth)
#define GBUF_DEBUG_CASCADE0     11u
#define GBUF_DEBUG_CASCADE1     12u
#define GBUF_DEBUG_CASCADE2     13u
#define GBUF_DEBUG_CASCADE3     14u
// Which cascade each pixel samples, tinted over the lit scene. Not part of the 'G'
// cycle above — GFX::effectiveDebugMode() feeds it from the dedicated 'C' toggle.
// Keep in sync with GFX::kGBufferDebugCascadeLevel.
#define GBUF_DEBUG_CASCADE_LEVEL 15u

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
    int4     idxGB4;   // linear view-space Z (posV.z), R32_FLOAT
    // Debug
    uint     debugMode;
    uint3    _pad0;
    // Exponential Height Fog
    int4 idxSkybox;
    float fogDensity;
    float heightFalloff;
    float fogBaseHeight;
    float _pad1;
    float3 camPos;
    float _pad2;
    // IBL
    int4  idxIrradiance;
    int4  idxPrefiltered;
    int4  idxBRDFLUT;
    uint  prefilteredMipCount;
    float iblIntensity;
    float2 _iblPad;
}

// gmtxTexturize required by sampleCascadePCF in pbrLighting.hlsli
static float4x4 gmtxTexturize = {
    0.5f,  0.0f, 0.0f, 0.0f,
    0.0f, -0.5f, 0.0f, 0.0f,
    0.0f,  0.0f, 1.0f, 0.0f,
    0.5f,  0.5f, 0.0f, 1.0f
};

#define IBL_ENABLED
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
    float4 gb2 = sampleBindless(idxGB2, uv);         // emissive.rgb (HDR)
    float2 gb3 = sampleBindless(idxGB3, uv).rg;      // metallic.r + roughness.g
    float  rawDepth = sampleBindless(idxDepth, uv).r; // NDC depth [0,1]

    float3 albedo      = gb0.rgb;
    float  ao          = gb0.a;
    float3 precompLight = gb2.rgb;  // emissive (pre-computed in geometry pass, HDR)
    float  metallic    = gb3.r;
    float  roughness   = gb3.g;

    // --- Decode view-space normal ---
    float3 normalV = octDecode(gb1);

    // --- Reconstruct view-space position from the exact linear depth (gb4) ---
    // Using the stored view-space Z (R32_FLOAT) instead of the quantized NDC depth keeps
    // the reconstructed position stable as the follow camera micro-moves, which eliminates
    // shadow shimmering. invProj is constant (proj is fixed per frame), so the per-pixel
    // view ray is frame-independent; only linearZ (exact) and invView (correct camera) vary.
    float  linearZ = sampleBindless(idxGB4, uv).r;
    float2 ndc     = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 farH    = mul(float4(ndc, 1.0f, 1.0f), invProj); // unproject at the NDC far plane
    float3 rayV    = farH.xyz / farH.w;
    float3 posV    = rayV * (linearZ / rayV.z);

    // --- Reconstruct world-space position ---
    float3 posW = mul(float4(posV, 1.0f), invView).xyz;

    // --- Reconstruct world-space normal ---
    // View rotation is orthonormal, so invView 3x3 == transpose(viewMatrix 3x3).
    // normalW = mul(normalV, invView3x3) in row-major convention.
    float3 normalW = normalize(mul(normalV, (float3x3)invView));

    // World-space view vector (needed by both IBL and the IBL debug views below).
    float3 Vworld = normalize(camPos - posW);

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
    // IBL debug views. Display-encoded here; the resolve pass passes debug output
    // through untouched (no second tonemap/gamma).
    if (debugMode == GBUF_DEBUG_IBL_DIFFUSE) {
        float  NdotV = max(dot(normalW, Vworld), 0.0f);
        float3 F0    = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
        float3 kS    = fresnelSchlickRoughness(NdotV, F0, roughness);
        float3 kD    = (1.0f - kS) * (1.0f - metallic);
        float3 irr   = sampleBindlessCube(idxIrradiance, normalW).rgb;
        float3 d     = kD * irr * albedo;
        return float4(pow(abs(d), 1.0f / 2.2f), 1.0f);
    }
    if (debugMode == GBUF_DEBUG_IBL_SPECULAR) {
        float  NdotV = max(dot(normalW, Vworld), 0.0f);
        float3 F0    = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
        float3 R     = reflect(-Vworld, normalW);
        float  maxMip = float(max(prefilteredMipCount, 1u) - 1u);
        float3 pf    = sampleLevelBindlessCube(idxPrefiltered, R, roughness * maxMip).rgb;
        float2 brdf  = sampleBindless(idxBRDFLUT, float2(NdotV, roughness)).rg;
        float3 s     = pf * (F0 * brdf.x + brdf.y);
        return float4(pow(abs(s), 1.0f / 2.2f), 1.0f);
    }
    if (debugMode == GBUF_DEBUG_BRDF) {
        float  NdotV = max(dot(normalW, Vworld), 0.0f);
        float2 brdf  = sampleBindless(idxBRDFLUT, float2(NdotV, roughness)).rg;
        return float4(brdf, 0.0f, 1.0f);
    }
    // CSM cascade shadow-map view (raw light-space depth, stretched fullscreen). The shadow
    // SRV carries a comparison sampler, so read the texel directly with Load (no sampler).
    if (debugMode >= GBUF_DEBUG_CASCADE0 && debugMode <= GBUF_DEBUG_CASCADE3) {
        uint ci = debugMode - GBUF_DEBUG_CASCADE0;
        if (ci >= cascadeCount) return float4(0.0f, 0.0f, 0.0f, 1.0f);
        int4 sidx = idxShadowMap[ci];
        if (sidx.x != 0) return float4(0.0f, 0.0f, 0.0f, 1.0f); // not a bound Texture2D
        uint sw, sh;
        gTex2Ds[sidx.y].GetDimensions(sw, sh);
        float d = gTex2Ds[sidx.y].Load(int3(int2(saturate(uv) * float2(sw, sh)), 0)).r;
        return float4(d, d, d, 1.0f);
    }

    // --- Full Deferred Lighting ---
    // illuminateFromGBuffer returns direct lighting * shadow (pre-tonemap)
    float3 directLight = illuminateFromGBuffer(posV, posW, normalV, normalW, albedo, roughness, metallic, ao);

    // Image-based lighting (diffuse irradiance + specular reflection). World-space N/V.
    float3 ibl = computeIBL(normalW, Vworld, albedo, roughness, metallic, ao);

    // precompLight (GB2.rgb) is emissive only now; combine direct + IBL + emissive.
    float3 color = directLight + ibl + precompLight;

    // Apply Exponential Height Fog
    float3 viewDir = normalize(posW);
    float3 fogColor = sampleLevelBindlessCube(idxSkybox, viewDir, 6.f).rgb;
    
    color = lerp(color, fogColor, computeExponentialHeightFog(posW, camPos, fogDensity, heightFalloff, fogBaseHeight));

    // Cascade level view: tint the lit scene by the cascade it samples, so the split
    // boundaries are readable against real geometry and shadows. The resolve passes
    // debug output through untouched, so display-encode here with the same Reinhard +
    // gamma the forward path uses (pbrLighting.hlsli::illuminateCSM) and tint on top —
    // tinting linear HDR instead would let the tonemap wash the hue out.
    // Background pixels are overwritten by the skybox composite that follows this pass.
    if (debugMode == GBUF_DEBUG_CASCADE_LEVEL) {
        float3 disp = color / (color + 1.0f);
        disp = pow(abs(disp), 1.0f / 2.2f);
        if (cascadeCount > 0u)
            disp = lerp(disp, kCascadeDebugColors[selectCascadeIndex(posV.z)], kCascadeDebugTint);
        return float4(disp, 1.0f);
    }

    // HDR pipeline: output linear radiance into SceneColorHDR.
    // Tonemap (Reinhard) + gamma is applied later by the dedicated tonemap-resolve pass.

    return float4(color, 1.0f);
}
