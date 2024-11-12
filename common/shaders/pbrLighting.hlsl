#include "bindless.hlsl"
#include "samplers.hlsl"

#define PI 3.14159
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_DIRECTIONAL 2

#define MAP_TYPE_TEXTURE2D 0
#define MAP_TYPE_TEXTUREARRAY 1
#define MAP_TYPE_TEXTURECUBE 2

struct Light {
    float3 color;
    float falloff;
    float3 posV;
    float cosTheta;
    float3 dirV;
    float cosPhi;
    float3 atten;
    float intensity;
    int type;
    int3 padding;
};

// MapRef.x: resource type, MapRef.y: resource index, MapRef.z: array index

struct Material {
    float4 albedoConstant;
    float roughnessConstant;
    float metallicConstant;
    float albedoConstantMapRatio;
    float roughnessConstantMapRatio;
    float metallicConstantMapRatio;
    float3 emmisiveConstant;
    float emmisiveConstantMapRatio;
    float ambientOcclusionConstant;
    float ambientOcclusionConstantMapRatio;
    float padding;
    uint4 albedoMapRef;
    uint4 roughnessMapRef;
    uint4 normalMapRef;
    uint4 metallicMapRef;
    uint4 emmisiveMapRef;
    uint4 ambientOcclusionMapRef;
};

StructuredBuffer<Light> gLights : register(t1);

cbuffer PerDrawcallData : register(b1) {
    Material material;
    uint instanceBase;
    uint samplerIdx;
    uint2 padding;
};

cbuffer PerFrameData : register(b2) {
    float3 globalAmbient;
    float padding0;
    uint lightCnt;
    uint3 padding1;
};

float3 fresnel(float3 F0, float HV) {
    return F0 + (1.f - F0) * pow(2, (-5.55473f * HV - 6.98316f) * HV);
	// return F0 + (1.f - F0)*pow(1 - HV, 5.f);
}

float distribute(float NH, float roughness) {
	float a = roughness * roughness;
	float a2 = a*a;

	float nom = a2;
	float denom = NH * NH * (a2 - 1.f) + 1.f;
	denom = PI * denom * denom;

	return nom / denom;
}

float GeometrySchlickGGX(float NV, float roughness) {
	float r = (roughness + 1.f);
	float k = (r*r) / 8.f;

	float nom = NV;
	float denom = NV * (1.f - k) + k;

	return nom / denom;
}

float GeometrySmith(float NV, float NL, float roughness) {
	float ggx2 = GeometrySchlickGGX(NV, roughness);
	float ggx1 = GeometrySchlickGGX(NL, roughness);
	return ggx1 * ggx2;
}

float3 pointLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    float3 N = normalV;
    float3 V = posVNormalized;

    float3 L = gLights[lightIdx].posV - posV;
    float dist = length(L);
    L /= dist;

    float3 H = normalize(L + V);

    float LH = max(dot(L, H), 0.f);
	float NL = max(dot(L, N), 0.f);
	float NV = max(dot(N, V), 0.f);
	float HV = max(dot(H, V), 0.f);
    float NH = max(dot(N, H), 0.f);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo, float3(metallic, metallic, metallic)); // use metalic value to get F
    float3 F = fresnel(F0, HV);
    float D = distribute(NH, roughness);
    float G = GeometrySmith(NV, NL, roughness);

    float3 kD = (1.f - F);
    kD *= 1.f - kD;
    kD *= albedo / PI;
    float3 kS = F * D * G / max(4 * NV * NL, 0.001f);

    float atten = 1.f / dot(gLights[lightIdx].atten, float3(1.f, dist, dist * dist));

    return gLights[lightIdx].color * gLights[lightIdx].intensity * (kD + kS) * atten;
}

float3 dirLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    float3 N = normalV;
    float3 V = posVNormalized;
    float3 L = -gLights[lightIdx].dirV;

    float3 H = normalize(L + V);

    float LH = max(dot(L, H), 0.f);
	float NL = max(dot(L, N), 0.f);
	float NV = max(dot(N, V), 0.f);
	float HV = max(dot(H, V), 0.f);
    float NH = max(dot(N, H), 0.f);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo, float3(metallic, metallic, metallic)); // use metalic value to get F
    float3 F = fresnel(F0, HV);
    float D = distribute(NH, roughness);
    float G = GeometrySmith(NV, NL, roughness);

    float3 kD = (1.f - F);
    kD *= 1.f - kD;
    kD *= albedo / PI;
    float3 kS = F * D * G / max(4 * NV * NL, 0.001f);

    return gLights[lightIdx].color * gLights[lightIdx].intensity * (kD + kS);
}

float3 spotLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    float3 N = normalV;
    float3 V = posVNormalized;

    float3 L = gLights[lightIdx].posV - posV;
    float dist = length(L);
    L /= dist;

    float3 H = normalize(L + V);

    float LH = max(dot(L, H), 0.f);
	float NL = max(dot(L, N), 0.f);
	float NV = max(dot(N, V), 0.f);
	float HV = max(dot(H, V), 0.f);
    float NH = max(dot(N, H), 0.f);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo, float3(metallic, metallic, metallic)); // use metalic value to get F
    float3 F = fresnel(F0, HV);
    float D = distribute(NH, roughness);
    float G = GeometrySmith(NV, NL, roughness);

    float3 kD = (1.f - F);
    kD *= 1.f - kD;
    kD *= albedo / PI;
    float3 kS = F * D * G / max(4 * NV * NL, 0.001f);

    float atten = 1.f / dot(gLights[lightIdx].atten, float3(1.f, dist, dist * dist));

    float cosChi = max(dot(-L, gLights[lightIdx].dirV), 0.f);

    float coneAtten = pow(
        max( (cosChi - gLights[lightIdx].cosPhi)
                / (gLights[lightIdx].cosTheta - gLights[lightIdx].cosPhi),
            0.f
        ),
        gLights[lightIdx].falloff
    );

    return gLights[lightIdx].color * gLights[lightIdx].intensity * (kD + kS) * atten * coneAtten;
}

