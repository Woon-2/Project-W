#include "bindless.hlsli"

// Port of Unity "Vefects/SH_VFX_Vefects_Piercing_BIRP_New" (Amplify, Unlit surface).
// Mesh-mode particle. Per-particle Custom1 stream drives the dissolve/opacity reveal:
//   uv2.z <- custom1.x   (alpha reveal over lifetime)
//   uv2.w <- custom1.y   (emissive reveal over lifetime)
// Blend is standard alpha (SrcAlpha / InvSrcAlpha), two-sided, depth read-only.

struct PerInstanceData {
    float4x4 world;
    float4   tint;
    float2   custom1;
    float2   custom2;
    float    t;
    float    customDataEnabled;
    float2   pad;
};

cbuffer PerDrawcallData : register(b0) {
    uint4 idxColorNoiseTex;
    uint4 idxPiercingTex;
    uint4 idxPiercingNoiseTex;
    uint4 idxDistortionNoiseTex;
    uint4 idxDistortionMaskTex;
    uint4 idxEmissiveNoiseTex;
    uint4 idxEmissiveMaskTex;
    uint4 idxOpacityMaskTex;

    uint  firstInstanceOffset;
    uint  hasDistortionMask;
    uint  hasEmissiveMask;
    uint  hasOpacityMask;

    float time;
    float colorBoost;
    float piercingNoiseIntensity;
    float distortionIntensity;

    float emissiveIntensity;
    float opacityBoost;
    float2 pad0;

    float4 color1;
    float4 color2;
    float4 emissiveColor;

    float4 colorNoiseScaleSpeed;       // xy = scale, zw = speed
    float4 piercingNoiseScaleSpeed;    // xy = scale, zw = speed
    float4 distortionNoiseScaleSpeed;  // xy = scale, zw = speed
    float4 emissiveDissolveScaleSpeed; // xy = scale, zw = speed

    float4 distortionMaskST;           // xy = scale, zw = offset
    float4 opacityMaskST;              // xy = scale, zw = offset
};

cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;
    float3   cameraPosW;
    float    pad1;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

struct VSInput {
    float3 position : POSITION;
    float2 uv       : UV;
    float4 color    : COLOR;
    uint   instID   : SV_InstanceID;
};

struct PSInput {
    float4 pos               : SV_Position;
    float2 uv                : TEXCOORD0;
    float4 vertexColor       : COLOR;
    float4 tint              : TINT;
    float2 custom1           : TEXCOORD1;
    float  customDataEnabled : TEXCOORD2;
};

float sampleR(uint4 idxTex, uint hasTexture, float2 uv, float fallback) {
    return hasTexture != 0u ? sampleBindless(idxTex, uv).r : fallback;
}

PSInput VSMain(VSInput input) {
    PSInput ret;
    PerInstanceData inst = gInstances[firstInstanceOffset + input.instID];

    float4 worldPos = mul(float4(input.position, 1.0f), inst.world);
    ret.pos = mul(worldPos, matViewProj);
    ret.uv = input.uv;
    ret.vertexColor = input.color;
    ret.tint = inst.tint;
    ret.custom1 = inst.custom1;
    ret.customDataEnabled = inst.customDataEnabled;
    return ret;
}

float4 PSMain(PSInput input) : SV_TARGET {
    const float2 uv = input.uv;

    // uv2.z / uv2.w come from Custom1.xy when custom data is fed; otherwise 0.
    const float uv2z = input.customDataEnabled > 0.5f ? input.custom1.x : 0.0f;
    const float uv2w = input.customDataEnabled > 0.5f ? input.custom1.y : 0.0f;

    // --- Distortion ---------------------------------------------------------
    const float2 distNoiseUV = uv * distortionNoiseScaleSpeed.xy + time * distortionNoiseScaleSpeed.zw;
    const float2 distMaskUV  = uv * distortionMaskST.xy + distortionMaskST.zw;
    const float distNoise = sampleBindless(idxDistortionNoiseTex, distNoiseUV).r;
    const float distMask  = sampleR(idxDistortionMaskTex, hasDistortionMask, distMaskUV, 1.0f);
    const float distortion = ((distNoise * (1.0f - distMask)) * 0.1f) * distortionIntensity;

    // --- Piercing texture + noise ------------------------------------------
    const float piercingTexR = sampleBindless(idxPiercingTex, uv + distortion).r;
    const float2 pierceNoiseUV = uv * piercingNoiseScaleSpeed.xy + time * piercingNoiseScaleSpeed.zw;
    const float pierceNoise = sampleBindless(idxPiercingNoiseTex, pierceNoiseUV).r;
    const float piercing = clamp(piercingTexR + (piercingTexR * (pierceNoise * piercingNoiseIntensity)), 0.0f, 1.0f);

    // --- Base color (color-noise lerp) -------------------------------------
    const float2 colorNoiseUV = uv * colorNoiseScaleSpeed.xy + time * colorNoiseScaleSpeed.zw;
    const float colorNoise = sampleBindless(idxColorNoiseTex, colorNoiseUV).r;
    const float3 lerpColor = lerp(color1.rgb, color2.rgb, colorNoise);
    const float3 baseColor = (lerpColor * colorBoost) * piercing;

    // --- Emissive dissolve --------------------------------------------------
    const float2 emisDissolveUV = uv * emissiveDissolveScaleSpeed.xy + time * emissiveDissolveScaleSpeed.zw;
    const float emisMaskG = hasEmissiveMask != 0u
                          ? sampleBindless(idxEmissiveMaskTex, uv + distortion).g
                          : 1.0f;
    const float emisNoiseR = sampleBindless(idxEmissiveNoiseTex, emisDissolveUV + distortion).r;
    const float emisReveal = -1.0f + (1.0f - uv2w);            // remap(1-uv2.w) over [-1,0]
    const float emisTerm = saturate(emisReveal + saturate(emisMaskG * emisNoiseR));
    const float3 emission = input.vertexColor.rgb * (emisTerm * emissiveColor.rgb * emissiveIntensity);

    const float3 outEmission = baseColor + emission;

    // --- Alpha --------------------------------------------------------------
    const float2 opacityUV = uv * opacityMaskST.xy + opacityMaskST.zw;
    const float opacityMaskB = sampleR(idxOpacityMaskTex, hasOpacityMask, opacityUV, 1.0f);
    const float alphaReveal = (1.0f - uv2z) * 2.0f;            // remap(1-uv2.z) over [0,2]
    const float opacityInner = saturate((piercing * opacityBoost) * opacityMaskB);
    const float alpha = saturate(input.vertexColor.a * saturate(alphaReveal + opacityInner));

    return float4(outEmission, alpha);
}
