// IBL specular pre-filtered environment map (compute).
// Each mip level corresponds to a roughness; the texel is filtered with GGX
// importance sampling of the environment cubemap (split-sum, prefilter term).
// One dispatch per mip; the mip's roughness/resolution come from the cbuffer and
// the bound DestTex UAV is that mip's TEXTURE2DARRAY view. D3D cube face basis.

#include "bindless.hlsli"

// Matches IBLShader::IBLParams (shader.hpp). Bound at PerDrawcallData (b0).
cbuffer IBLParams : register(b0) {
    int4  idxEnv;      // bindless cube SRV of the source environment
    uint  faceRes;     // this mip's face resolution
    uint  mipLevel;    // this mip index
    uint  mipCount;    // total mip count
    uint  envIsLDR;    // 1 => recover approx HDR from an LDR source
    float roughness;   // this mip's roughness in [0,1]
    float3 _pad;
};

RWTexture2DArray<float4> dstCube : register(u0);

static const float PI = 3.14159265f;

float3 dirFromFaceUV(uint face, float2 uv) {
    float a = 2.0f * uv.x - 1.0f;
    float b = 2.0f * uv.y - 1.0f;
    float3 d;
    if      (face == 0u) d = float3( 1.0f,   -b,   -a);
    else if (face == 1u) d = float3(-1.0f,   -b,    a);
    else if (face == 2u) d = float3(    a, 1.0f,    b);
    else if (face == 3u) d = float3(    a,-1.0f,   -b);
    else if (face == 4u) d = float3(    a,   -b, 1.0f);
    else                 d = float3(   -a,   -b,-1.0f);
    return normalize(d);
}

float3 sampleEnv(float3 dir) {
    float3 c = sampleLevelBindlessCube(idxEnv, dir, 0.0f).rgb;
    if (envIsLDR != 0u) {
        c = c / max(1.0f - c, 1e-4f);
    }
    return c;
}

// Van der Corput radical inverse + Hammersley low-discrepancy sequence.
float radicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}
float2 hammersley(uint i, uint n) {
    return float2(float(i) / float(n), radicalInverseVdC(i));
}

// GGX importance sample: returns a world-space half vector around N.
float3 importanceSampleGGX(float2 Xi, float3 N, float rough) {
    float a = rough * rough;
    float phi = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);
    return normalize(T * H.x + B * H.y + N * H.z);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= faceRes || id.y >= faceRes) return;
    uint face = id.z;

    float2 uv = (float2(id.xy) + 0.5f) / float(faceRes);
    float3 N = dirFromFaceUV(face, uv);
    float3 R = N;
    float3 V = N;

    const uint SAMPLE_COUNT = 256u;
    float3 prefiltered = float3(0.0f, 0.0f, 0.0f);
    float  totalWeight = 0.0f;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        float2 Xi = hammersley(i, SAMPLE_COUNT);
        float3 H  = importanceSampleGGX(Xi, N, roughness);
        float3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0f);
        if (NdotL > 0.0f) {
            prefiltered += sampleEnv(L) * NdotL;
            totalWeight += NdotL;
        }
    }
    prefiltered = prefiltered / max(totalWeight, 1e-4f);

    dstCube[uint3(id.xy, face)] = float4(prefiltered, 1.0f);
}
