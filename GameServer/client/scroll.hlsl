#include "bindless.hlsli"

// Port of Unity Hovl/Particles/Scroll.
// Billboard particle shader with noise-based alpha dissolve, flow distortion,
// and path-based UV Y-remapping for trail scroll effects.

struct PerInstanceData {
    float4x4 world;               // row-major (transposed before upload)
    float4   tint;                // particle color * color-over-lifetime
    float4   stretchAxisAndMode;  // xyz=world-space stretch axis, w=1 when enabled
    float    rotation;
    float    path;                // trail position [0..1] (Unity TEXCOORD0.z)
    float    customDataW;         // per-particle dissolve power bias (Unity TEXCOORD0.w)
    float    pad;
};

cbuffer PerDrawcallData : register(b0) {
    uint4 idxMainTex;   // 16B
    uint4 idxNoiseTex;  // 16B
    uint4 idxFlowTex;   // 16B
    uint4 idxMaskTex;   // 16B

    uint  firstInstanceOffset;  // 4B
    uint  hasNoiseTex;          // 4B
    uint  hasFlowTex;           // 4B
    uint  hasMaskTex;           // 4B

    float time;                 // 4B
    float pathSet;              // 4B  _PathSet0ifyouuseinPS
    float usePSCustomDataW;     // 4B  0=off, 1=on (_UsePScustomdataW)
    float noiseDistortPower;    // 4B  _Noisedistortpower

    float4 mainTexST;           // 16B
    float4 noiseTexST;          // 16B
    float4 flowTexST;           // 16B
    float4 maskTexST;           // 16B

    float4 speedMainTexUVNoiseZW;    // 16B  xy=main UV scroll, zw=noise UV scroll
    float4 distortionSpeedXYPowerZ;  // 16B  xy=flow UV scroll, z=distortion power

    float4 color;    // 16B  rgb=tint, a=alpha multiplier

    float emission;  // 4B
    float opacity;   // 4B
    float2 pad0;     // 8B
};

cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;  // row-major
    float3   cameraPosW;
    float    pad1;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

float2 transformUV(float2 uv, float4 st) {
    return uv * st.xy + st.zw;
}

float4 sampleOrWhite(uint4 idx, uint hasTexture, float2 uv) {
    return hasTexture != 0u ? sampleBindless(idx, uv) : float4(1.0f, 1.0f, 1.0f, 1.0f);
}

// VS -> GS: world-space point. No SV_Position to avoid premature hardware clipping.
struct VSOutput {
    float3 worldPos       : TEXCOORD0;
    float2 size           : TEXCOORD1;
    float  rotation       : TEXCOORD2;
    float4 tint           : COLOR;
    float3 stretchAxisW   : TEXCOORD3;
    float  stretchMode    : TEXCOORD4;
    float  path           : TEXCOORD5;
    float  customDataW    : TEXCOORD6;
};

struct PSInput {
    float4 pos         : SV_Position;
    float2 uv          : TEXCOORD0;
    float4 vertexColor : COLOR;
    float  path        : TEXCOORD1;
    float  customDataW : TEXCOORD2;
};

VSOutput VSMain(float3 position : POSITION, float size : SIZE, uint instID : SV_InstanceID) {
    VSOutput ret;
    PerInstanceData inst = gInstances[firstInstanceOffset + instID];

    float4 worldPos = mul(float4(position, 1.0f), inst.world);
    float scaleX = length(float3(inst.world._m00, inst.world._m10, inst.world._m20));
    float scaleY = length(float3(inst.world._m01, inst.world._m11, inst.world._m21));

    ret.worldPos     = worldPos.xyz;
    ret.size         = float2(size * scaleX, size * scaleY);
    ret.rotation     = inst.rotation;
    ret.tint         = inst.tint;
    ret.stretchAxisW = inst.stretchAxisAndMode.xyz;
    ret.stretchMode  = inst.stretchAxisAndMode.w;
    ret.path         = inst.path;
    ret.customDataW  = inst.customDataW;
    return ret;
}

