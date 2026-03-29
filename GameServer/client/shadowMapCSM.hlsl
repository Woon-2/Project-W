struct PerInstanceData {
    float4x4 world;
};

cbuffer PerDrawcallData : register(b0) {
    uint firstInstanceOffset;
    uint3 padding;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP;
    uint     cascadeIdx;
    uint3    _pfd0;
}

StructuredBuffer<PerInstanceData> gInstances : register(t0);

// VS: transforms position directly into light clip space for the current cascade.
// No GS: each cascade is rendered in a separate pass (separate Texture2D per cascade).
float4 VSMain(
    float3 position : POSITION,
    uint idxInst : SV_InstanceID
) : SV_Position {
    float4 posW = mul(float4(position, 1.0f), gInstances[idxInst + firstInstanceOffset].world);
    return mul(posW, lightVP);
}
// No PSMain: depth-only pass, NumRenderTargets = 0