float4 illuminate(float3 posV, float3 normalV, float2 tex) {
    float4 albedo = material.albedoConstant * material.albedoConstantMapRatio;
    if (material.albedoMapRef.x != uint(-1)) {
        if (material.albedoMapRef.x == MAP_TYPE_TEXTURE2D) {
            albedo = gTex2Ds[material.albedoMapRef.y].Sample(gSamplers[samplerIdx], tex);
        } else if (material.albedoMapRef.x == MAP_TYPE_TEXTUREARRAY) {
            albedo = gTex2DArrays[material.albedoMapRef.y].Sample(gSamplers[samplerIdx], float3(tex, material.albedoMapRef.z));
        } else /* if (material.albedoMapRef.x == MAP_TYPE_TEXTURECUBE) */ {
            // albedo = gTexCubes[material.albedoMapRef.y].Sample(...);
        }
    }
    float roughness = material.roughnessConstant * material.roughnessConstantMapRatio;
    if (material.roughnessMapRef.x != uint(-1)) {
        if (material.roughnessMapRef.x == MAP_TYPE_TEXTURE2D) {
            roughness = gTex2Ds[material.roughnessMapRef.y].Sample(gSamplers[samplerIdx], tex).r;
        } else if (material.roughnessMapRef.x == MAP_TYPE_TEXTUREARRAY) {
            roughness = gTex2DArrays[material.roughnessMapRef.y].Sample(gSamplers[samplerIdx], float3(tex, material.roughnessMapRef.z)).r;
        } else /* if (material.roughnessMapRef.x == MAP_TYPE_TEXTURECUBE) */ {
            // roughness = gTexCubes[material.roughnessMapRef.y].Sample(...);
        }
    }
    float metallic = material.metallicConstant * material.metallicConstantMapRatio;
    if (material.metallicMapRef.x != uint(-1)) {
        if (material.metallicMapRef.x == MAP_TYPE_TEXTURE2D) {
            metallic = gTex2Ds[material.metallicMapRef.y].Sample(gSamplers[samplerIdx], tex).r;
        } else if (material.metallicMapRef.x == MAP_TYPE_TEXTUREARRAY) {
            metallic = gTex2DArrays[material.metallicMapRef.y].Sample(gSamplers[samplerIdx], float3(tex, material.metallicMapRef.z)).r;
        } else /* if (material.metallicMapRef.x == MAP_TYPE_TEXTURECUBE) */ {
            // metallic = gTexCubes[material.metallicMapRef.y].Sample(...);
        }
    }
    float ao = material.ambientOcclusionConstant * material.ambientOcclusionConstantMapRatio;
    if (material.ambientOcclusionMapRef.x != uint(-1)) {
        if (material.ambientOcclusionMapRef.x == MAP_TYPE_TEXTURE2D) {
            ao = gTex2Ds[material.ambientOcclusionMapRef.y].Sample(gSamplers[samplerIdx], tex).r;
        } else if (material.ambientOcclusionMapRef.x == MAP_TYPE_TEXTUREARRAY) {
            ao = gTex2DArrays[material.ambientOcclusionMapRef.y].Sample(gSamplers[samplerIdx], float3(tex, material.ambientOcclusionMapRef.z)).r;
        } else /* if (material.ambientOcclusionMapRef.x == MAP_TYPE_TEXTURECUBE) */ {
            // ao = gTexCubes[material.ambientOcclusionMapRef.y].Sample(...);
        }
    }

    float3 emmisive = material.emmisiveConstant * material.emmisiveConstantMapRatio;
    if (material.emmisiveMapRef.x != uint(-1)) {
        if (material.emmisiveMapRef.x == MAP_TYPE_TEXTURE2D) {
            emmisive = gTex2Ds[material.emmisiveMapRef.y].Sample(gSamplers[samplerIdx], tex).rgb;
        } else if (material.emmisiveMapRef.x == MAP_TYPE_TEXTUREARRAY) {
            emmisive = gTex2DArrays[material.emmisiveMapRef.y].Sample(gSamplers[samplerIdx], float3(tex, material.emmisiveMapRef.z)).rgb;
        } else /* if (material.emmisiveMapRef.x == MAP_TYPE_TEXTURECUBE) */ {
            // emmisive = gTexCubes[material.emmisiveMapRef.y].Sample(...);
        }
    }

    float3 posVNormalized = normalize(posV);

    float3 color = float3(0.f, 0.f, 0.f);

    for (uint i = 0; i < lightCnt; i++) {
        if (gLights[i].type == LIGHT_TYPE_POINT) {
            color += pointLight(i, posV, posVNormalized, normalV, tex, albedo.rgb, roughness, metallic, ao);
        } else if (gLights[i].type == LIGHT_TYPE_SPOT) {
            color += spotLight(i, posV, posVNormalized, normalV, tex, albedo.rgb, roughness, metallic, ao);
        } else if (gLights[i].type == LIGHT_TYPE_DIRECTIONAL) {
            color += dirLight(i, posV, posVNormalized, normalV, tex, albedo.rgb, roughness, metallic, ao);
        }
    }

    float3 ambient = globalAmbient * albedo.rgb * ao;
    color += ambient + emmisive;

    return float4(color, albedo.w);
}