[maxvertexcount(4)]
void GSMain(point VSOutput input[1], inout TriangleStream<PSInput> triStream) {
    float3 pos   = input[0].worldPos;
    float3 vLook = normalize(cameraPosW - pos);

    float3 worldUp = (abs(vLook.y) < 0.999f) ? float3(0.0f, 1.0f, 0.0f)
                                              : float3(1.0f, 0.0f, 0.0f);
    float3 vRight = normalize(cross(worldUp, vLook));
    float3 vUp    = cross(vLook, vRight);

    float3 vRightR = vRight;
    float3 vUpR    = vUp;

    float3 stretchAxis        = input[0].stretchAxisW;
    float3 stretchAxisOnPlane = stretchAxis - dot(stretchAxis, vLook) * vLook;
    float  stretchLenSq       = dot(stretchAxisOnPlane, stretchAxisOnPlane);
    if (input[0].stretchMode > 0.5f && stretchLenSq > 0.000001f) {
        vUpR    = stretchAxisOnPlane * rsqrt(stretchLenSq);
        vRightR = normalize(cross(vUpR, vLook));
    } else {
        float c = cos(input[0].rotation);
        float s = sin(input[0].rotation);
        vRightR =  c * vRight + s * vUp;
        vUpR    = -s * vRight + c * vUp;
    }

    float halfW = input[0].size.x * 0.5f;
    float halfH = input[0].size.y * 0.5f;

    float3 p[4];
    p[0] = pos - halfW * vRightR - halfH * vUpR;  // bottom-left
    p[1] = pos + halfW * vRightR - halfH * vUpR;  // bottom-right
    p[2] = pos - halfW * vRightR + halfH * vUpR;  // top-left
    p[3] = pos + halfW * vRightR + halfH * vUpR;  // top-right

    // uv.y=1 at bottom (trail tail), uv.y=0 at top (trail head) - matches Unity convention
    float2 baseUV[4] = {
        float2(0.0f, 1.0f),
        float2(1.0f, 1.0f),
        float2(0.0f, 0.0f),
        float2(1.0f, 0.0f)
    };

    PSInput output;
    [unroll]
    for (int i = 0; i < 4; ++i) {
        output.pos         = mul(float4(p[i], 1.0f), matViewProj);
        output.uv          = baseUV[i];
        output.vertexColor = input[0].tint;
        output.path        = input[0].path;
        output.customDataW = input[0].customDataW;
        triStream.Append(output);
    }
}

float4 PSMain(PSInput input) : SV_TARGET {
    const float2 uv = input.uv;

    // --- Flow map: scrolled, used to distort noise and main UV ---
    float2 flowUV     = transformUV(uv, flowTexST) + distortionSpeedXYPowerZ.xy * time;
    float2 flowSample = sampleOrWhite(idxFlowTex, hasFlowTex, flowUV).rg;

    // --- Noise UV ---
    // 1. Apply tiling/offset + time scroll
    float2 noiseUV = transformUV(uv, noiseTexST) + speedMainTexUVNoiseZW.zw * time;
    // 2. Path-based Y remapping: dissolves the trail from tail (path=1) to head (path=0).
    //    Remaps so that pixels whose noiseUV.y < path become invisible (0).
    float  path = input.path + pathSet;
    noiseUV.y = (noiseUV.y - path) / max(1.0f - path, 0.0001f);
    // 3. Flow distortion
    noiseUV -= flowSample * distortionSpeedXYPowerZ.z;
    // Clip very low noise values to sharpen edges
    float4 noiseSample = saturate(sampleOrWhite(idxNoiseTex, hasNoiseTex, noiseUV) - 0.1f);

    // --- Main texture UV: scroll + flow distortion ---
    float2 mainUV = transformUV(uv, mainTexST) + speedMainTexUVNoiseZW.xy * time;
    mainUV       -= flowSample * distortionSpeedXYPowerZ.z;
    float4 mainSample = sampleBindless(idxMainTex, mainUV);

    // --- RGB ---
    float3 rgb = noiseSample.rgb * mainSample.rgb * color.rgb * input.vertexColor.rgb;

    // --- Edge mask: steep power gradient, fades out at trail tail (uv.y ~ 0) ---
    // 1 - (1 - uv.y)^40: near 0 at uv.y=0, near 1 at uv.y=1
    float edgeMask = max(0.0f, 1.0f - pow(max(1.0f - uv.y, 0.0f), 40.0f));

    float3 finalRGB = rgb * edgeMask * emission;

    // --- Alpha ---
    // Noise dissolve: exponent optionally driven by per-particle customDataW.
    // exponent = saturate(customDataW) * 10 * toggle + noiseDistortPower
    float exponent  = saturate(input.customDataW) * 10.0f * usePSCustomDataW + noiseDistortPower;
    float noisePow  = min(pow(max(noiseSample.a, 0.0001f), exponent), 1.0f);
    // Remap [2/7..3/7] -> [0..1] to create a sharp dissolve edge
    float dissolved = saturate(noisePow * 7.0f - 2.0f);

    float alpha  = edgeMask * dissolved * color.a * input.vertexColor.a * opacity;
    float maskA  = sampleOrWhite(idxMaskTex, hasMaskTex, transformUV(uv, maskTexST)).a;
    float finalA = alpha * maskA;

    return float4(finalRGB, finalA);
}
