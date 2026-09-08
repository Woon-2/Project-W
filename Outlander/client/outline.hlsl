// outline.hlsl
// Inverted-hull silhouette for interaction highlighting (aimed world item drops).
// Pipeline: OutlinePipeline (CullMode = Front, depth test on / depth write off,
// additive into SceneColorHDR so bloom turns the rim into a glow).
//
// The hull is expanded in CLIP space, not object space: object-space extrusion
// makes the rim thicken with distance, while a clip-space offset scaled by w
// keeps a constant pixel width at any range.

#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 world;          // row-major (transposed before upload)
    float4   color;          // HDR rim color (values > 1 feed bloom)
    float    thicknessPx;    // silhouette width in pixels
    float3   pad;
};

// b0: per-drawcall
cbuffer PerDrawcallData : register(b0) {
    uint   firstInstanceOffset;  // 4B
    uint3  pad0;                 // 12B
};

// b1: per-frame
cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;        // 64B row-major
    float2   invScreenSize;      // 8B  (1/w, 1/h)
    float2   cbpad;              // 8B
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    uint   instID   : SV_InstanceID;
};

struct PSInput {
    float4 pos   : SV_Position;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input) {
    PerInstanceData inst = gInstances[firstInstanceOffset + input.instID];

    const float4 worldPos = mul(float4(input.position, 1.0f), inst.world);
    const float3 worldNrm = normalize(mul(input.normal, (float3x3)inst.world));

    float4 clipPos = mul(worldPos, matViewProj);
    // Project the normal into clip space and offset along its screen direction.
    // Multiplying by clipPos.w cancels the perspective divide, so thicknessPx is
    // an exact pixel count regardless of depth.
    const float4 clipNrm = mul(float4(worldNrm, 0.0f), matViewProj);
    const float2 dir2D = normalize(clipNrm.xy + 1e-6f);
    clipPos.xy += dir2D * inst.thicknessPx * 2.0f * invScreenSize * clipPos.w;

    PSInput output;
    output.pos   = clipPos;
    output.color = inst.color;
    return output;
}

float4 PSMain(PSInput input) : SV_Target {
    return input.color;
}
