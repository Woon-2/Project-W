// terrainShadowMapCSM.hlsl
// Depth-only shadow pass for terrain geometry, separate Texture2D per cascade.
// No GS: each cascade is rendered in a separate pass.

cbuffer PerDrawcallData : register(b0) {
    float4x4 world;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP;       // maps CAMERA-RELATIVE world positions (posW - camPos) to light NDC
    uint     cascadeIdx;
    uint3    _pfd0;
    float3   camPos;        // camera world position for the camera-relative shadow rebase
    float    _pfd1;
};

// VS: transforms terrain vertex directly into light clip space for the current cascade.
// Camera-relative shadow space: rebase by camPos to match the receiver side.
float4 VSMain(float3 position : POSITION) : SV_Position {
    float3 posW = mul(float4(position, 1.0f), world).xyz;
    return mul(float4(posW - camPos, 1.0f), lightVP);
}
// No PSMain: depth-only pass, NumRenderTargets = 0
