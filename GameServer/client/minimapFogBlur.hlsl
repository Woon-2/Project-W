// Fog-of-war edge feather for the minimap cache. The terrain/prop bake writes alpha=1
// where a loaded chunk/prop covers a pixel, 0 elsewhere. This 2-pass separable filter
// blurs ONLY that coverage alpha (so the streamed-area edge fades smoothly) while keeping
// the RGB color SHARP (center tap, never blurred). The vertical pass composites
// finalRGB = sharpRGB * blurredAlpha so the UI quad samples it with a plain alpha blend:
// color stays crisp inside the loaded area and fades to black (fog) at the edge.
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

// Box-blur ONLY the coverage alpha along `dir`; RGB is taken sharp from the center tap.
float blurAlpha(float2 uv, float2 dir) {
    const float step = blurRadiusTexels / float(kTapHalfCount);
    float a = 0.f;
    float w = 0.f;
    [unroll] for (int i = -kTapHalfCount; i <= kTapHalfCount; ++i) {
        const float2 offset = dir * srcTexelSize * (float(i) * step);
        a += sampleBindless(idxSrc, uv + offset).a;
        w += 1.f;
    }
    return a / w;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    const float3 rgb = sampleBindless(idxSrc, input.uv).rgb;   // sharp, never blurred

    if (horizontal) {
        // Pass 1: horizontal alpha blur; pass RGB through unchanged.
        return float4(rgb, blurAlpha(input.uv, float2(1.f, 0.f)));
    }

    // Pass 2: vertical alpha blur (input alpha is already H-blurred) + composite.
    const float a = blurAlpha(input.uv, float2(0.f, 1.f));
    return float4(rgb * a, a);   // premultiplied: black (fog) where coverage fades out
}
