#include "bindless.hlsli"

// �� hlsl �ڵ忡�� ������ BRDF ����
// Cook-Torrance BRDF ���� ������.

// �� hlsl �ڵ忡 ���� include�� ���� ������ ��ü���� ���ǵǾ� ���������� ������ �������� �����ȴ�.
// {material}
// - idxAlbedo: int4 (Albedo Map�� ���� Bindless Index)
// - idxMetallicSmoothness: int4 (MetallicSmoothnessMap�� ���� Bindless Index - ����Ƽ ����)
// - idxNormal: int4 (Normal Map�� ���� Bindless Index)
// - idxEmmisive: int4 (Emmisive Map�� ���� Bindless Index)
// - idxAmbientOcclusion: int4 (Ambient Occlusion Map�� ���� Bindless Index)
// - cAlbedo: float4 (��ü�� ������ ��Ÿ���� ���)
// - cRoughness: float (��ü�� ��ĥ�⸦ ��Ÿ���� ���)
// - cMetallic: float (��ü�� �ݼӼ��� ��Ÿ���� ���)
// - cAOStrength: float (��ü�� ���� �ֺ��� ������ ���� ������ ��Ÿ���� ���)
// - cEmmisive: float3 (��ü�� ��ü�߱��� ���� ������ ��Ÿ���� ���)
// {lightCnt}: uint
// {idxShadowMap}: int4 (bindless index)

#define PI 3.14159f
// ���� ���� ���, Light ��ü�� type ����� �� ������ ����.
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_DIRECTIONAL 2

struct Light {
    float3 color;   // ���� ����
    float falloff;  // spotlight���� �߽����κ����� ������ ���� ����
    float3 posV;    // ������ �� ���� ��ġ
    float cosTheta; // spotlight���� ������ ���ߴ� �ִ� ���� (�ܺ� �� ����)
    float3 dirV;    // directional light, spotlight���� ���� �� ���� ����
    float cosPhi;   // spotlight���� ������ �ִ�� ���ߴ� ���� (���� �� ����)
    float3 atten;   // ���� ���� ���
    float intensity;    // ���� ����
    int type;   // ���� ����
    int3 padding;
};

StructuredBuffer<Light> gLightData : register(t1);

// ������ ���� ����Ѵ�.
// @param F0: ��ü�� �������� �ٶ� �ݻ��� ��
// @param HV: Halfway ���Ϳ� View ������ ����
float3 fresnel(float3 F0, float HV) {
    // return F0 + (1.f - F0) * pow(2, (-5.55473f * HV - 6.98316f) * HV);
	return F0 + (1.f - F0)*pow(1.f - HV, 5.f);
}

// �̼����� ���⿡ ���� �л�(Distribution)�� ����Ѵ�.
// @param NH: Normal ���Ϳ� Halfway ������ ����
// @param roughness: �̼����� ��ĥ��
float distribute(float NH, float roughness) {
	float a = roughness * roughness;
	float a2 = a*a;

	float nom = a2;
	float denom = NH * NH * (a2 - 1.f) + 1.f;
	denom = PI * denom * denom;

	return nom / denom;
}

// �̼����� ��ü �׸��� ����� ����� �� ���̴�,
// Schlick-GGX �ٻ��
// @param NV: Normal ���Ϳ� View ������ ����
// @param roughness: �̼����� ����
float GeometrySchlickGGX(float NV, float roughness) {
	float r = roughness + 1.f;
	float k = (r*r) / 8.f;

	float nom = NV;
	float denom = NV * (1.f - k) + k;

	return nom / denom;
}

// �̼����� ��ü �׸��� ����� ����Ѵ�.
// @param NV: Normal ���Ϳ� View ������ ����
// @param NL: Normal ���Ϳ� Light ������ ����
// @param roughness: �̼����� ����
float GeometrySmith(float NV, float NL, float roughness) {
	float ggx2 = max(GeometrySchlickGGX(NV, roughness), 0.00002f);
	float ggx1 = GeometrySchlickGGX(NL, roughness);
	return ggx1 * ggx2;
}

