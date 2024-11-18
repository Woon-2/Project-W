#ifndef __skybox_hlsl__
#define __skybox_hlsl__

#include "bindless.hlsl"
#include "samplers.hlsl"

cbuffer PerDrawcallData : register(b1) {
    float4x4 wvp;
    uint4 skyboxMapRef;
    uint instanceBase;
    uint samplerIdx;
    uint2 padding;
};

struct VSOutput {
    float3 localpos : POSITION;
    float4 pos : SV_POSITION;	
}

VSOutput VSMain(float3 position : POSITION, float2 texcoord : TEXCOORD) {
    VSOutput result;

    result.pos = mul(float4(position, 1.0f),wvp);
    result.localpos = position;

    return result;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    float4 color = gTexCubes[material.skyboxMapRef.y].Sample(gSamplers[samplerIdx], input.localpos);

    return color;
}

#endif // __skybox_hlsl__