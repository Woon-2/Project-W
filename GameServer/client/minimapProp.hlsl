// Top-down albedo bake of scatter props (trees, rocks, mesh props) into the minimap
// background cache. No lighting; alpha-cutout keeps foliage canopy shapes (so trees
// read as leafy blobs rather than solid quads). Output alpha = 1 so props also count
// as "loaded coverage" for the fog-of-war mask.

#include "bindless.hlsli"

cbuffer PerDrawcallData : register(b0) {
    float4x4 wvp;
    int4     idxAlbedo;
    float4   tint;
    float    alphaCutoff;
    float3   _pad;
};

struct VSOutput {
    float4 posH : SV_Position;
    float2 uv   : UV;
};

VSOutput VSMain(float3 position : POSITION, float2 uv : UV) {
    VSOutput o;
    o.posH = mul(float4(position, 1.f), wvp);
    o.uv   = uv;
    return o;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    float4 albedo = (idxAlbedo.x >= 0) ? sampleBindless(idxAlbedo, input.uv) : float4(1.f, 1.f, 1.f, 1.f);
    albedo *= tint;
    if (alphaCutoff > 0.f) clip(albedo.a - alphaCutoff);
    return float4(albedo.rgb, 1.f);
}
