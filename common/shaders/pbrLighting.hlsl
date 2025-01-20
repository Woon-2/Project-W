#include "bindless.hlsl"

#define PI 3.14159
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_DIRECTIONAL 2

#define COLOR_SPACE_SRGB 0
#define COLOR_SPACE_LINEAR 1

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

StructuredBuffer<Light> gLights : register(t1);

float3 fresnel(float3 F0, float HV) {
    // return F0 + (1.f - F0) * pow(2, (-5.55473f * HV - 6.98316f) * HV);
	return F0 + (1.f - F0)*pow(1.f - HV, 5.f);
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
    float3 V = -posVNormalized;

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

    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = F * D * G / max(4 * NV * NL, 0.001f);

    float atten = 1.f / dot(gLights[lightIdx].atten, float3(1.f, dist, dist * dist));

    return gLights[lightIdx].color * gLights[lightIdx].intensity * (kD + specular) * NL * atten;
}

float3 dirLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    float3 N = normalV;
    float3 V = -posVNormalized;
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

    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = D * F * G / max(4 * NV * NL, 0.001f);

    return gLights[lightIdx].color * gLights[lightIdx].intensity * (kD + specular) * NL;
}

float3 spotLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    float3 N = normalV;
    float3 V = -posVNormalized;

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

    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = F * D * G / max(4 * NV * NL, 0.001f);

    float atten = 1.f / dot(gLights[lightIdx].atten, float3(1.f, dist, dist * dist));

    float cosChi = max(dot(-L, gLights[lightIdx].dirV), 0.f);

    float coneAtten = pow(
        max( (cosChi - gLights[lightIdx].cosPhi)
                / (gLights[lightIdx].cosTheta - gLights[lightIdx].cosPhi),
            0.f
        ),
        gLights[lightIdx].falloff
    );

    return gLights[lightIdx].color * gLights[lightIdx].intensity * (kD + specular) * NL * atten * coneAtten;
}

float4 accumulateLighting(float3 posV, float3 normalV, float2 tex) {
    float4 albedo = material.albedoConstant * material.albedoConstantMapRatio;
    albedo += sampleFromMapRef(material.albedoMapRef, tex, samplerIdx) * (1.f - material.albedoConstantMapRatio);
    if (material.albedoMapRef.w == COLOR_SPACE_SRGB) {
        // sRGB => linear
        albedo.rgb = pow( abs(albedo.rgb), 2.2f );
    }

    float roughness = material.roughnessConstant * material.roughnessConstantMapRatio;
    float metallic = material.metallicConstant * material.metallicConstantMapRatio;

    if (material.metallicSmoothnessMapRef.x != uint(-1)) {
        float4 metallicSmoothness = sampleFromMapRef(material.metallicSmoothnessMapRef, tex, samplerIdx);
        roughness += (1.f - metallicSmoothness.a) * (1.f - material.roughnessConstantMapRatio);
        metallic += metallicSmoothness.r * (1.f - material.metallicConstantMapRatio);
    }
    else {
        roughness += sampleFromMapRef(material.roughnessMapRef, tex, samplerIdx).r * (1.f - material.roughnessConstantMapRatio);
        metallic += sampleFromMapRef(material.metallicMapRef, tex, samplerIdx).r * (1.f - material.metallicConstantMapRatio);
    }

    float ao = material.ambientOcclusionConstant * material.ambientOcclusionConstantMapRatio;
    ao += sampleFromMapRef(material.ambientOcclusionMapRef, tex, samplerIdx).r * (1.f - material.ambientOcclusionConstantMapRatio);

    float3 emmisive = material.emmisiveConstant * material.emmisiveConstantMapRatio;
    emmisive += sampleFromMapRef(material.emmisiveMapRef, tex, samplerIdx).rgb * (1.f - material.emmisiveConstantMapRatio);

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

    // tone mapping
	// color = color / (color + float3(1.f, 1.f, 1.f));
    // linear => sRGB
    color = pow( abs(color), 1.f/2.2f );

    return float4(color, albedo.w);
}

float4 illuminate(float3 posV, float4 posL, float3 normalV, float2 tex) {
    float4 color = accumulateLighting(posV, normalV, tex);

    // calculate illumination factor from shadow map
    posL.xyz /= posL.w;
    float illuminationFactor = sampleCmpFromMapRef(shadowMapRef, posL.xy, posL.z, shadowSamplerIdx).r;

    return illuminationFactor * float4(color);
    // return float4(illuminationFactor, illuminationFactor, illuminationFactor, illuminationFactor);
}