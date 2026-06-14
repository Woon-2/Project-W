// Bloom pipeline shaders (pixel-based, HDR mip chain).
// Shared fullscreen-triangle VS + three PS entry points:
//   PSPrefilter  : reads HDR scene color, soft-threshold + 2x downsample -> bloom mip 0
//   PSDownsample : 13-tap 2x downsample of the previous (larger) mip
//   PSUpsample   : 3x3 tent upsample of the smaller mip (additively blended onto larger)
// All sources are sampled bindlessly (BilinearClamp). srcTexelSize = 1 / source dims.

#include "bindless.hlsli"

// Matches BloomShader::PerDrawcallData in shader.hpp (32 bytes).
cbuffer PerDrawcallData : register(b0) {
    int4   idxSrc;        // source SRV (scene color for prefilter, a bloom mip otherwise)
    float2 srcTexelSize;  // 1 / source dimensions
    float  threshold;     // prefilter brightness threshold (unused by down/upsample)
    float  _pad;
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

float3 srcTap(float2 uv) {
    return sampleBindless(idxSrc, uv).rgb;
}

// 13-tap downsample filter (Call of Duty / Jimenez). t = source texel size.
float3 downsample13(float2 uv, float2 t) {
    float3 a = srcTap(uv + t * float2(-2, -2));
    float3 b = srcTap(uv + t * float2( 0, -2));
    float3 c = srcTap(uv + t * float2( 2, -2));
    float3 d = srcTap(uv + t * float2(-2,  0));
    float3 e = srcTap(uv + t * float2( 0,  0));
    float3 f = srcTap(uv + t * float2( 2,  0));
    float3 g = srcTap(uv + t * float2(-2,  2));
    float3 h = srcTap(uv + t * float2( 0,  2));
    float3 i = srcTap(uv + t * float2( 2,  2));
    float3 j = srcTap(uv + t * float2(-1, -1));
    float3 k = srcTap(uv + t * float2( 1, -1));
    float3 l = srcTap(uv + t * float2(-1,  1));
    float3 m = srcTap(uv + t * float2( 1,  1));
    float3 r  = (j + k + l + m) * 0.5f   * 0.25f;
    r        += (a + b + d + e) * 0.125f * 0.25f;
    r        += (b + c + e + f) * 0.125f * 0.25f;
    r        += (d + e + g + h) * 0.125f * 0.25f;
    r        += (e + f + h + i) * 0.125f * 0.25f;
    return r;
}

// Soft-knee brightness threshold. Keeps energy above `thr`, soft ramp around the knee.
float3 softThreshold(float3 c, float thr) {
    float knee   = max(thr * 0.5f, 1e-4f);
    float br     = max(c.r, max(c.g, c.b));
    float soft   = br - thr + knee;
    soft         = clamp(soft, 0.0f, 2.0f * knee);
    soft         = soft * soft / (4.0f * knee + 1e-4f);
    float contrib = max(soft, br - thr) / max(br, 1e-4f);
    return c * contrib;
}

float4 PSPrefilter(VSOutput input) : SV_TARGET {
    float3 c = downsample13(input.uv, srcTexelSize);
    return float4(softThreshold(c, threshold), 1.0f);
}

float4 PSDownsample(VSOutput input) : SV_TARGET {
    return float4(downsample13(input.uv, srcTexelSize), 1.0f);
}

// 3x3 tent upsample. Output is additively blended onto the destination (larger) mip.
float4 PSUpsample(VSOutput input) : SV_TARGET {
    float2 t = srcTexelSize;
    float3 s  = srcTap(input.uv + t * float2(-1, -1)) * 1.0f;
    s        += srcTap(input.uv + t * float2( 0, -1)) * 2.0f;
    s        += srcTap(input.uv + t * float2( 1, -1)) * 1.0f;
    s        += srcTap(input.uv + t * float2(-1,  0)) * 2.0f;
    s        += srcTap(input.uv + t * float2( 0,  0)) * 4.0f;
    s        += srcTap(input.uv + t * float2( 1,  0)) * 2.0f;
    s        += srcTap(input.uv + t * float2(-1,  1)) * 1.0f;
    s        += srcTap(input.uv + t * float2( 0,  1)) * 2.0f;
    s        += srcTap(input.uv + t * float2( 1,  1)) * 1.0f;
    s        *= (1.0f / 16.0f);
    return float4(s, 1.0f);
}
