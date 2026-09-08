#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 world;         // row-major (transposed before upload from CPU)
    float4   tint;          // RGBA multiplier
    float4   uvScaleOffset; // xy = sprite-sheet frame scale, zw = frame offset (1,1,0,0 = full texture)
};

// b0: 32B
// int4 idxTex(16B) | firstInstanceOffset(4B) | pad(12B)
cbuffer PerDrawcallData : register(b0) {
    uint4 idxTex;
    uint  firstInstanceOffset;
    uint3 pad;
};

// b1: 64B
cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;  // row-major (transposed before upload from CPU)
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

struct VSInput {
    float3 position : POSITION;
    float2 uv       : UV;
    uint   instID   : SV_InstanceID;
};

struct PSInput {
    float4 pos  : SV_Position;
    float2 uv   : UV;
    float4 tint : TINT;
};

PSInput VSMain(VSInput input) {
    PSInput ret;
    PerInstanceData inst = gInstances[firstInstanceOffset + input.instID];

    // row-vector * row-major matrix
    float4 worldPos = mul(float4(input.position, 1.0f), inst.world);
    ret.pos  = mul(worldPos, matViewProj);
    // Remap the mesh UV into the current sprite-sheet frame (flipbook).
    ret.uv   = input.uv * inst.uvScaleOffset.xy + inst.uvScaleOffset.zw;
    ret.tint = inst.tint;
    return ret;
}

float4 PSMain(PSInput input) : SV_TARGET {
    float4 src = sampleBindless(idxTex, input.uv);
    return float4(src.rgb * input.tint.rgb, src.a * input.tint.a);
}
