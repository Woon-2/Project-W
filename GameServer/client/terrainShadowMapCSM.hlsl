// terrainShadowMapCSM.hlsl
// Depth-only shadow pass for terrain geometry, separate Texture2D per cascade.
// No GS: each cascade is rendered in a separate pass.

cbuffer PerDrawcallData : register(b0) {
    float4x4 world;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP;
    uint     cascadeIdx;
    uint3    _pfd0;
};

// VS: transforms terrain vertex directly into light clip space for the current cascade.
float4 VSMain(float3 position : POSITION) : SV_Position {
    float4 posW = mul(float4(position, 1.0f), world);
    return mul(posW, lightVP);
}
// No PSMain: depth-only pass, NumRenderTargets = 0
