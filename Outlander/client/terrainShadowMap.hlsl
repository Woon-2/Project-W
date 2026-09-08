// terrainShadowMap.hlsl
// Non-CSM single-cascade shadow map shader for terrain geometry.
// Kept for reference; active CSM path uses terrainShadowMapCSM.hlsl.
cbuffer PerDrawcallData : register(b0) {
    float4x4 world;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP;
};

float4 VSMain(float3 position : POSITION) : SV_Position {
    return mul(mul(float4(position, 1.f), world), lightVP);
}
// No PSMain: depth-only pass, NumRenderTargets = 0
