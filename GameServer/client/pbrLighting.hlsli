#include "bindless.hlsli"

// Oct encoding for view-space unit normals → 2-channel float2 [0,1]
float2 octEncode(float3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    if (n.z < 0.0f) n.xy = (1.0f - abs(n.yx)) * sign(n.xy);
    return n.xy * 0.5f + 0.5f;
}

// Oct decoding: 2-channel float2 [0,1] → view-space unit normal
float3 octDecode(float2 oct) {
    oct = oct * 2.0f - 1.0f;
    float3 n = float3(oct, 1.0f - abs(oct.x) - abs(oct.y));
    if (n.z < 0.0f) n.xy = (1.0f - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}

//// - worldPos : 픽셀의 월드 좌표
//// - camPos : 카메라의 월드 좌표
//// - fogDensity : 전체적인 안개 강도, 값이 클수록 멀리 있는 객체가 안 보임
//// - heightFalloff : 높이에 따른 안개 감소 속도, 값이 클수록 높이 차가 큰 지역에 대한 안개가 짙어짐
//// - fogBaseHeight : 안개가 시작되는 기준 높이, 지면이나 해수면 등의 높이로 설정하기
//float computeExponentialHeightFog(float3 worldPos, float3 camPos, float fogDensity, float heightFalloff, float fogBaseHeight) {
//    float3 ray = worldPos - camPos;
//    float dist = length(ray);
//    float3 dir = ray / dist;
    
//    float camHeight = camPos.y;
//    float pixHeight = worldPos.y;
    
//    float heightDiff = pixHeight - camHeight;
    
//    float fogDensityHeight = exp(-heightFalloff * (camHeight - fogBaseHeight));
    
//    float fogFactor = fogDensityHeight * (1 - exp(-fogDensity * dist)) / (heightFalloff * max(abs(dir.y), 0.001f));
//    return saturate(fogFactor);
//}

float computeExponentialHeightFog(float3 worldPos, float3 camPos, float fogDensity, float heightFalloff, float fogBaseHeight) {
    float3 ray = worldPos - camPos;
    float dist = length(ray);
    float3 dir = ray / dist;

    // 1. 카메라와 픽셀의 상대적 높이 계산 (Base Height 기준)
    float relativeCamHeight = camPos.y - fogBaseHeight;
    
    // 2. 적분 공식의 핵심 파트
    // Falloff가 적용된 카메라 지점의 안개 농도
    float fogAtStart = fogDensity * exp(-heightFalloff * relativeCamHeight);

    // 3. 수평 방향(dir.y 가 0에 가까울 때)의 제로 디비전 방지 및 정밀도 확보
    // dir.y가 매우 작으면 높이 변화가 없으므로 단순히 (농도 * 거리)로 수렴함
    float slope = heightFalloff * dir.y;
    float precisionThreshold = 0.0001f;
    float fogIntegral;

    if (abs(slope) > precisionThreshold) {
        // 표준적인 지수 높이 안개 적분 공식
        fogIntegral = fogAtStart * (1.0f - exp(-slope * dist)) / slope;
    } else {
        // 수평에 가까울 경우 선형 근사 (L'Hopital's rule 적용 결과와 같음)
        fogIntegral = fogAtStart * dist;
    }

    return saturate(fogIntegral);
}

// 이 hlsl 코드에서 조명의 BRDF 모델은
// Cook-Torrance BRDF 모델을 따른다.

// 이 hlsl 코드에 대한 include문 전에 다음의 객체들이 정의되어 전역적으로 접근이 가능함이 가정된다.
// {material}
// - idxAlbedo: int4 (Albedo Map에 대한 Bindless Index)
// - idxMetallicSmoothness: int4 (MetallicSmoothnessMap에 대한 Bindless Index - 유니티 대응)
// - idxNormal: int4 (Normal Map에 대한 Bindless Index)
// - idxEmmisive: int4 (Emmisive Map에 대한 Bindless Index)
// - idxAmbientOcclusion: int4 (Ambient Occlusion Map에 대한 Bindless Index)
// - cAlbedo: float4 (물체의 색상을 나타내는 상수)
// - cRoughness: float (물체의 거칠기를 나타내는 상수)
// - cMetallic: float (물체의 금속성을 나타내는 상수)
// - cAOStrength: float (물체에 대해 주변광 차폐의 적용 강도를 나타내는 상수)
// - cEmmisive: float3 (물체의 자체발광에 대해 색상을 나타내는 상수)
// {lightCnt}: uint
// {idxShadowMap}: int4 (bindless index)

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

StructuredBuffer<Light> gLightData : register(t1);

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
    
    // BRDF 계산
    float3 F = fresnel(F0, HV);
    float D = distribute(NH, roughness);
    float G = GeometrySmith(NV, NL, roughness);

    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = F * D * G / max(4 * NV * NL, 0.001f);

    // 감쇠 적용
    float atten = 1.f / dot(gLightData[lightIdx].atten, float3(1.f, dist, dist * dist));

    // 조명 반사 계산
    return gLightData[lightIdx].color * gLightData[lightIdx].intensity * (kD + specular) * NL * atten;
}