// �� ������ ���� ���� �ݻ� ���
float3 pointLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    // Cook-Torrance BRDF ����� ���� ���Ϳ� ���������� �����س��´�.
    float3 N = normalV;
    float3 V = -posVNormalized;

    float3 L = gLightData[lightIdx].posV - posV;
    float dist = length(L);
    L /= dist;

    float3 H = normalize(L + V);

    float LH = max(dot(L, H), 0.f);
	float NL = max(dot(L, N), 0.f);
	float NV = max(dot(N, V), 0.f);
	float HV = max(dot(H, V), 0.f);
    float NH = max(dot(N, H), 0.f);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo, float3(metallic, metallic, metallic));
    
    // BRDF ���
    float3 F = fresnel(F0, HV);
    float D = distribute(NH, roughness);
    float G = GeometrySmith(NV, NL, roughness);

    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = F * D * G / max(4 * NV * NL, 0.001f);

    // ���� ����
    float atten = 1.f / dot(gLightData[lightIdx].atten, float3(1.f, dist, dist * dist));

    // ���� �ݻ� ���
    return gLightData[lightIdx].color * gLightData[lightIdx].intensity * (kD + specular) * NL * atten;
}

// ���Ɽ�� ���� ���� �ݻ� ���
float3 dirLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    // Cook-Torrance BRDF ����� ���� ���Ϳ� ���������� �����س��´�.
    float3 N = normalV;
    float3 V = -posVNormalized;
    float3 L = -gLightData[lightIdx].dirV;

    float3 H = normalize(L + V);

    float LH = max(dot(L, H), 0.f);
	float NL = max(dot(L, N), 0.f);
	float NV = max(dot(N, V), 0.f);
	float HV = max(dot(H, V), 0.f);
    float NH = max(dot(N, H), 0.f);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo, float3(metallic, metallic, metallic)); // use metalic value to get F
    
    // BRDF ���
    float3 F = fresnel(F0, HV);
    float D = distribute(NH, roughness);
    float G = GeometrySmith(NV, NL, roughness);

    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = F * D * G / max(4 * NV * NL, 0.0001f);

    // ���� �ݻ� ��� (���Ɽ�� ���谡 ����.)
    return gLightData[lightIdx].color * gLightData[lightIdx].intensity * (kD + specular) * NL;
}

// ���������� ���� ���� �ݻ� ���
float3 spotLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    // Cook-Torrance BRDF ����� ���� ���Ϳ� ���������� �����س��´�.
    float3 N = normalV;
    float3 V = -posVNormalized;

    float3 L = gLightData[lightIdx].posV - posV;
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

    // BRDF ���
    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = F * D * G / max(4 * NV * NL, 0.001f);

    // ���� ����
    float atten = 1.f / dot(gLightData[lightIdx].atten, float3(1.f, dist, dist * dist));

    float cosChi = max(dot(-L, gLightData[lightIdx].dirV), 0.f);

    float coneAtten = pow(
        max( (cosChi - gLightData[lightIdx].cosPhi)
                / (gLightData[lightIdx].cosTheta - gLightData[lightIdx].cosPhi),
            0.f
        ),
        gLightData[lightIdx].falloff
    );

    // ���� �ݻ� ���
    return gLightData[lightIdx].color * gLightData[lightIdx].intensity * (kD + specular) * NL * atten * coneAtten;
}

float PCF(int4 idx, float4 posL) {
    posL.xyz /= posL.w;
    
    posL.z = min(posL.z, 1.0f);
    
    float p00 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(-1, -1)).r;
    float p01 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(-1, 0)).r;
    float p02 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(-1, 1)).r;
    float p10 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(0, -1)).r;
    float p11 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(0, 0)).r;
    float p12 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(0, 1)).r;
    float p20 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(1, -1)).r;
    float p21 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(1, 0)).r;
    float p22 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(1, 1)).r;
    
    return (p00 + p01 + p02 + p10 + p11 + p12 + p20 + p21 + p22) / 9.f;
}

#ifdef SINGLE_SHADOW
float calcSingleShadow(float3 posV, float4 posL) {
    return PCF(idxShadowMap, posL);
}
#endif

