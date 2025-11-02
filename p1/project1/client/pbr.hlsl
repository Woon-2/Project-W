struct PerInstanceData {
    float4x4 wvp;
    float4x4 wv;
    float3x3 wvNormal;
};

struct Material {
    int4 idxAlbedo;
    int4 idxRoughness;
    int4 idxMetallic;
    
    float4 cAlbedo;
    float cRoughness;
    float cMetallic;
    float cAO;
    float padding0;
    float3 cEmmisive;
    float padding1;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 posV : POSITION_V;
    float3 normalV : NORMAL_V;
    float2 uv : UV;
};

cbuffer PerDrawcallData : register(b0) {
    Material material;
    uint idxDrawcall;
};

cbuffer PerFrameData : register(b1) {
    float3 globalAmbient;
    float padding0;
    uint lightCnt;
    uint3 padding1;
}

StructuredBuffer<PerInstanceData> gInstances : register(t0);

#include "pbrLighting.hlsli"

VSOutput VSMain(
    float3 position : POSITION,
    float3 normal : NORMAL,
    float2 uv : UV,
    uint idxInst : SV_InstanceID
) {
    VSOutput ret;
    
    ret.pos = mul(float4(position, 1.0f), gInstances[idxInst + idxDrawcall].wvp);
    ret.posV = mul(float4(position, 1.0f), gInstances[idxInst + idxDrawcall].wv).xyz;
    ret.normalV = mul(normal, gInstances[idxInst + idxDrawcall].wvNormal);
    ret.uv = uv;
    
    return ret;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    return illuminate(input.posV, input.normalV, input.uv);
}