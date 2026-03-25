// terrainShadowMap.hlsl
// Depth-only shadow pass for terrain geometry.
// No instancing: terrain is rendered as a single draw call per chunk.

cbuffer PerDrawcallData : register(b0) {
    float4x4 world;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP;
};

float4 VSMain(float3 position : POSITION) : SV_Position {
    return mul(mul(float4(position, 1.0f), world), lightVP);
}
// No PSMain: depth-only pass, NumRenderTargets = 0
