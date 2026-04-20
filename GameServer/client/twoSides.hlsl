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

// b0: per-drawcall
cbuffer PerDrawcallData : register(b0) {
    uint4  idxMainTex;           // 16B
    uint4  idxMaskTex;           // 16B
    uint4  idxNoiseTex;          // 16B
    uint   hasNoiseTex;          // 4B
    uint   hasMaskTex;           // 4B
    uint2  pad0;                 // 8B
    uint   firstInstanceOffset;  // 4B
    uint3  pad1;                 // 12B
    float4 mainTexST;            // 16B  xy=tiling, zw=offset
    float4 maskTexST;            // 16B
    float4 noiseTexST;           // 16B
    float4 texSpeed;             // 16B  xy=main UV speed, zw=noise UV speed
    float  emission;             // 4B
    float  opacity;              // 4B
    float  useFresnel;           // 4B   0=off, 1=on
    float  fresnelPower;         // 4B   exponent
    float4 frontFacesColor;      // 16B  front face base tint
    float4 backFacesColor;       // 16B  back face base tint
    float4 fresnelColor;         // 16B  front Fresnel rim color
    float  fresnelEmission;      // 4B
    float  useBackFresnel;       // 4B   0=off, 1=on
    float  backFresnel;          // 4B   exponent (negative -> saturates to 1 = full rim)
    float  backFresnelEmission;  // 4B
    float4 backFresnelColor;     // 16B  back face rim color
    float  time;                 // 4B
    float3 pad3;                 // 12B
};

// b1: per-frame
cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;        // 64B  row-major
    float3   cameraPos;          // 12B  world-space
    float    cbpad;              // 4B
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

float2 transformUV(float2 uv, float4 st) {
    return uv * st.xy + st.zw;
}

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
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
    float3 worldPos    : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
};

PSInput VSMain(VSInput input) {
    PSInput ret;
    PerInstanceData inst = gInstances[firstInstanceOffset + input.instID];

    float4 worldPos4 = mul(float4(input.position, 1.0f), inst.world);
    ret.pos         = mul(worldPos4, matViewProj);
    ret.worldPos    = worldPos4.xyz;
    // inst.world = transpose(W_true); mul(n, W_true^T) gives correct world-space normal
    ret.worldNormal = normalize(mul(input.normal, (float3x3)inst.world));
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
        float2 noiseUV     = transformUV(input.uv, noiseTexST) + texSpeed.zw * time;
        float2 noiseSample = sampleBindless(idxNoiseTex, noiseUV).rg;
        distortedUV += (noiseSample * 2.0f - 1.0f) * 0.04f;
    }

    // --- Main texture (with UV animation) ---
    float2 mainUV    = transformUV(distortedUV + texSpeed.xy * time, mainTexST);
    float4 mainColor = sampleBindless(idxMainTex, mainUV);

    // --- Mask ---
    float2 maskUV    = transformUV(input.uv, maskTexST);
    float  maskAlpha = hasMaskTex ? sampleBindless(idxMaskTex, maskUV).r : 1.0f;

    // --- Fresnel (view-angle rim, N.V based) ---
    float3 N    = normalize(input.worldNormal);
    float3 V    = normalize(cameraPos - input.worldPos);
    float  NdotV = saturate(dot(N, V));

    // --- Front face color ---
    float3 frontColor;
    if (useFresnel > 0.5f) {
        float fresnel = pow(1.0f - NdotV, max(fresnelPower, 0.001f));
        frontColor = frontFacesColor.rgb * (1.0f - fresnel)
                   + fresnelColor.rgb * fresnelEmission * fresnel;
    } else {
        frontColor = frontFacesColor.rgb;
    }

    // --- Back face color ---
    // pow(base, negative_exp) saturates to 1 for all valid NdotV, giving full rim.
    float3 backColor;
    if (useBackFresnel > 0.5f) {
        float backF = saturate(pow(max(1.0f - NdotV, 0.0001f), backFresnel));
        backColor = backFacesColor.rgb * max(1.0f - backF, 0.0f)
                  + backFresnelColor.rgb * backFresnelEmission * backF;
    } else {
        backColor = backFacesColor.rgb;
    }

    float3 faceColor = isFrontFace ? frontColor : backColor;

    // --- Composite ---
    float4 vc       = input.vertexColor * input.tint;
    float3 finalRGB = mainColor.rgb * emission * faceColor * vc.rgb;
    float  finalA   = mainColor.a * maskAlpha * opacity * vc.a;

    return float4(finalRGB, finalA);
}
