#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

#define TEX_DIFFUSE 0x01
#define TEX_SPECULAR 0x02
#define TEX_AMBIENT 0x04
#define TEX_EMMISIVE 0x08

#include "textures.hlsl"

cbuffer PerFrameData : register(b1)
{
    matrix gView2LightProj;
    float4 gcGlobalAmbientLight;
    uint gLightCnt;
    uint gShadowMapIdx;
};

struct Light
{
    float4 ambient;
    float4 diffuse;
    float3 specular;
    float shininess;
    float3 posV;
    float falloff;
    float3 dirV;
    float cosTheta;
    float3 atten;
    float cosPhi;
    int type;
    float3 padding;
};

StructuredBuffer<Light> gLights : register(t2);
SamplerState gSample : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

float4 dirLight(uint lightIdx, float3 posVNormalized, float3 normalV, float2 tex) {
    float3 toLight = -gLights[lightIdx].dirV;
    float diffused = max(0.f, dot(normalV, toLight));
    float specular = 0.f;
            
    if (diffused > 0.f && gLights[lightIdx].shininess > 0.f) {
        float3 reflected = reflect(-toLight, normalV);
        specular = pow(max(0.f, dot(reflected, -posVNormalized)), gMaterial.shininess);
    }

    float4 color = float4(0.f, 0.f, 0.f, 0.f);
    if (gMaterial.textureFlag & TEX_AMBIENT) {
        color += gTex2DLUT[ gMaterial.ambientMapIdx ].Sample(gSample, tex) * gLights[lightIdx].ambient;
    }
    if (gMaterial.textureFlag & TEX_DIFFUSE) {
        color += gTex2DLUT[ gMaterial.diffuseMapIdx ].Sample(gSample, tex) * gLights[lightIdx].diffuse * diffused;
    }
    if (gMaterial.textureFlag & TEX_SPECULAR) {
        color += gTex2DLUT[ gMaterial.specularMapIdx ].Sample(gSample, tex) * float4(gLights[lightIdx].specular, 1.f) * specular;
    }
    if (gMaterial.textureFlag & TEX_EMMISIVE) {
        color += gTex2DLUT[ gMaterial.emmisiveMapIdx ].Sample(gSample, tex);
    }

    return color;
}

float4 pointLight(uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV, float2 tex) {
    float3 toLight = gLights[lightIdx].posV - posV;
    float dist = length(toLight);
    toLight /= dist;
    float atten = 1.f / dot(gLights[lightIdx].atten, float3(1.f, dist, dist * dist));
    float diffused = max(0.f, dot(normalV, toLight));
    float specular = 0.f;

    if (diffused > 0.f && gLights[lightIdx].shininess > 0.f) {
        float3 reflected = reflect(-toLight, normalV);
        specular = pow(max(0.f, dot(reflected, -posVNormalized)), gMaterial.shininess);
    }

    float4 color = float4(0.f, 0.f, 0.f, 0.f);
    if (gMaterial.textureFlag & TEX_AMBIENT) {
        color += gTex2DLUT[ gMaterial.ambientMapIdx ].Sample(gSample, tex) * gLights[lightIdx].ambient;
    }
    if (gMaterial.textureFlag & TEX_DIFFUSE) {
        color += gTex2DLUT[ gMaterial.diffuseMapIdx ].Sample(gSample, tex) * gLights[lightIdx].diffuse * diffused;
    }
    if (gMaterial.textureFlag & TEX_SPECULAR) {
        color += gTex2DLUT[ gMaterial.specularMapIdx ].Sample(gSample, tex) * float4(gLights[lightIdx].specular, 1.f) * specular;
    }
    if (gMaterial.textureFlag & TEX_EMMISIVE) {
        color += gTex2DLUT[ gMaterial.emmisiveMapIdx ].Sample(gSample, tex);
    }

    return color * atten;
}

float4 spotLight(uint lightIdx, float3 posV, float3 posVNormalized, float3 normalV, float2 tex) {
    float3 toLight = gLights[lightIdx].posV - posV;
    float dist = length(toLight);
    toLight /= dist;
    float atten = 1.f / dot(gLights[lightIdx].atten, float3(1.f, dist, dist * dist));
    float diffused = max(0.f, dot(normalV, toLight));
    float specular = 0.f;
    
    if (diffused > 0.f && gLights[lightIdx].shininess > 0.f) {
        float3 reflected = reflect(-toLight, normalV);
        specular = pow(max(0.f, dot(reflected, -posVNormalized)), gMaterial.shininess);
    }

    float cosChi = max(dot(-toLight, gLights[lightIdx].dirV), 0.f);

    float coneAtten = pow(
        max( (cosChi - gLights[lightIdx].cosPhi)
                / (gLights[lightIdx].cosTheta - gLights[lightIdx].cosPhi),
            0.f
        ),
        gLights[lightIdx].falloff
    );

    float4 color = float4(0.f, 0.f, 0.f, 0.f);
    if (gMaterial.textureFlag & TEX_AMBIENT) {
        color += gTex2DLUT[ gMaterial.ambientMapIdx ].Sample(gSample, tex) * gLights[lightIdx].ambient;
    }
    if (gMaterial.textureFlag & TEX_DIFFUSE) {
        color += gTex2DLUT[ gMaterial.diffuseMapIdx ].Sample(gSample, tex) * gLights[lightIdx].diffuse * diffused;
    }
    if (gMaterial.textureFlag & TEX_SPECULAR) {
        color += gTex2DLUT[ gMaterial.specularMapIdx ].Sample(gSample, tex) * float4(gLights[lightIdx].specular, 1.f) * specular;
    }
    if (gMaterial.textureFlag & TEX_EMMISIVE) {
        color += gTex2DLUT[ gMaterial.emmisiveMapIdx ].Sample(gSample, tex);
    }

    return color * atten * coneAtten;
}

float4 Lighting(float3 posV, float3 normalV, float2 tex) {
    float4 color = gTex2DLUT[ gMaterial.diffuseMapIdx ].Sample(gSample, tex) * gcGlobalAmbientLight;

    for (uint i = 0; i < gLightCnt; ++i) {
        float3 posVNormalized = normalize(posV);

        switch (gLights[i].type) {
            case LIGHT_TYPE_DIRECTIONAL:
                color += dirLight(i, posVNormalized, normalV, tex);
                break;
            case LIGHT_TYPE_POINT:
                color += pointLight(i, posV, posVNormalized, normalV, tex);
                break;
            case LIGHT_TYPE_SPOT:
                color += spotLight(i, posV, posVNormalized, normalV, tex);
                break;
        }
    }

    color.a = gTex2DLUT[ gMaterial.diffuseMapIdx ].Sample(gSample, tex).a;

    return color;
}