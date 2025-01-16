#include "bindless.hlsl"

cbuffer PerDrawcallData : register(b1) {
    uint4 frameMapRef;
    uint samplerIdx;
    uint3 padding;
};

struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VS_OUTPUT VSMain(uint vertexID : SV_VertexID) {
    VS_OUTPUT result;

    // screen quad
    if (vertexID == 0) {
        result.pos = float4(-1.0f, 1.0f, 1.0f, 1.0f);
        result.uv = float2(0.0f, 0.0f);
    } else if (vertexID == 1) {
        result.pos = float4(1.0f, 1.0f, 1.0f, 1.0f);
        result.uv = float2(1.0f, 0.0f);
    } else if (vertexID == 2) {
        result.pos = float4(-1.0f, -1.0f, 1.0f, 1.0f);
        result.uv = float2(0.0f, 1.0f);
    } else if (vertexID == 3) {
        result.pos = float4(1.0f, -1.0f, 1.0f, 1.0f);
        result.uv = float2(1.0f, 1.0f);
    }

    return result;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET {
    return float4( sampleFromMapRef(frameMapRef, input.uv, samplerIdx).rrr, 1.0f );
}