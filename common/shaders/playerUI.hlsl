#ifndef __playerUI_hlsl__
#define __playerUI_hlsl__

#include "bindless.hlsl"
#include "samplers.hlsl"

struct Material
{
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
    uint4 metallicSmoothnessMapRef;
    uint4 emmisiveMapRef;
    uint4 ambientOcclusionMapRef;
};

cbuffer PerDrawcallData : register(b1)
{
    Material material;
    uint samplerIdx;
    uint instanceBase;
    uint padding[2];
};

struct PerInstanceData {
    float4x4 world;
};

StructuredBuffer<PerInstanceData> gInstances: register(t0);

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VSOutput VSMain(float3 position : POSITION, float2 uv : TEXCOORD, uint instanceOffset : SV_InstanceID) {
    VSOutput result;
  
    result.pos = mul(float4(position, 1.0f), gInstances[instanceBase + instanceOffset].world);
    result.uv = uv;

    return result;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float4 color = sampleFromMapRef(material.albedoMapRef, input.uv, samplerIdx);
    
    return color;
}

#endif // __playerUI_hlsl__