// ---------------------------------------------------------------------------
// CSM shadow functions (separate Texture2D per cascade, 9-tap PCF)
// ---------------------------------------------------------------------------
#ifndef MAX_CSM_CASCADES
#define MAX_CSM_CASCADES 4
#endif

// calcCSMShadow: selects cascade by view-space depth, then performs 9-tap PCF.
// posV: view-space position, posW: world-space position.
// Uses cbuffer vars: cascadeCount, cascadeSplitsFarV, lightVP[], idxShadowMap[].
// Uses gmtxTexturize (declared in the including .hlsl before this include).
float calcCSMShadow(float3 posV, float3 posW) {
    // Select cascade: find first cascade whose far depth exceeds |posV.z|
    uint cascadeIdx = cascadeCount - 1u;
    float splits[4] = {
        cascadeSplitsFarV.x, cascadeSplitsFarV.y,
        cascadeSplitsFarV.z, cascadeSplitsFarV.w
    };
    [unroll]
    for (uint ci = 0u; ci < cascadeCount; ++ci) {
        if (posV.z < splits[ci]) { cascadeIdx = ci; break; }
    }

    // Project world-space position into the selected cascade's light space
    float4 posL = mul(mul(float4(posW, 1.f), lightVP[cascadeIdx]), gmtxTexturize);
    posL.xyz /= posL.w;
    posL.z = min(posL.z, 1.0f);

    // 9-tap PCF on the cascade's dedicated Texture2D (idxShadowMap[cascadeIdx])
    int4 idx = idxShadowMap[cascadeIdx];
    float p00 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(-1, -1)).r;
    float p01 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(-1,  0)).r;
    float p02 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2(-1,  1)).r;
    float p10 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2( 0, -1)).r;
    float p11 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2( 0,  0)).r;
    float p12 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2( 0,  1)).r;
    float p20 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2( 1, -1)).r;
    float p21 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2( 1,  0)).r;
    float p22 = sampleCmpBindless2DOffset(idx, posL.xy, posL.z, int2( 1,  1)).r;
    return (p00 + p01 + p02 + p10 + p11 + p12 + p20 + p21 + p22) / 9.f;
}

#ifndef TERRAIN_SHADER

#ifdef SINGLE_SHADOW
// ��鿡 �����ϴ� ��� ������(gLightData)�� ���� ���� �ݻ縦 ����Ͽ�
// ��ü�� �Ѻ��� ������ �����Ѵ�.
float4 illuminate(float3 posV, float4 posL, float3 normalV, float2 tex) {
    // ���� �ݻ� ����� ���� �������� ����Ѵ�.
    float4 albedo = material.cAlbedo;
    // bindless index�� ������ ���� �ؽ�ó�� ������ �ǹ��Ѵ�.
    // ���� bindless index�� ����� ���� �ؽ�ó�� ���ø��Ѵ�.
    if (material.idxAlbedo.x >= 0) {
        albedo = sampleBindless(material.idxAlbedo, tex);
    }
    // sRGB => linear
    albedo.rgb = pow( abs(albedo.rgb), 2.2f );
    
    float roughness = material.cRoughness;
    float metallic = material.cMetallic;
    if (material.idxMetallicSmoothness.x >= 0) {
        // ����Ƽ �ͽ����͸� ����ϹǷ� ����Ƽ �ؽ�ó ������ �����Ѵ�.
        // ����Ƽ���� metallicSmoothness �ؽ�ó��
        // rä�ο� metallic ��, aä�ο� smoothness ���� �����Ѵ�.
        float4 metallicSmoothness = sampleBindless(material.idxMetallicSmoothness, tex);
        metallic = metallicSmoothness.r;
        roughness = 1.f - metallicSmoothness.a; // roughness = 1.f - smoothness
    }
    
    float ao = 0.f;
    if (material.idxAmbientOcclusion.x >= 0) {
        ao = material.cAOStrength * sampleBindless(material.idxAmbientOcclusion, tex).r;
    }

    float3 emmisive = material.cEmmisive;
    if (material.idxEmmisive.x >= 0) {
        emmisive = sampleBindless(material.idxEmmisive, tex).rgb;
    }

    float3 posVNormalized = normalize(posV);

    // ���� �ݻ簪�� �����Ѵ�.
    float3 color = float3(0.f, 0.f, 0.f);

    for (uint i = 0; i < lightCnt; i++) {
        if (gLightData[i].type == LIGHT_TYPE_POINT) {
            color += pointLight(i, posV, posVNormalized, normalV, tex, albedo.rgb, roughness, metallic, ao);
        } else if (gLightData[i].type == LIGHT_TYPE_SPOT) {
            color += spotLight(i, posV, posVNormalized, normalV, tex, albedo.rgb, roughness, metallic, ao);
        } else if (gLightData[i].type == LIGHT_TYPE_DIRECTIONAL) {
            color += dirLight(i, posV, posVNormalized, normalV, tex, albedo.rgb, roughness, metallic, ao);
        }
    }
    
    float directFactor = calcSingleShadow(posV, posL);
    color *= directFactor;

    float3 ambient = globalAmbient * albedo.rgb * (1.f - ao);
    color += ambient + emmisive;

    // ������� �����Ѵ�.
	color = color / (color + float3(1.f, 1.f, 1.f));
    // linear => sRGB
    color = pow( abs(color), 1.f/2.2f );

    posL.xyz /= posL.w;
    posL.z = min(posL.z, 1.0f);

    return float4(color, albedo.w);
}
#endif

