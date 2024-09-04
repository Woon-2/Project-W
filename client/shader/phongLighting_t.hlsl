#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

cbuffer PerFrameData : register(b1)
{
    float4 gcGlobalAmbientLight;
    uint gLightCnt;
};

struct Material
{
    Texture2D diffuseMap;
    Texture2D specularMap;
};

struct Light
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float3 posV;
    float falloff;
    float3 dirV;
    float cosTheta;
    float3 atten;
    float cosPhi;
    int type;
    float3 padding;
};

StructuredBuffer<Material> gMaterials : register(t1);

StructuredBuffer<Light> gLights : register(t2);

SamplerState gSample : register(s0);

float4 dirLight(uint lightIdx, uint matIdx, float3 posVNormalized, float3 normalV) {
    float3 toLight = -gLights[lightIdx].dirV;
    float diffused = max(0.f, dot(normalV, toLight));
    float specular = 0.f;
    
    if (diffused > 0.f && gLights[lightIdx].specular.a > 0.f) {
        float3 reflected = reflect(-toLight, normalV);
        specular = pow(max(0.f, dot(reflected, -posVNormalized)), gMaterials[matIdx].specular.a);
    }

    
    return gMaterials[matIdx].ambient * gLights[lightIdx].ambient +
        gMaterials[matIdx].diffuseMap.sample(gSam0, u, v) * gLights[lightIdx].diffuse * diffused +
        gMaterials[matIdx].specularMap.sample(gSam0, u, v) * gLights[lightIdx].specular * specular;
}

float4 pointLight(uint lightIdx, uint matIdx, float3 posV, float3 posVNormalized, float3 normalV) {
    float3 toLight = gLights[lightIdx].posV - posV;
    float dist = length(toLight);
    toLight /= dist;
    float atten = 1.f / dot(gLights[lightIdx].atten, float3(1.f, dist, dist * dist));
    float diffused = max(0.f, dot(normalV, toLight));
    float specular = 0.f;

    if (diffused > 0.f && gLights[lightIdx].specular.a > 0.f) {
        float3 reflected = reflect(-toLight, normalV);
        specular = pow(max(0.f, dot(reflected, -posVNormalized)), gMaterials[matIdx].specular.a);
    }

    return ( gMaterials[matIdx].ambient * gLights[lightIdx].ambient +
        gMaterials[matIdx].diffuseMap.sample(gSam0, u, v) * gLights[lightIdx].diffuse * diffused +
        gMaterials[matIdx].specularMap.sample(gSam0, u, v) * gLights[lightIdx].specular * specular
    ) * atten;
}

float4 spotLight(uint lightIdx, uint matIdx, float3 posV, float3 posVNormalized, float3 normalV) {
    float3 toLight = gLights[lightIdx].posV - posV;
    float dist = length(toLight);
    toLight /= dist;
    float atten = 1.f / dot(gLights[lightIdx].atten, float3(1.f, dist, dist * dist));
    float diffused = max(0.f, dot(normalV, toLight));
    float specular = 0.f;
    
    if (diffused > 0.f && gLights[lightIdx].specular.a > 0.f) {
        float3 reflected = reflect(-toLight, normalV);
        specular = pow(max(0.f, dot(reflected, -posVNormalized)), gMaterials[matIdx].specular.a);
    }

    float cosChi = max(dot(-toLight, gLights[lightIdx].dirV), 0.f);

    float coneAtten = pow(
        max( (cosChi - gLights[lightIdx].cosPhi)
                / (gLights[lightIdx].cosTheta - gLights[lightIdx].cosPhi),
            0.f
        ),
        gLights[lightIdx].falloff
    );

    return ( gMaterials[matIdx].ambient * gLights[lightIdx].ambient +
        gMaterials[matIdx].diffuseMap.sample(gSam0, u, v) * gLights[lightIdx].diffuse * diffused +
        gMaterials[matIdx].specularMap.sample(gSam0, u, v) * gLights[lightIdx].specular * specular
    ) * atten * coneAtten;
}

float4 Lighting(float3 posV, float3 normalV, uint matIdx) {
    float4 color = gMaterials[matIdx].ambient * gcGlobalAmbientLight + gMaterials[matIdx].emmisive;

    for (uint i = 0; i < gLightCnt; ++i) {
        float3 posVNormalized = normalize(posV);

        switch (gLights[i].type) {
            case LIGHT_TYPE_DIRECTIONAL:
                color += dirLight(i, matIdx, posVNormalized, normalV);
                break;
            case LIGHT_TYPE_POINT:
                color += pointLight(i, matIdx, posV, posVNormalized, normalV);
                break;
            case LIGHT_TYPE_SPOT:
                color += spotLight(i, matIdx, posV, posVNormalized, normalV);
                break;
        }
    }

    color.a = gMaterials[matIdx].diffuse.a;

    return color;
}