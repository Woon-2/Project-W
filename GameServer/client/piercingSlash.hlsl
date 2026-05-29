#include "bindless.hlsli"

// Port of Unity "Vefects/SH_VFX_Vefects_Slash_BIRP_New" (Amplify, Unlit surface).
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
    uint4 idxSlashTex;
    uint4 idxSlashNoiseTex;
    uint4 idxEmissiveSlashTex;
    uint4 idxEmissiveDissolveTex;
    uint4 idxDistortionNoiseTex;
    uint4 idxColorNoiseTex;
    uint4 idxMaskTex;
    uint4 idxCutoutTex;

    uint  firstInstanceOffset;
    uint  hasMaskTex;
    uint  hasCutoutTex;
    uint  pad0;

    float time;
    float colorBoost;
    float slashScale;
    float slashSpeed;

    float emissiveSlashScale;
    float emissiveSlashSpeed;
    float slashNoiseIntensity;
    float distortionIntensity;

    float emissiveIntensity;
    float opacityBoost;
    float additiveLerp;
    float cutoutErosion;

    float cutoutErosionSmoothness;
    float cutoutRotation;
    float2 cutoutOffset;

    float4 color1;
    float4 color2;
    float4 emissiveColor;

    float4 colorNoiseScaleSpeed;       // xy = scale, zw = speed
    float4 slashNoiseScaleSpeed;       // xy = scale, zw = speed
    float4 emissiveDissolveScaleSpeed; // xy = scale, zw = speed
    float4 distortionNoiseScaleSpeed;  // xy = scale, zw = speed

    float4 maskST;                     // xy = scale, zw = offset
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

    // --- Mask (with _Mask_ST scale/offset) ---------------------------------
    const float2 maskUV = uv * maskST.xy + maskST.zw;
    const float maskR = hasMaskTex != 0u ? sampleBindless(idxMaskTex, maskUV).r : 1.0f;

    // --- Distortion --------------------------------------------------------
    const float2 distNoiseUV = uv * distortionNoiseScaleSpeed.xy + time * distortionNoiseScaleSpeed.zw;
    const float distNoise = sampleBindless(idxDistortionNoiseTex, distNoiseUV).r;
    const float distortion = (distNoise * 0.1f) * distortionIntensity;

    // --- Slash texture + Slash noise ---------------------------------------
    const float2 slashUV = uv * float2(slashScale, 1.0f) + time * float2(slashSpeed, 0.0f);
    const float slashR = sampleBindless(idxSlashTex, slashUV + distortion).r;
    const float2 slashNoiseUV = uv * slashNoiseScaleSpeed.xy + time * slashNoiseScaleSpeed.zw;
    const float slashNoiseG = sampleBindless(idxSlashNoiseTex, slashNoiseUV).g;
    const float slash = saturate(slashR * slashNoiseIntensity + slashNoiseG);
    const float slashMasked = maskR * slash;

    // ColorOverLifetime * startColor lives on input.tint; combine with the
    // particle's per-vertex color so rgb tint AND alpha fade reach the output
    // (mirrors swordSlash/twoSides/blendCGMesh).
    const float4 vc = input.vertexColor * input.tint;

    // --- Base color (color-noise lerp) -------------------------------------
    const float2 colorNoiseUV = uv * colorNoiseScaleSpeed.xy + time * colorNoiseScaleSpeed.zw;
    const float colorNoise = sampleBindless(idxColorNoiseTex, colorNoiseUV).r;
    const float3 lerpColor = lerp(color1.rgb, color2.rgb, colorNoise);
    const float3 baseColor = (lerpColor * colorBoost) * slashMasked * vc.rgb;

    // --- Emissive ----------------------------------------------------------
    const float2 emisSlashUV = uv * float2(emissiveSlashScale, 1.0f) + time * float2(emissiveSlashSpeed, 0.0f);
    const float emisSlashG = sampleBindless(idxEmissiveSlashTex, emisSlashUV + distortion).g;
    const float2 emisDissolveUV = uv * emissiveDissolveScaleSpeed.xy + time * emissiveDissolveScaleSpeed.zw;
    const float emisDissolveR = sampleBindless(idxEmissiveDissolveTex, emisDissolveUV).r;
    const float emisCombined = saturate(emisSlashG * emisDissolveR);
    // remap (1 - uv2.w) over [0,1] -> [-1, 0]: equals (-uv2.w)
    const float emisReveal = saturate(-uv2w + emisCombined);
    const float3 emission = vc.rgb * ((emisReveal * maskR) * emissiveColor.rgb * emissiveIntensity);

    const float3 colorBeforeAlphaCombine = baseColor + emission;

    // --- Cutout (rotated, erosion smoothstep) ------------------------------
    float cutout = 1.0f;
    if (hasCutoutTex != 0u) {
        const float2 cutoutBaseUV = uv + cutoutOffset;
        const float cosA = cos(radians(cutoutRotation));
        const float sinA = sin(radians(cutoutRotation));
        const float2 centered = cutoutBaseUV - float2(0.5f, 0.5f);
        const float2 rotated = float2(centered.x * cosA - centered.y * sinA,
                                      centered.x * sinA + centered.y * cosA) + float2(0.5f, 0.5f);
        const float cutoutG = sampleBindless(idxCutoutTex, rotated).g;
        cutout = smoothstep(cutoutErosion, cutoutErosion + cutoutErosionSmoothness, cutoutG);
    }

    // --- Alpha -------------------------------------------------------------
    // remap (1 - uv2.z) over [0,1] -> [0, 2]: equals (1 - uv2.z) * 2
    const float opacityReveal = saturate((1.0f - uv2z) * 2.0f);
    const float opacityInner = saturate(saturate(slashMasked) * opacityBoost);
    const float opacityTerm = saturate(opacityReveal + opacityInner);
    const float alpha = saturate((vc.a * opacityTerm) * cutout);

    // --- Final emission (AdditiveLerp: blend toward pre-multiplied) --------
    const float3 outEmission = lerp(colorBeforeAlphaCombine,
                                    saturate(colorBeforeAlphaCombine * alpha),
                                    additiveLerp);

    return float4(outEmission, alpha);
}
