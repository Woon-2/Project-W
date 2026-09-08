// IBL BRDF integration LUT (compute, split-sum environment BRDF).
// Output: RWTexture2D<float2> (R16G16_FLOAT) bound to the "DestTex" UAV (u0).
// x axis = NdotV, y axis = roughness. Result = (scale, bias) for F0.
// Environment-independent; computed once. Uses the IBL geometry term k = roughness^2/2.

// Matches IBLShader::IBLParams (shader.hpp). Bound at PerDrawcallData (b0).
cbuffer IBLParams : register(b0) {
    int4  idxEnv;      // unused
    uint  faceRes;     // LUT resolution (pixels per side)
    uint  mipLevel;    // unused
    uint  mipCount;    // unused
    uint  envIsLDR;    // unused
    float roughness;   // unused (derived per texel)
    float3 _pad;
};

RWTexture2D<float2> dstLUT : register(u0);

static const float PI = 3.14159265f;

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

// IBL geometry term: k = roughness^2 / 2 (differs from the direct-light k=(r+1)^2/8).
float geometrySchlickGGX(float NdotV, float rough) {
    float k = (rough * rough) / 2.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}
float geometrySmith(float NdotV, float NdotL, float rough) {
    return geometrySchlickGGX(NdotV, rough) * geometrySchlickGGX(NdotL, rough);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= faceRes || id.y >= faceRes) return;

    float NdotV     = (float(id.x) + 0.5f) / float(faceRes);
    float roughness = (float(id.y) + 0.5f) / float(faceRes);

    float3 V = float3(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);
    float3 N = float3(0.0f, 0.0f, 1.0f);

    float A = 0.0f;
    float B = 0.0f;
    const uint SAMPLE_COUNT = 1024u;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        float2 Xi = hammersley(i, SAMPLE_COUNT);
        float3 H  = importanceSampleGGX(Xi, N, roughness);
        float3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V, H), 0.0f);

        if (NdotL > 0.0f) {
            float G     = geometrySmith(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc    = pow(1.0f - VdotH, 5.0f);
            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    dstLUT[id.xy] = float2(A, B) / float(SAMPLE_COUNT);
}
