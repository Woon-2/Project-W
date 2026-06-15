#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 world;
    float4 uvScaleBias;   // uv' = uv * xy + zw  (9-slice sub-rect mapping)
};

struct Material
{
    int4 idxAlbedo;
    int4 idxRoughness;
    int4 idxMetallic;
    
    float4 cAlbedo;
    float cRoughness;
    float cMetallic;
};

struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer PerDrawcallData : register(b0) {
    Material material;
    uint idxDrawcall;
};

cbuffer PerFrameData : register(b1)
{
    float screenWidth;
    float screenHeight;
    float time;        // seconds; animates the skill charge-fill surface
    float padding0;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

VS_OUTPUT VSMain(VS_INPUT input, uint idxInst : SV_InstanceID)
{
    VS_OUTPUT ret;
    
    PerInstanceData instance = gInstances[idxInst + idxDrawcall];

    ret.pos = mul(float4(input.pos, 1.0f), instance.world);
    ret.uv = input.uv * instance.uvScaleBias.xy + instance.uvScaleBias.zw;
    
    float2 ndc;
    ndc.x = (ret.pos.x / screenWidth) * 2.0f - 1.0f;
    ndc.y = (ret.pos.y / screenHeight) * 2.0f - 1.0f;
    
    ret.pos = float4(ndc, ret.pos.z, 1.0f);
    
    return ret;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    float4 tex = sampleBindless(material.idxAlbedo, input.uv);

    // material.cMetallic is repurposed as the UI effect mode:
    //   0 = normal UI draw, 1 = skill charging, 2 = skill ready (see uiPipeline.cpp).
    float mode = material.cMetallic;
    if (mode < 0.5f)
        return tex * material.cAlbedo;

    // Skill charge fill: a liquid rises from the bottom with a wavy, always-moving
    // surface. material.cRoughness carries the fill fraction (0..1) toward the next
    // stack; the surface keeps undulating even when the fill is constant.
    float fill = saturate(material.cRoughness);
    float h    = 1.0f - input.uv.y;                 // 0 at bottom, 1 at top

    float px   = input.pos.x * 0.012f;              // per-icon phase from screen x
    float wave = 0.018f * sin(input.uv.x * 18.0f + time * 3.0f + px)
               + 0.010f * sin(input.uv.x * 11.0f - time * 2.0f + px);
    float surface = fill + wave;

    float inside  = 1.0f - smoothstep(surface - 0.012f, surface + 0.012f, h);  // 1 below surface
    float crest   = smoothstep(0.05f, 0.0f, abs(h - surface)) * step(0.02f, fill);

    float3 base = tex.rgb;
    float3 col;
    if (mode < 1.5f)
        col = lerp(base * 0.30f, base * 1.12f, inside);   // charging: dim base, bright fill
    else
        col = base * (1.0f + inside * 0.55f);             // ready: lit base + glow tide

    col += (base * 0.6f + float3(0.25f, 0.45f, 0.75f)) * crest;   // cool glow crest

    return float4(col * material.cAlbedo.rgb, tex.a * material.cAlbedo.a);
}