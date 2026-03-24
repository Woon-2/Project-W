#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 world;
    float    rotation;
    float3   pad;
};

struct Material
{
    int4 idxTex;
    float3 tint;
};

struct VSOutput
{
    float4 pos      : SV_Position;
    float  size     : SIZE;
    float  rotation : ROTATION;
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
    ret.pos      = mul(float4(position, 1.0f), instance.world);
    ret.size     = size.x * instance.world._m00;
    ret.rotation = instance.rotation;

    return ret;
}

[maxvertexcount(4)]
void GSMain(point VSOutput input[1],
    inout TriangleStream<PSInput> triStream
) {
    float3 vUP = float3(0.0f, 1.0f, 0.0f);
    float3 vLook = normalize(cameraPosV - input[0].pos.xyz);
    float3 vRight = normalize(cross(vUP, vLook));
    vUP = cross(vLook, vRight);

    float c = cos(input[0].rotation);
    float s = sin(input[0].rotation);
    float3 vRightR = c * vRight + s * vUP;
    float3 vUPR    = -s * vRight + c * vUP;

    float halfWidth  = input[0].size * 0.5f;
    float halfHeight = input[0].size * 0.5f;

    float4 vertices[4];
    vertices[0] = float4(input[0].pos.xyz + halfWidth * vRightR - halfHeight * vUPR, 1.0f);
    vertices[1] = float4(input[0].pos.xyz + halfWidth * vRightR + halfHeight * vUPR, 1.0f);
    vertices[2] = float4(input[0].pos.xyz - halfWidth * vRightR - halfHeight * vUPR, 1.0f);
    vertices[3] = float4(input[0].pos.xyz - halfWidth * vRightR + halfHeight * vUPR, 1.0f);
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
    float4 src = sampleBindless(material.idxTex, input.uv);
    return float4(src.xyz * material.tint, src.a);
}