#ifndef __skybox_hlsl__
#define __skybox_hlsl__

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

cbuffer PerDrawcallData : register(b1) {
    Material material;
    uint samplerIdx;
};

cbuffer PerFrameData : register(b2)
{
    float4x4 vp;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

VS_OUTPUT VS_Main(VS_INPUT input)
{
    VS_OUTPUT output;
   
    float4 clipPos = mul(float4(input.position, 0.0f), vp);
    
    output.position = clipPos.xyww;
    output.texcoord = input.texcoord;
   
    return output;
}

float4 PS_Main(VS_OUTPUT input) : SV_TARGET
{
    float4 color = sampleFromMapRef(material.albedoMapRef, input.texcoord, samplerIdx);
    
    return color;
}


#endif // __skybox_hlsl__