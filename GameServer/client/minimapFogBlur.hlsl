// 2-pass separable box blur over the minimap terrain cache's "loaded chunk" alpha
// mask, producing a fog-of-war style soft fade at the edge of the streamed area.
// Pass 1 (horizontal == 1): blur along X, pass-through RGBA.
// Pass 2 (horizontal == 0): blur along Y, then composite finalRGB = lerp(black,
// srcRGB, blurredAlpha) so the UI quad can sample the result with a plain alpha blend.
// Shares the fullscreen-triangle VS trick with bloom.hlsl.

#include "bindless.hlsli"

cbuffer PerDrawcallData : register(b0) {
    int4   idxSrc;
    float2 srcTexelSize;
    float  blurRadiusTexels;
    uint   horizontal;
}

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv  : UV;
};

VSOutput VSMain(uint vertexID : SV_VertexID) {
    float2 uv  = float2((vertexID << 1) & 2, vertexID & 2);
    float4 pos = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);
    VSOutput o;
    o.pos = pos;
    o.uv  = uv;
    return o;
}

static const int kTapHalfCount = 8;

float4 PSMain(VSOutput input) : SV_TARGET {
    const float2 dir  = horizontal ? float2(1.f, 0.f) : float2(0.f, 1.f);
    const float  step = blurRadiusTexels / float(kTapHalfCount);

    float4 sum = float4(0.f, 0.f, 0.f, 0.f);
    float  totalWeight = 0.f;
    for (int i = -kTapHalfCount; i <= kTapHalfCount; ++i) {
        const float2 offset = dir * srcTexelSize * (float(i) * step);
        sum += sampleBindless(idxSrc, input.uv + offset);
        totalWeight += 1.f;
    }
    sum /= totalWeight;

    if (horizontal) {
        return sum;
    }

    const float3 finalRGB = lerp(float3(0.f, 0.f, 0.f), sum.rgb, sum.a);
    return float4(finalRGB, sum.a);
}
