#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 wvp;
};

struct Material {
    int4 idxAlbedo;
    int4 idxRoughness;
    int4 idxMetallic;
    
    float4 cAlbedo;
    float cRoughness;
    float cMetallic;
};

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv : UV;
};

cbuffer PerDrawcallData : register(b0) {
    Material material;
    uint idxDrawcall;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

VSOutput VSMain( float3 position : POSITION, float2 uv : UV,
    uint idxInst : SV_InstanceID
) {
    VSOutput ret;
    
    ret.pos = mul(float4(position, 1.0f), gInstances[idxInst + idxDrawcall].wvp);
    ret.uv = uv;
    
    return ret;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    return sampleBindless(material.idxAlbedo, input.uv);
}