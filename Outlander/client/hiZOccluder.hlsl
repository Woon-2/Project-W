struct PerInstanceData {
    float4x4 wvp;
};

cbuffer PerDrawcallData : register(b0) {
    uint firstInstanceOffset;
    uint3 padding;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

float4 VSMain(
    float3 position : POSITION,
    uint idxInst : SV_InstanceID
) : SV_Position {
    return mul(float4(position, 1.f), gInstances[idxInst + firstInstanceOffset].wvp);
}
// No PSMain: depth-only pass, NumRenderTargets = 0