// illuminateCSM: CSM(Cascaded Shadow Map) 버전. illuminate()와 별도로 유지.
// posW: world-space position (cascade 선택 후 light-space 변환에 사용)
float4 illuminateCSM(float3 posV, float3 posW, float3 normalV, float2 tex) {
    float4 albedo = material.cAlbedo;
    if (material.idxAlbedo.x >= 0) {
        albedo = sampleBindless(material.idxAlbedo, tex);
    }
    albedo.rgb = pow( abs(albedo.rgb), 2.2f );

    float roughness = material.cRoughness;
    float metallic = material.cMetallic;
    if (material.idxMetallicSmoothness.x >= 0) {
        float4 metallicSmoothness = sampleBindless(material.idxMetallicSmoothness, tex);
        metallic = metallicSmoothness.r;
        roughness = 1.f - metallicSmoothness.a;
    }

    float ao = 0.f;
    if (material.idxAmbientOcclusion.x >= 0) {
        ao = material.cAOStrength * sampleBindless(material.idxAmbientOcclusion, tex).r;
    }

    float3 emmisive = material.cEmmisive;
    if (material.idxEmmisive.x >= 0) {
        emmisive = sampleBindless(material.idxEmmisive, tex).rgb;
    }

    float3 posVNormalized = normalize(posV);

    float3 color = float3(0.f, 0.f, 0.f);
    for (uint i = 0; i < lightCnt; i++) {
        if (gLightData[i].type == LIGHT_TYPE_POINT) {
            color += pointLight(i, posV, posVNormalized, normalV, tex, albedo.rgb, roughness, metallic, ao);
        } else if (gLightData[i].type == LIGHT_TYPE_SPOT) {
            color += spotLight(i, posV, posVNormalized, normalV, tex, albedo.rgb, roughness, metallic, ao);
        } else if (gLightData[i].type == LIGHT_TYPE_DIRECTIONAL) {
            color += dirLight(i, posV, posVNormalized, normalV, tex, albedo.rgb, roughness, metallic, ao);
        }
    }

    float directFactor = calcCSMShadow(posV, posW);
    color *= directFactor;

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
            if (posV.z < dbgSplits[dci]) { dbgCascade = dci; break; }
        }
        color.rgb = lerp(color.rgb, kCascadeColors[dbgCascade], 0.4f);
    }
#endif

    float3 ambient = globalAmbient * albedo.rgb * (1.f - ao);
    color += ambient + emmisive;

    color = color / (color + float3(1.f, 1.f, 1.f));
    color = pow( abs(color), 1.f/2.2f );

    return float4(color, albedo.w);
}
#endif // TERRAIN_SHADER