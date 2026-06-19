struct PerInstanceData {
    float4x4 world;
};

cbuffer PerDrawcallData : register(b0) {
    uint firstInstanceOffset;
    uint3 padding;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP;       // maps CAMERA-RELATIVE world positions (posW - camPos) to light NDC
    uint     cascadeIdx;
    uint3    _pfd0;
    float3   camPos;        // camera world position for the camera-relative shadow rebase
    float    _pfd1;
}

StructuredBuffer<PerInstanceData> gInstances : register(t0);

// VS: transforms position directly into light clip space for the current cascade.
// No GS: each cascade is rendered in a separate pass (separate Texture2D per cascade).
// Camera-relative shadow space: lightVP expects (posW - camPos), so rebase here. This must
// match the receiver-side rebase in pbrLighting.hlsli::calcCSMShadow, otherwise casters and
// receivers would land in different light-space frames and shadows would be wrong.
float4 VSMain(
    float3 position : POSITION,
    uint idxInst : SV_InstanceID
) : SV_Position {
    float3 posW = mul(float4(position, 1.0f), gInstances[idxInst + firstInstanceOffset].world).xyz;
    return mul(float4(posW - camPos, 1.0f), lightVP);
}
// No PSMain: depth-only pass, NumRenderTargets = 0
