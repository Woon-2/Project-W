#include "bindless.hlsli"

// 이 hlsl 코드에서 조명의 BRDF 모델은
// Cook-Torrance BRDF 모델을 따른다.

// 이 hlsl 코드에 대한 include문 전에
// 다음과 같은 멤버를 갖는 객체 material이 정의되어 전역적으로 접근이 가능함이 가정된다.
// - idxAlbedo: int4 (Albedo Map에 대한 Bindless Index)
// - idxRoughness: int4 (Roughness Map에 대한 Bindless Index)
// - idxMetallic: int4 (Metallic Map에 대한 Bindless Index)
// - cAlbedo: float4 (물체의 색상을 나타내는 상수)
// - cRoughness: float (물체의 거칠기를 나타내는 상수)
// - cMetallic: float (물체의 금속성을 나타내는 상수)
// - ao: float (물체의 주변광 차폐율을 나타내는 상수)
// - emmisive: float3 (물체의 자체발광에 대해 색상을 나타내는 상수)

#define PI 3.14159f
// 빛의 종류 목록, Light 객체의 type 멤버에 이 값들을 쓴다.
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_DIRECTIONAL 2

struct Light {
    float3 color;   // 빛의 색상
    float falloff;  // spotlight에서 중심으로부터의 각도에 따른 감쇠
    float3 posV;    // 조명의 뷰 공간 위치
    float cosTheta; // spotlight에서 조명이 비추는 최대 각도 (외부 콘 각도)
    float3 dirV;    // directional light, spotlight에서 빛의 뷰 공간 방향
    float cosPhi;   // spotlight에서 조명이 최대로 비추는 각도 (내부 콘 각도)
    float3 atten;   // 빛의 감쇠 계수
    float intensity;    // 빛의 강도
    int type;   // 빛의 종류
    int3 padding;
};

StructuredBuffer<Light> gLights : register(t1);

// 프레넬 항을 계산한다.
// @param F0: 물체를 수직에서 바라본 반사율 값
// @param HV: Halfway 벡터와 View 벡터의 내적
float3 fresnel(float3 F0, float HV) {
    // return F0 + (1.f - F0) * pow(2, (-5.55473f * HV - 6.98316f) * HV);
	return F0 + (1.f - F0)*pow(1.f - HV, 5.f);
}

// 미세면의 기울기에 대한 분산(Distribution)을 계산한다.
// @param NH: Normal 벡터와 Halfway 벡터의 내적
// @param roughness: 미세면의 거칠기
float distribute(float NH, float roughness) {
	float a = roughness * roughness;
	float a2 = a*a;

	float nom = a2;
	float denom = NH * NH * (a2 - 1.f) + 1.f;
	denom = PI * denom * denom;

	return nom / denom;
}

// 미세면의 자체 그림자 계수를 계산할 때 쓰이는,
// Schlick-GGX 근사식
// @param NV: Normal 벡터와 View 벡터의 내적
// @param roughness: 미세면의 기울기
float GeometrySchlickGGX(float NV, float roughness) {
	float r = roughness + 1.f;
	float k = (r*r) / 8.f;

	float nom = NV;
	float denom = NV * (1.f - k) + k;

	return nom / denom;
}

// 미세면의 자체 그림자 계수를 계산한다.
// @param NV: Normal 벡터와 View 벡터의 내적
// @param NL: Normal 벡터와 Light 벡터의 내적
// @param roughness: 미세면의 기울기
float GeometrySmith(float NV, float NL, float roughness) {
	float ggx2 = max(GeometrySchlickGGX(NV, roughness), 0.00002f);
	float ggx1 = GeometrySchlickGGX(NL, roughness);
	return ggx1 * ggx2;
}

// 점 조명에 대한 조명 반사 계산
float3 pointLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    // Cook-Torrance BRDF 계산을 위한 벡터와 내적값들을 마련해놓는다.
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
    F0 = lerp(F0, albedo, float3(metallic, metallic, metallic));
    
    // BRDF 계산
    float3 F = fresnel(F0, HV);
    float D = distribute(NH, roughness);
    float G = GeometrySmith(NV, NL, roughness);

    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = F * D * G / max(4 * NV * NL, 0.001f);

    // 감쇠 적용
    float atten = 1.f / dot(gLights[lightIdx].atten, float3(1.f, dist, dist * dist));

    // 조명 반사 계산
    return gLights[lightIdx].color * gLights[lightIdx].intensity * (kD + specular) * NL * atten;
}

// 방향광에 대한 조명 반사 계산
float3 dirLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    // Cook-Torrance BRDF 계산을 위한 벡터와 내적값들을 마련해놓는다.
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
    
    // BRDF 계산
    float3 F = fresnel(F0, HV);
    float D = distribute(NH, roughness);
    float G = GeometrySmith(NV, NL, roughness);

    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = F * D * G / max(4 * NV * NL, 0.0001f);

    // 조명 반사 계산 (방향광은 감쇠가 없다.)
    return gLights[lightIdx].color * gLights[lightIdx].intensity * (kD + specular) * NL;
}

// 집중조명에 대한 조명 반사 계산
float3 spotLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    // Cook-Torrance BRDF 계산을 위한 벡터와 내적값들을 마련해놓는다.
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

    // BRDF 계산
    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = F * D * G / max(4 * NV * NL, 0.001f);

    // 감쇠 적용
    float atten = 1.f / dot(gLights[lightIdx].atten, float3(1.f, dist, dist * dist));

    float cosChi = max(dot(-L, gLights[lightIdx].dirV), 0.f);

    float coneAtten = pow(
        max( (cosChi - gLights[lightIdx].cosPhi)
                / (gLights[lightIdx].cosTheta - gLights[lightIdx].cosPhi),
            0.f
        ),
        gLights[lightIdx].falloff
    );

    // 조명 반사 계산
    return gLights[lightIdx].color * gLights[lightIdx].intensity * (kD + specular) * NL * atten * coneAtten;
}

// 장면에 존재하는 모든 조명들(gLights)에 대해 조명 반사를 계산하여
// 물체의 겉보기 색상을 결정한다.
// 재질 속성 상수값의 특정 요소가 0보다 작으면, 해당 속성은 텍스처 매핑을 사용하는 것으로 간주한다.
// Albedo: material.cAlbedo.w < 0.f
// Roughness: material.cRoughness < 0.f
// Metallic: material.cMetallic < 0.f
float4 illuminate(float3 posV, float3 normalV, float2 tex)
{
    // 조명 반사 계산을 위한 변수들을 계산한다.
    float4 albedo = material.cAlbedo;
    if (albedo.w < 0.f) {
        albedo = sampleBindless(material.idxAlbedo, tex);
        // sRGB => linear
        albedo.rgb = pow( abs(albedo.rgb), 2.2f );
    }
    
    float roughness = material.cRoughness;
    if (roughness < 0.f) {
        roughness = sampleBindless(material.idxRoughness, tex);
    }
    
    float metallic = material.cMetallic;
    if (metallic < 0.f) {
        metallic = sampleBindless(material.idxMetallic, tex);
    }
    
    float ao = material.cAO;
    float3 emmisive = material.cEmmisive;

    float3 posVNormalized = normalize(posV);

    // 조명 반사값을 누적한다.
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

    float3 ambient = globalAmbient * albedo.rgb * (1.f - ao);
    color += ambient + emmisive;

    // // 톤매핑을 수행한다.
	// color = color / (color + float3(1.f, 1.f, 1.f));
    // linear => sRGB
    color = pow( abs(color), 1.f/2.2f );

    return float4(color, albedo.w);
}