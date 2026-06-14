// IBL diffuse irradiance convolution (compute).
// For each output cubemap texel, integrate cosine-weighted radiance over the
// hemisphere around the texel direction, sampling the environment cubemap.
// Output: RWTexture2DArray<float4> (6 faces) bound to the "DestTex" UAV (u0).
// Env cube is read bindlessly (sampleLevelBindlessCube). D3D cube face basis
// (left-handed); no GL-style Y flip.

#include "bindless.hlsli"

// Matches IBLShader::IBLParams (shader.hpp). Bound at PerDrawcallData (b0).
cbuffer IBLParams : register(b0) {
    int4  idxEnv;      // bindless cube SRV of the source environment
    uint  faceRes;     // output face resolution (pixels per side)
    uint  mipLevel;    // unused for irradiance
    uint  mipCount;    // unused for irradiance
    uint  envIsLDR;    // 1 => recover approx HDR from an LDR source
    float roughness;   // unused for irradiance
    float3 _pad;
};

RWTexture2DArray<float4> dstCube : register(u0);

static const float PI = 3.14159265f;

// D3D cubemap face basis: face index 0..5 = +X,-X,+Y,-Y,+Z,-Z.
// uv in [0,1] with v increasing downward (texture convention).
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
        // Approximate inverse Reinhard: lifts an LDR [0,1) source toward HDR so
        // bright sky retains energy. Removed automatically once a true HDR env is used.
        c = c / max(1.0f - c, 1e-4f);
    }
    return c;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= faceRes || id.y >= faceRes) return;
    uint face = id.z;

    float2 uv = (float2(id.xy) + 0.5f) / float(faceRes);
    float3 N = dirFromFaceUV(face, uv);

    // Build a tangent frame around N.
    float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);

    float3 irradiance = float3(0.0f, 0.0f, 0.0f);
    float  nrSamples  = 0.0f;
    const float sampleDelta = 0.025f;

    for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta) {
        for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta) {
            // tangent-space sample -> world
            float3 ts = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = ts.x * T + ts.y * B + ts.z * N;
            irradiance += sampleEnv(sampleVec) * cos(theta) * sin(theta);
            nrSamples  += 1.0f;
        }
    }
    irradiance = PI * irradiance / max(nrSamples, 1.0f);

    dstCube[uint3(id.xy, face)] = float4(irradiance, 1.0f);
}
