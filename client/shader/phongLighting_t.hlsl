#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

#include "textures.hlsl"

cbuffer PerFrameData : register(b1)
{
    float4 gcGlobalAmbientLight;
    uint gLightCnt;
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

float4 dirLight(uint lightIdx, float3 posVNormalized, float3 normalV, float2 tex) {
    float3 toLight = -gLights[lightIdx].dirV;
    float diffused = max(0.f, dot(normalV, toLight));
    float specular = 0.f;
            
    if (diffused > 0.f && gLights[lightIdx].shininess > 0.f) {
        float3 reflected = reflect(-toLight, normalV);
        specular = pow(max(0.f, dot(reflected, -posVNormalized)), gMaterial.shininess);
    }
        
    return gTex2DLUT[ gMaterial.diffuseMapIdx ].Sample(gSample, tex) * gLights[lightIdx].ambient +
        gTex2DLUT[ gMaterial.diffuseMapIdx ].Sample(gSample, tex) * gLights[lightIdx].diffuse * diffused +
        gTex2DLUT[ gMaterial.specularMapIdx ].Sample(gSample, tex) * float4(gLights[lightIdx].specular, 1.f) * specular;
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

    return (gTex2DLUT[ gMaterial.diffuseMapIdx ].Sample(gSample, tex) * gLights[lightIdx].ambient +
        gTex2DLUT[ gMaterial.diffuseMapIdx ].Sample(gSample, tex) * gLights[lightIdx].diffuse * diffused +
        gTex2DLUT[ gMaterial.specularMapIdx ].Sample(gSample, tex) * float4(gLights[lightIdx].specular, 1.f) * specular
    ) * atten;
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

    return (gTex2DLUT[ gMaterial.diffuseMapIdx ].Sample(gSample, tex) * gLights[lightIdx].ambient +
        gTex2DLUT[ gMaterial.diffuseMapIdx ].Sample(gSample, tex) * gLights[lightIdx].diffuse * diffused +
        gTex2DLUT[ gMaterial.specularMapIdx ].Sample(gSample, tex) * float4(gLights[lightIdx].specular, 1.f) * specular
    ) * atten * coneAtten;
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