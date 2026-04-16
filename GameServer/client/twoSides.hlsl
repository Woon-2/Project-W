// twoSides.hlsl
// Port of Unity Shader Graphs/HS_Blend_TwoSides.
// Used for mesh particles with two-sided rendering (e.g. slash wave trail).
// Pipeline: TwoSidesPipeline (Mesh mode, CullMode = None).

#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 world;              // row-major (transposed before upload)
    float4   tint;               // ColorOverLifetime * startColor
    float2   custom1;
    float2   custom2;
    float    t;                  // normalized particle age [0, 1]
    float    customDataEnabled;
    float2   pad;
};

// b0: per-drawcall — bindless indices + FX params
cbuffer PerDrawcallData : register(b0) {
    uint4  idxMainTex;           // 16B
    uint4  idxMaskTex;           // 16B
    uint4  idxNoiseTex;          // 16B
    uint   hasNoiseTex;          // 4B
    uint3  pad0;                 // 12B
    uint   firstInstanceOffset;  // 4B
    uint3  pad1;                 // 12B
    float4 mainTexST;            // 16B  xy=tiling, zw=offset
    float4 maskTexST;            // 16B
    float4 noiseTexST;           // 16B
    float2 noiseSpeed;           // 8B   UV animation speed
    float2 pad2;                 // 8B
    float  emission;             // 4B
    float  opacity;              // 4B
    float  useBackFresnel;       // 4B   0=off, 1=on
    float  backFresnel;          // 4B   negative = darker back faces
    float  time;                 // 4B
    float3 pad3;                 // 12B
};

// b1: per-frame
cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;        // 64B  row-major
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
    float4 vertexColor : COLOR;
    float4 tint        : TINT;
    float  t           : PRAGE;
};

PSInput VSMain(VSInput input) {
    PSInput ret;
    PerInstanceData inst = gInstances[firstInstanceOffset + input.instID];

    float4 worldPos = mul(float4(input.position, 1.0f), inst.world);
    ret.pos         = mul(worldPos, matViewProj);
    ret.uv          = input.uv;
    ret.vertexColor = input.color;
    ret.tint        = inst.tint;
    ret.t           = inst.t;
    return ret;
}

float4 PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET {
    // --- Noise-based UV distortion ---
    float2 distortedUV = input.uv;
    if (hasNoiseTex != 0u) {
        float2 noiseUV     = transformUV(input.uv, noiseTexST) + noiseSpeed * time;
        float2 noiseSample = sampleBindless(idxNoiseTex, noiseUV).rg;
        distortedUV += (noiseSample * 2.0f - 1.0f) * 0.04f;
    }

    // --- Main texture ---
    float2 mainUV    = transformUV(distortedUV, mainTexST);
    float4 mainColor = sampleBindless(idxMainTex, mainUV);

    // --- Mask ---
    float2 maskUV    = transformUV(input.uv, maskTexST);
    float  maskAlpha = sampleBindless(idxMaskTex, maskUV).r;

    // --- Composite ---
    float4 vc       = input.vertexColor * input.tint;
    float3 finalRGB = mainColor.rgb * emission * vc.rgb;
    float  finalA   = mainColor.a * maskAlpha * opacity * vc.a;

    // --- Back-face darkening via pseudo-Fresnel ---
    // backFresnel=-4 -> saturate(1+(-4)*0.25)=0 -> back faces fully transparent.
    if (useBackFresnel > 0.5f && !isFrontFace) {
        float fresnelFactor = saturate(1.0f + backFresnel * 0.25f);
        finalA *= fresnelFactor;
    }

    return float4(finalRGB, finalA);
}
