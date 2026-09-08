#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 world;              // row-major (transposed before upload)
    float4   tint;               // ColorOverLifetime * startColor
    float2   custom1;            // Unity Particle Custom1.xy
    float2   custom2;            // Unity Particle Custom2.xy
    float    t;                  // normalized particle age [0, 1]
    float    customDataEnabled;  // 0=legacy fallback, 1=Unity custom streams
    float2   pad;
};

// b0: bindless indices + instance offset + FX params (all per-drawcall)
cbuffer PerDrawcallData : register(b0) {
    uint4  idxMainTex;
    uint4  idxEmissionTex;
    uint4  idxDissolveTex;
    uint4  idxFlowTex;
    uint   firstInstanceOffset;
    uint3  pad0;
    float2 speedMainTexUV;
    float2 speedDissolveUV;
    float2 speedFlow;
    float2 padUV0;
    float4 mainTexST;
    float4 emissionTexST;
    float4 dissolveTexST;
    float4 flowTexST;
    float4 addColor;
    float  emission;
    float  desaturation;
    float  opacity;
    float  flowPower;
    float  useSmoothDissolve;  // 0=hard, 1=smooth
    float  time;
    float2 remapMinMax;        // x=min, y=max
    float4 pad1;
};

// b1: per-frame
cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;  // row-major
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

float2 transformUV(float2 uv, float4 st) {
    return uv * st.xy + st.zw;
}

struct VSInput {
    float3 position : POSITION;
    float2 uv       : UV;
    float4 color    : COLOR;
    uint   instID   : SV_InstanceID;
};

struct PSInput {
    float4 pos         : SV_Position;
    float2 uv          : TEXCOORD0;
    float2 custom1     : TEXCOORD1;
    float2 custom2     : TEXCOORD2;
    float4 vertexColor : COLOR;
    float4 tint        : TINT;
    float  t           : PRAGE;
    float  customDataEnabled : TEXCOORD3;
};

PSInput VSMain(VSInput input) {
    PSInput ret;
    PerInstanceData inst = gInstances[firstInstanceOffset + input.instID];

    float4 worldPos = mul(float4(input.position, 1.0f), inst.world);
    ret.pos         = mul(worldPos, matViewProj);
    ret.uv          = input.uv;
    ret.custom1     = inst.custom1;
    ret.custom2     = inst.custom2;
    ret.vertexColor = input.color;
    ret.tint        = inst.tint;
    ret.t           = inst.t;
    ret.customDataEnabled = inst.customDataEnabled;
    return ret;
}

float4 PSMain(PSInput input) : SV_TARGET {
    float2 uv = input.uv;

    // --- Flow map UV distortion (skipped when flowPower == 0) ---
    if (flowPower > 0.0f) {
        float2 flowUV     = transformUV(input.uv, flowTexST) + speedFlow * time;
        float2 flowSample = sampleBindless(idxFlowTex, flowUV).rg;
        uv += (flowSample * 2.0f - 1.0f) * flowPower;
    }

    // --- Main texture ---
    float2 mainUV     = transformUV(uv, mainTexST) + speedMainTexUV * time;
    float2 emissionUV = transformUV(uv, emissionTexST) + speedMainTexUV * time;
    float2 dissolveUVBase = uv + speedDissolveUV * time;
    if (input.customDataEnabled > 0.5f)
        dissolveUVBase.y += input.custom2.y;
    float2 dissolveUV = transformUV(dissolveUVBase, dissolveTexST);
    float4 mainSample = sampleBindless(idxMainTex, mainUV);
    float  mainAlpha  = saturate(mainSample.a * opacity);

    // --- Emission texture: desaturate -> remap -> * emission ---
    // Remap: Unity Shader Graph Remap node maps [0,1] -> [min,max], then saturates.
    // Formula: sat(emColor * (max - min) + min)
    float4 emSample = sampleBindless(idxEmissionTex, emissionUV);
    float  lum      = dot(emSample.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    float3 emColor  = lerp(emSample.rgb, float3(lum, lum, lum), desaturation);
    emColor = saturate(emColor * (remapMinMax.y - remapMinMax.x) + remapMinMax.x);
    emColor *= emission;

    // --- Final RGB: (emission + addColor) * vertexColor * tint ---
    float4 vc       = input.vertexColor * input.tint;
    float3 finalRGB = (emColor + addColor.rgb) * vc.rgb;

    // --- Dissolve alpha mask ---
    float dissolveRaw = sampleBindless(idxDissolveTex, dissolveUV).r;
    float dissolveMask;
    if (input.customDataEnabled > 0.5f) {
        // Unity HS_SwordSlash packs Custom1.xy after UV.xy, and Custom2.xy after UV2.xy.
        // Custom1.x drives the slash reveal/dissolve curve, Custom1.y clips the UV arc,
        // and Custom2.y randomizes the dissolve-noise vertical offset per particle.
        float smoothMask = saturate(dissolveRaw + 0.5f - input.custom1.x);
        float arcMask = (input.custom1.y >= 1.0f - input.uv.x) ? 1.0f : 0.0f;
        float hardThreshold = (useSmoothDissolve > 0.5f) ? 1.0f : input.custom1.x;
        float hardMask = (arcMask * hardThreshold >= dissolveRaw) ? 1.0f : 0.0f;
        dissolveMask = (useSmoothDissolve > 0.5f) ? hardMask * smoothMask : hardMask;
    } else {
        // Legacy fallback: no Unity custom streams available.
        float dissolveSample = dissolveRaw + 0.5f;
        float dissolveThresh = input.t;  // 0=fresh, 1=dead -> dissolves over lifetime
        if (useSmoothDissolve > 0.5f)
            dissolveMask = saturate(dissolveSample - dissolveThresh);
        else
            dissolveMask = (dissolveSample >= dissolveThresh) ? 1.0f : 0.0f;
    }

    float finalAlpha = dissolveMask * mainAlpha * vc.a;

    return float4(finalRGB, finalAlpha);
}