// 방향광에 대한 조명 반사 계산
float3 dirLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    // Cook-Torrance BRDF 계산을 위한 벡터와 내적값들을 마련해놓는다.
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
    
    // BRDF 계산
    float3 F = fresnel(F0, HV);
    float D = distribute(NH, roughness);
    float G = GeometrySmith(NV, NL, roughness);

    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = F * D * G / max(4 * NV * NL, 0.0001f);

    // 조명 반사 계산 (방향광은 감쇠가 없다.)
    return gLightData[lightIdx].color * gLightData[lightIdx].intensity * (kD + specular) * NL;
}

// 집중조명에 대한 조명 반사 계산
float3 spotLight( uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV,
    float2 tex, float3 albedo, float roughness, float metallic, float ao
) {
    // Cook-Torrance BRDF 계산을 위한 벡터와 내적값들을 마련해놓는다.
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

    // BRDF 계산
    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - metallic;
    kD *= albedo / PI;
    float3 specular = F * D * G / max(4 * NV * NL, 0.001f);

    // 감쇠 적용
    float atten = 1.f / dot(gLightData[lightIdx].atten, float3(1.f, dist, dist * dist));

    float cosChi = max(dot(-L, gLightData[lightIdx].dirV), 0.f);

    float coneAtten = pow(
        max( (cosChi - gLightData[lightIdx].cosPhi)
                / (gLightData[lightIdx].cosTheta - gLightData[lightIdx].cosPhi),
            0.f
        ),
        gLightData[lightIdx].falloff
    );

    // 조명 반사 계산
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

float sampleCascadePCF(uint cascadeIdx, float3 posW, float3 normalW, float sinTheta, float offsets[4]) {
    // adjust the offset of selected cascade
    float3 biasedPosW = posW + normalize(normalW) * offsets[cascadeIdx] * sinTheta;

    // project to light space of the selected cascade
    float4 posL = mul(mul(float4(biasedPosW, 1.f), lightVP[cascadeIdx]), gmtxTexturize);
    posL.xyz /= posL.w;
    posL.z = min(posL.z, 1.0f);

    // 9-tap PCF
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

// calcCSMShadow: selects related cascades by view-space depth, performs 9-tap PCF,
//                then blends the results.
float calcCSMShadow(float3 posV, float3 posW, float3 normalW, float rawNdotl) {
    // No-shadow path (e.g. lobby portrait): cascadeCount == 0 means no CSM bound.
    // Skip early to avoid `cascadeCount - 1u` unsigned underflow and OOB cascade sampling.
    if (cascadeCount == 0u) return 1.0f;

    float splits[4] = {
        cascadeSplitsFarV.x, cascadeSplitsFarV.y,
        cascadeSplitsFarV.z, cascadeSplitsFarV.w
    };

    // Select cascade: find first cascade whose far depth exceeds |posV.z|
    uint cascadeIdx = cascadeCount - 1u;
    for (uint ci = 0u; ci < cascadeCount; ++ci) {
        if (posV.z < splits[ci]) { 
            cascadeIdx = ci; 
            break; 
        }
    }

    // NdotL-adaptive normal offset: maximum when surface is perpendicular to light,
    // zero when surface faces light directly.
    // Back-lit faces (rawNdotl < 0) get zero offset — offsetting away from the sun
    // would push biasedPosL.z shallower than the shadow map depth, incorrectly
    // marking the surface as lit and causing view-direction-dependent shadow flicker.
    float offsets[4] = {
        cascadeNormalOffsets.x, cascadeNormalOffsets.y,
        cascadeNormalOffsets.z, cascadeNormalOffsets.w
    };
    float sinTheta = (rawNdotl > 0.f) ? sqrt(max(0.f, 1.f - rawNdotl * rawNdotl)) : 0.f;

    // main cascade sampling
    float shadow = sampleCascadePCF(cascadeIdx, posW, normalW, sinTheta, offsets);

    // --- Cascade Blending logic ---
    // regard blending with next cascade only if the cascade is not the last cascade.
    if (cascadeIdx < cascadeCount - 1u) {
        // the bandwidth of blending range (view space unit)
        // for now, 15% of split distance
        float blendBand = splits[cascadeIdx] * 0.15f; 
        
        float splitDist = splits[cascadeIdx];
        float blendStart = splitDist - blendBand;
        
        // check if the depth is in the blending range
        if (posV.z > blendStart) {
            // calculate the blending factor, between 0.0 (blendStart) ~ 1.0 (splitDist).
            float blendFactor = smoothstep(blendStart, splitDist, posV.z);
            
            // sample the next cascade
            uint nextCascadeIdx = cascadeIdx + 1u;
            float nextShadow = sampleCascadePCF(nextCascadeIdx, posW, normalW, sinTheta, offsets);
            
            // softly blend the two different cascades.
            shadow = lerp(shadow, nextShadow, blendFactor);
        }
    }

    return shadow;
}

#ifndef TERRAIN_SHADER

#ifdef SINGLE_SHADOW
// 장면에 존재하는 모든 조명들(gLightData)에 대해 조명 반사를 계산하여
// 물체의 겉보기 색상을 결정한다.
float4 illuminate(float3 posV, float4 posL, float3 normalV, float2 tex) {
    // 조명 반사 계산을 위한 변수들을 계산한다.
    float4 albedo = material.cAlbedo;
    // bindless index가 음수인 것은 텍스처가 없음을 의미한다.
    // 따라서 bindless index가 양수일 때만 텍스처를 샘플링한다.
    if (material.idxAlbedo.x >= 0) {
        albedo = sampleBindless(material.idxAlbedo, tex);
    }
    // sRGB => linear
    albedo.rgb = pow( abs(albedo.rgb), 2.2f );
    
    float roughness = material.cRoughness;
    float metallic = material.cMetallic;
    if (material.idxMetallicSmoothness.x >= 0) {
        // 유니티 익스포터를 사용하므로 유니티 텍스처 포맷을 상정한다.
        // 유니티에서 metallicSmoothness 텍스처는
        // r채널에 metallic 값, a채널에 smoothness 값을 저장한다.
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

    // 조명 반사값을 누적한다.
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

    // 톤매핑을 수행한다.
	color = color / (color + float3(1.f, 1.f, 1.f));
    // linear => sRGB
    color = pow( abs(color), 1.f/2.2f );
    
    posL.xyz /= posL.w;
    posL.z = min(posL.z, 1.0f);
    
    return float4(color, albedo.w);
}
#endif

#ifndef DEFERRED_LIGHTING_PASS
// illuminateCSM: CSM(Cascaded Shadow Map) 버전. illuminate()와 별도로 유지.
// posW: world-space position (cascade 선택 후 light-space 변환에 사용)
// normalW: world-space geometric normal (normal offset shadow bias용)
float4 illuminateCSM(float3 posV, float3 posW, float3 normalV, float2 tex, float3 normalW) {
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

    // Compute raw (unsaturated) NdotL for the main directional light.
    // Must NOT be saturated: back-lit faces need rawNdotl < 0 so calcCSMShadow
    // applies zero normal offset (saturating to 0 would give sinTheta=1 → max offset
    // → biasedPosL.z shallower than shadow map → surface incorrectly marked as lit).
    float rawNdotl_shadow = 0.5f;
    for (uint si = 0u; si < lightCnt; ++si) {
        if (gLightData[si].type == LIGHT_TYPE_DIRECTIONAL) {
            rawNdotl_shadow = dot(normalV, -gLightData[si].dirV);
            break;
        }
    }
    float directFactor = calcCSMShadow(posV, posW, normalW, rawNdotl_shadow);
    color *= directFactor;

#ifdef CSM_DEBUG_VIS
    if (cascadeCount > 0u) {
        static const float3 kCascadeColors[4] = {
            float3(1,0,0), float3(0,1,0), float3(0,0,1), float3(1,1,0)
        };
        uint dbgCascade = cascadeCount - 1u;
        float dbgSplits[4] = {
            cascadeSplitsFarV.x, cascadeSplitsFarV.y,
            cascadeSplitsFarV.z, cascadeSplitsFarV.w
        };
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
#endif // DEFERRED_LIGHTING_PASS

#endif // TERRAIN_SHADER

#ifdef DEFERRED_LIGHTING_PASS
// illuminateFromGBuffer: deferred lighting pass에서 GBuffer로부터 직접 조명을 계산한다.
// Returns direct lighting * shadow only (pre-tonemap, pre-ambient/emissive).
// Caller adds precompLight (GB2.rgb = ambient+emissive) then tonemaps the combined result.
float3 illuminateFromGBuffer(
    float3 posV, float3 posW,
    float3 normalV, float3 normalW,
    float3 albedo, float roughness, float metallic, float ao
) {
    float3 posVNormalized = normalize(posV);
    float3 color = float3(0.f, 0.f, 0.f);
    for (uint i = 0u; i < lightCnt; i++) {
        if (gLightData[i].type == LIGHT_TYPE_POINT)
            color += pointLight(i, posV, posVNormalized, normalV, float2(0.f, 0.f), albedo, roughness, metallic, ao);
        else if (gLightData[i].type == LIGHT_TYPE_SPOT)
            color += spotLight(i, posV, posVNormalized, normalV, float2(0.f, 0.f), albedo, roughness, metallic, ao);
        else if (gLightData[i].type == LIGHT_TYPE_DIRECTIONAL)
            color += dirLight(i, posV, posVNormalized, normalV, float2(0.f, 0.f), albedo, roughness, metallic, ao);
    }

    float rawNdotl = 0.5f;
    for (uint si = 0u; si < lightCnt; ++si) {
        if (gLightData[si].type == LIGHT_TYPE_DIRECTIONAL) {
            rawNdotl = dot(normalV, -gLightData[si].dirV);
            break;
        }
    }
    return color * calcCSMShadow(posV, posW, normalW, rawNdotl);
}
#endif // DEFERRED_LIGHTING_PASS