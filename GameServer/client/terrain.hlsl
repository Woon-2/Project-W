#include "bindless.hlsli"

// ---------------------------------------------------------------------------
// Constant buffers
// ---------------------------------------------------------------------------

#define MAX_TERRAIN_LAYERS 4

cbuffer PerDrawcallData : register(b0) {
    float4x4 wvp;
    float4x4 world;         // terrain world matrix (identity for ground-origin terrain)
    float4x4 worldNormal;   // inverse-transpose of world (also identity here)

    int4  idxSplatMap;               // bindless index for the RGBA splat map
    int4  idxDiffuse[MAX_TERRAIN_LAYERS];
    int4  idxNormal [MAX_TERRAIN_LAYERS];
    // tiling.xy = tileSize, tiling.zw = tileOffset
    float4 tiling   [MAX_TERRAIN_LAYERS];
    int    layerCount;
    float3 _pdd0;
};

cbuffer PerFrameData : register(b1) {
    float3 lightDirW;
    float  _p0;
    float3 lightColor;
    float  lightIntensity;
    float3 globalAmbient;
    float  _p1;
    int4   idxShadowMap;
    float4x4 lightVP;
};

// ---------------------------------------------------------------------------
// Vertex shader
// ---------------------------------------------------------------------------

struct VSOutput {
    float4 posH   : SV_Position;
    float3 posW   : POSITION_W;   // world-space position (for shadow lookup)
    float3 normalW: NORMAL_W;     // world-space normal
    float2 uv     : UV;           // normalized terrain UV [0,1]
};

VSOutput VSMain(
    float3 position : POSITION,
    float3 normal   : NORMAL,
    float2 uv       : UV
) {
    VSOutput ret;
    float4 posW4  = mul(float4(position, 1.f), world);
    ret.posH      = mul(float4(position, 1.f), wvp);
    ret.posW      = posW4.xyz;
    ret.normalW   = normalize(mul(normal, (float3x3)worldNormal));
    ret.uv        = uv;
    return ret;
}

// ---------------------------------------------------------------------------
// Pixel shader
// ---------------------------------------------------------------------------

// Builds a simple TBN matrix from a world-space normal.
// Since terrain normals mostly point up, we bias the reference vector.
float3x3 buildTBN(float3 N) {
    float3 ref = abs(N.y) < 0.999f ? float3(0.f, 1.f, 0.f) : float3(1.f, 0.f, 0.f);
    float3 T   = normalize(cross(ref, N));
    float3 B   = cross(N, T);
    return float3x3(T, B, N); // rows: T, B, N -> tangent-to-world
}

float4 PSMain(VSOutput input) : SV_TARGET {
    // 1. Sample splat weights from the RGBA splat map.
    float4 splatWeights = sampleBindless(idxSplatMap, input.uv);

    // Normalize weights to sum to 1.
    float totalWeight = splatWeights.r + splatWeights.g + splatWeights.b + splatWeights.a;
    if (totalWeight > 0.0001f) {
        splatWeights /= totalWeight;
    } else {
        splatWeights = float4(1.f, 0.f, 0.f, 0.f);
    }

    // Pack weights into an array for indexed access.
    float weights[4];
    weights[0] = splatWeights.r;
    weights[1] = splatWeights.g;
    weights[2] = splatWeights.b;
    weights[3] = splatWeights.a;

    // 2. Blend diffuse albedo across layers.
    float3 albedo = float3(0.f, 0.f, 0.f);
    float3 blendedNormalTS = float3(0.f, 0.f, 0.f); // tangent-space

    [unroll]
    for (int i = 0; i < MAX_TERRAIN_LAYERS; ++i) {
        if (i >= layerCount) break;

        float w = weights[i];
        if (w < 0.0001f) continue;

        // Per-layer UV with tiling and offset.
        float2 layerUV = input.uv * tiling[i].xy + tiling[i].zw;

        // Diffuse contribution.
        float4 diffuseSample = sampleBindless(idxDiffuse[i], layerUV);
        albedo += diffuseSample.rgb * w;

        // Normal map contribution (tangent-space, RG channels store XY deviation).
        if (idxNormal[i].x >= 0) {
            float4 nmSample = sampleBindless(idxNormal[i], layerUV);
            // Reconstruct tangent-space normal from BC5/RGBA normal map.
            float3 nTS;
            nTS.xy = nmSample.rg * 2.f - 1.f;
            nTS.z  = sqrt(max(0.f, 1.f - dot(nTS.xy, nTS.xy)));
            blendedNormalTS += nTS * w;
        } else {
            blendedNormalTS += float3(0.f, 0.f, 1.f) * w; // flat normal
        }
    }

    // 3. Compute world-space shading normal from blended tangent-space normal.
    float3 vertNormalW  = normalize(input.normalW);
    float3x3 tbn        = buildTBN(vertNormalW);
    // tbn rows are T, B, N. We want to transform from tangent to world:
    float3 shadingNormalW = normalize(
        blendedNormalTS.x * tbn[0] +
        blendedNormalTS.y * tbn[1] +
        blendedNormalTS.z * tbn[2]
    );

    // 4. Shadow lookup.
    static float4x4 gmtxTexturize = {
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f,-0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f
    };
    float4 posL = mul(mul(float4(input.posW, 1.f), lightVP), gmtxTexturize);
    float shadow = 1.f;
    if (idxShadowMap.x >= 0) {
        float2 shadowUV = posL.xy / posL.w;
        float  shadowZ  = posL.z  / posL.w;
        shadow = sampleCmpBindless(idxShadowMap, shadowUV, shadowZ);
    }

    // 5. Lambertian directional lighting.
    float3 L       = normalize(-lightDirW);
    float  NdotL   = saturate(dot(shadingNormalW, L));
    float3 diffuse = albedo * lightColor * lightIntensity * NdotL; // * shadow;
    float3 ambient = albedo * globalAmbient;

    float3 color = diffuse + ambient;
    return float4(color, 1.f);
}
