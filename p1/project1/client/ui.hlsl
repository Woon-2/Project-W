#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 world;
};

struct Material
{
    int4 idxAlbedo;
    int4 idxRoughness;
    int4 idxMetallic;
    
    float4 cAlbedo;
    float cRoughness;
    float cMetallic;
};

struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer PerDrawcallData : register(b0) {
    Material material;
    uint idxDrawcall;
};

cbuffer PerFrameData : register(b1)
{
    float screenWidth;
    float screenHeight;
    float2 padding0;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

VS_OUTPUT VSMain(VS_INPUT input, uint idxInst : SV_InstanceID)
{
    VS_OUTPUT ret;
    
    PerInstanceData instance = gInstances[idxInst + idxDrawcall];

    ret.pos = mul(float4(input.pos, 1.0f), instance.world);
    ret.uv = input.uv;
    
    float2 ndc;
    ndc.x = (ret.pos.x / screenWidth) * 2.0f - 1.0f;
    ndc.y = 1.0f - (ret.pos.y / screenHeight) * 2.0f;
    
    ret.pos = float4(ndc, ret.pos.z, 1.0f);
    
    return ret;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    return sampleBindless(material.idxAlbedo, input.uv);
}