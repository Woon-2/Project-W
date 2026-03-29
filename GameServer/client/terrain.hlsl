#define MAX_TERRAIN_LAYERS 4
#define MAX_CSM_CASCADES 4

cbuffer PerDrawcallData : register(b0) {
    float4x4 wvp;
    float4x4 world;
    float4x4 wv;            // world-view (uniform scale assumed)

    int4  idxSplatMap;
    int4  idxDiffuse[MAX_TERRAIN_LAYERS];
    int4  idxNormal [MAX_TERRAIN_LAYERS];
    float4 tiling            [MAX_TERRAIN_LAYERS]; // xy = tileSize, zw = tileOffset
    float4 metallicRoughness [MAX_TERRAIN_LAYERS]; // x = metallic, y = roughness
    int    layerCount;
    int    hasAnyNormal;
    float2 _pdd0;
};

// Same layout as PBRShader::PerFrameData
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
};

static float4x4 gmtxTexturize = {
    0.5f,  0.0f, 0.0f, 0.0f,
    0.0f, -0.5f, 0.0f, 0.0f,
    0.0f,  0.0f, 1.0f, 0.0f,
    0.5f,  0.5f, 0.0f, 1.0f
};

// Skip illuminate() from pbrLighting.hlsli (it requires a Material cbuffer
// that terrain does not have — terrain blends albedo from splat layers instead).
#define TERRAIN_SHADER
#include "pbrLighting.hlsli"

// ---------------------------------------------------------------------------
// Vertex shader
// ---------------------------------------------------------------------------

struct VSOutput {
    float4 posH       : SV_Position;
    float3 posV       : POSITION_V;   // view-space position
    float3 posW       : POSITION_W;   // world-space position (for CSM shadow)
    float3 normalV    : NORMAL_V;     // view-space normal
    float3 normalW    : NORMAL_W;     // world-space geometric normal (for shadow normal offset)
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
    ret.posV    = mul(float4(position, 1.f), wv).xyz;
    ret.posW    = mul(float4(position, 1.f), world).xyz;
    ret.normalV = normalize(mul(normal, (float3x3)wv)); // uniform scale: no inv-transpose needed
    ret.normalW = mul(normal, (float3x3)world);         // uniform scale: direct upper-left 3x3 is correct
    if (hasAnyNormal) {
        ret.tangentV   = normalize(mul(tangent,   (float3x3)wv));
        ret.bitangentV = normalize(mul(bitangent, (float3x3)wv));
    }
    ret.uv      = uv;
    return ret;
}

// ---------------------------------------------------------------------------
// Pixel shader
// ---------------------------------------------------------------------------

float4 PSMain(VSOutput input) : SV_TARGET {
    // 1. Sample splat weights.
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
    float3 albedo        = float3(0.f, 0.f, 0.f);
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

        // Normal map.
        // Unity DXT5nm format: X stored in Alpha channel, Y in Green channel.
        // Red channel is a dummy constant (≈1.0) — do NOT read nmSample.rg.
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
    const float ao = 0.0f;

    float3 posVNorm = normalize(input.posV);
    float3 color    = float3(0.f, 0.f, 0.f);

    for (uint li = 0; li < lightCnt; li++) {
        if (gLightData[li].type == LIGHT_TYPE_POINT) {
            color += pointLight(li, input.posV, posVNorm, shadingNormalV,
                                input.uv, albedo, roughness, metallic, ao);
        } else if (gLightData[li].type == LIGHT_TYPE_SPOT) {
            color += spotLight(li, input.posV, posVNorm, shadingNormalV,
                               input.uv, albedo, roughness, metallic, ao);
        } else if (gLightData[li].type == LIGHT_TYPE_DIRECTIONAL) {
            color += dirLight(li, input.posV, posVNorm, shadingNormalV,
                              input.uv, albedo, roughness, metallic, ao);
        }
    }

    // 5. Shadow.
    // Raw (unsaturated) NdotL: back-lit faces (< 0) must stay negative so
    // calcCSMShadow applies zero normal offset and avoids shadow flicker.
    float ndotl = 0.5f;
    for (uint li2 = 0u; li2 < lightCnt; ++li2) {
        if (gLightData[li2].type == LIGHT_TYPE_DIRECTIONAL) {
            ndotl = dot(shadingNormalV, -gLightData[li2].dirV);
            break;
        }
    }
    float shadow = calcCSMShadow(input.posV, input.posW, normalize(input.normalW), ndotl);
    color *= shadow;

#ifdef CSM_DEBUG_VIS
    {
        static const float3 kCascadeColors[4] = {
            float3(1,0,0), float3(0,1,0), float3(0,0,1), float3(1,1,0)
        };
        uint dbgCascade = cascadeCount - 1u;
        float dbgSplits[4] = {
            cascadeSplitsFarV.x, cascadeSplitsFarV.y,
            cascadeSplitsFarV.z, cascadeSplitsFarV.w
        };
        [unroll]
        for (uint dci = 0u; dci < cascadeCount; ++dci) {
            if (input.posV.z < dbgSplits[dci]) { dbgCascade = dci; break; }
        }
        color = lerp(color, kCascadeColors[dbgCascade], 0.4f);
    }
#endif

    // 6. Ambient.
    color += globalAmbient * albedo * (1.f - ao);

    // 7. Tonemap (Reinhard) + gamma correction.
    color = color / (color + float3(1.f, 1.f, 1.f));
    color = pow(abs(color), 1.f / 2.2f);

    return float4(color, 1.f);
}
