#include "bindless.hlsli"

cbuffer PerDrawcallData : register(b0) {
    int4 idxAlbedo;
};

cbuffer PerFrameData : register(b1) {
    float4x4 vp;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 uvw : UVW;
};

VSOutput VSMain(
    float3 position : POSITION
) {
    VSOutput ret;
    
    position.xyz *= 2.f;
    
    float4 clipPos = mul(float4(position, 0.f), vp);
    ret.pos = clipPos.xyww;
    ret.uvw = position.xyz;
    
    return ret;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    input.uvw = normalize(input.uvw);
    return sampleBindlessCube(idxAlbedo, input.uvw);
}