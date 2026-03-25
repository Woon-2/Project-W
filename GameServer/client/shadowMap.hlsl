// shadowMap.hlsl
// Non-CSM single-cascade shadow map shader.
// Kept for reference; active CSM path uses shadowMapCSM.hlsl.
#define MAX_INSTANCES 1000

struct PerInstanceData {
    float4x4 world;
};

cbuffer PerDrawcallData : register(b0) {
    uint firstInstanceOffset;
    uint3 padding;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

float4 VSMain(
    float3 position : POSITION,
    uint idxInst : SV_InstanceID
) : SV_Position {
    return mul(mul(float4(position, 1.f), gInstances[idxInst + firstInstanceOffset].world), lightVP);
}
// No PSMain: depth-only pass, NumRenderTargets = 0
