#include "drawcallIdxOnly.hlsl"

#ifdef INCLUDE_basicInstancing
#include "basicInstancing.hlsl"
#endif

cbuffer PerFrameData : register(b1) {
    matrix gView2LightProj;
}

struct VS_INPUT {
    float3 pos : POSITION;
    uint instID : SV_InstanceID;
}

float4 VSMain(VS_INPUT input) : SV_POSITION {
    return mul( mul( float4(pos, 1.f), getInstanceData(input.instID).wv ), gView2LightProj );
}