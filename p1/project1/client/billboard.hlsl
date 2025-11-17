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

struct VSOutput
{
    float4 pos : SV_Position;
    float size : SIZE;
};

struct PSInput
{
    float4 pos : SV_Position;
    float3 normal : NORMAL;
    float2 uv : UV;
};

cbuffer PerDrawcallData : register(b0) {
    Material material;
    uint idxDrawcall;
};

cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;
    float3 cameraPosV;
    float padding0;
};  

StructuredBuffer<PerInstanceData> gInstances : register(t0);

VSOutput VSMain( float3 position : POSITION, float2 size : SIZE,
    uint idxInst : SV_InstanceID
) {
    VSOutput ret;
    
    PerInstanceData instance = gInstances[idxInst + idxDrawcall];
    ret.pos = mul(float4(position, 1.0f), instance.world);
    //ret.size = float2(2.0f, 2.0f); // Fixed size for billboard
    ret.size = size;
    
    return ret;
}

[maxvertexcount(4)]
void GSMain(point VSOutput input[1],
    inout TriangleStream<PSInput> triStream
) {
    float3 vUP = float3(0.0f, 1.0f, 0.0f);
    float3 vLook = normalize(cameraPosV - input[0].pos.xyz);
    float3 vRight = cross(vUP, vLook);
    
    float halfWidth = input[0].size.x * 0.5f;
    float halfHeight = input[0].size.x * 0.5f;
    
    float4 vertices[4];
    vertices[0] = float4(input[0].pos.xyz + halfWidth * vRight - halfHeight * vUP, 1.0f);
    vertices[1] = float4(input[0].pos.xyz + halfWidth * vRight + halfHeight * vUP, 1.0f);
    vertices[2] = float4(input[0].pos.xyz - halfWidth * vRight - halfHeight * vUP, 1.0f);
    vertices[3] = float4(input[0].pos.xyz - halfWidth * vRight + halfHeight * vUP, 1.0f);
    float2 uv[4] = { float2(0.f, 1.f), float2(0.f, 0.f), float2(1.f, 1.f), float2(1.f, 0.f) };
    
    PSInput output;
    for(int i = 0; i < 4; ++i) {
        output.pos = mul(vertices[i], matViewProj);
        output.normal = vLook;
        output.uv = uv[i];
        triStream.Append(output);
    }
}

float4 PSMain(PSInput input) : SV_TARGET {
    return sampleBindless(material.idxAlbedo, input.uv);
}