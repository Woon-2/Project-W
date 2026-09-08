#ifndef __heatField_hlsli__
#define __heatField_hlsli__

// Shared screen-space "heat field" evaluation used by two passes:
//   - heatHaze.hlsl      : additive tinted glow into SceneColorHDR (pre-bloom)
//   - tonemapResolve.hlsl: refraction warp of the scene-color sample UV
// Both read the same HeatSource array so the glow and the warp stay coherent.
//
// Conventions (must match the C++ HeatDistortionShader structs in shader.hpp):
//   centerRadius.xy    = screen-space center in UV (0..1)
//   centerRadius.zw    = screen-space radius in UV (x horizontal, y vertical)
//   zMarginIntensity.x = boss linear view-space Z (positive forward, matches GB4)
//   zMarginIntensity.y = depth-gate softness in view-Z units
//   zMarginIntensity.z = intensity envelope (spawn/death fade applied on the CPU)
//   zMarginIntensity.w = shimmer scroll speed
//   tint.rgb           = HDR tint color
//   tint.a             = per-source warp amplitude (UV units)

#define HEAT_MAX_SOURCES 4

struct HeatSource {
    float4 centerRadius;
    float4 zMarginIntensity;
    float4 tint;
};

// --- cheap procedural value-noise fbm (no texture asset) ---
float heatHash21(float2 p) {
    p = frac(p * float2(123.34f, 345.45f));
    p += dot(p, p + 34.345f);
    return frac(p.x * p.y);
}

float heatValueNoise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    float a = heatHash21(i);
    float b = heatHash21(i + float2(1.0f, 0.0f));
    float c = heatHash21(i + float2(0.0f, 1.0f));
    float d = heatHash21(i + float2(1.0f, 1.0f));
    float2 u = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float heatFbm(float2 p) {
    float v = 0.0f;
    float amp = 0.5f;
    [unroll] for (int i = 0; i < 3; ++i) {
        v += amp * heatValueNoise(p);
        p *= 2.02f;
        amp *= 0.5f;
    }
    return v;
}

// Depth gate: 1 where the pixel is at/behind the boss plane or is background
// (GB4 == 0, no geometry written -> sky/air), 0 for foreground occluders so a
// prop between the camera and the boss is never refracted.
float heatDepthGate(float pixelViewZ, float bossViewZ, float margin) {
    float bg = step(pixelViewZ, 1e-3f);                                   // background (no geometry)
    float fg = smoothstep(bossViewZ, bossViewZ + margin * 0.5f, pixelViewZ);
    return saturate(max(bg, fg));
}

// Evaluates the combined field at `uv` for a pixel at linear view-space depth
// `pixelViewZ`. Returns the refraction warp offset (UV) and the additive glow.
void evalHeatField(
    float2 uv, float pixelViewZ,
    HeatSource sources[HEAT_MAX_SOURCES], uint count,
    float time, float warpStrength, float glowStrength,
    out float2 warpOffset, out float3 glowColor)
{
    warpOffset = float2(0.0f, 0.0f);
    glowColor  = float3(0.0f, 0.0f, 0.0f);

    [loop] for (uint i = 0u; i < count; ++i) {
        HeatSource s = sources[i];
        float2 center       = s.centerRadius.xy;
        float2 radius       = max(s.centerRadius.zw, float2(1e-4f, 1e-4f));
        float  bossZ        = s.zMarginIntensity.x;
        float  margin       = max(s.zMarginIntensity.y, 1e-3f);
        float  intensity    = s.zMarginIntensity.z;
        float  shimmerSpeed = s.zMarginIntensity.w;
        float3 tint         = s.tint.rgb;
        float  warpAmp      = s.tint.a;

        // Elliptical radial falloff (smoothstep) around the boss center.
        float2 d       = (uv - center) / radius;
        float  dist    = length(d);
        float  falloff = saturate(1.0f - dist);
        falloff = falloff * falloff * (3.0f - 2.0f * falloff);

        float gate = heatDepthGate(pixelViewZ, bossZ, margin);
        float w    = falloff * intensity * gate;
        if (w <= 0.0f) continue;

        // Rising-air domain: noise scrolls upward (screen -Y) over time.
        //float2 np = uv * float2(7.0f, 5.0f) + float2(0.0f, -time * shimmerSpeed);
        //float  nx = heatFbm(np);
        //float  ny = heatFbm(np + float2(5.2f, 1.3f));
        //float2 flow = float2(nx, ny) - 0.5f;     // -0.5..0.5
        
        float2 np =
            uv * float2(32.0f, 24.0f)
            + float2(0.0f, -time * shimmerSpeed);

        float eps = 0.01f;

        float nL = heatFbm(np - float2(eps,0));
        float nR = heatFbm(np + float2(eps,0));

        float nD = heatFbm(np - float2(0,eps));
        float nU = heatFbm(np + float2(0,eps));

        float2 flow;

        flow.x = nU - nD;
        flow.y = -(nR - nL);

        flow = normalize(flow + 1e-5f);

        warpOffset += flow
                    * warpAmp
                    * warpStrength
                    * w;

        warpOffset += flow * (2.0f * warpAmp * warpStrength) * w;

        float shimmer = heatFbm(np * 1.3f + float2(0.0f, -time * shimmerSpeed * 1.6f));
        glowColor += tint * (w * (0.45f + 0.85f * shimmer) * glowStrength);
    }
}

#endif // __heatField_hlsli__
