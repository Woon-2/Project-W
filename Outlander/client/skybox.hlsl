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
    // Reversed-Z: far plane이 NDC z=0.0이므로 clip.z를 0으로 고정해 항상 far plane에 그린다.
    // (표준-Z에서는 xyww로 clip.z=w(NDC z=1.0=far)를 강제했음)
    ret.pos = float4(clipPos.xy, 0.f, clipPos.w);
    ret.uvw = position.xyz;
    
    return ret;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    input.uvw = normalize(input.uvw);
    return sampleBindlessCube(idxAlbedo, input.uvw);
}