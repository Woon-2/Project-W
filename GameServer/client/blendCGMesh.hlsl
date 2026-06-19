#include "bindless.hlsli"

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
    uint4 idxMainTex;
    uint4 idxNoiseTex;
    uint4 idxFlowTex;
    uint4 idxMaskTex;
    uint4 idxCameraDepthTex;

    uint  firstInstanceOffset;
    uint  hasNoiseTex;
    uint  hasFlowTex;
    uint  hasMaskTex;

    uint  hasCameraDepthTex;
    float time;
    float cameraNear;
    float cameraFar;

    float4 mainTexST;
    float4 noiseTexST;
    float4 flowTexST;
    float4 maskTexST;

    float4 speedMainTexUVNoiseZW;
    float4 distortionSpeedXYPowerZ;
    float4 color;
    float4 uvRect;

    float emission;
    float opacity;
    float textureOpacity;
    float multiplyTexture;

    float useOnlyColor;
    float useFresnel;
    float fresnelPower;
    float fresnelScale;

    float useCenterGlow;
    float useDepth;
    float depthPower;
    float pad0;
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
    float4 pos         : SV_Position;
    float4 clipPos     : TEXCOORD0;
    float3 worldPos    : TEXCOORD1;
    float2 uv          : TEXCOORD2;
    float4 vertexColor : COLOR;
    float4 tint        : TINT;
};

float2 transformUV(float2 uv, float4 st) {
    return uv * st.xy + st.zw;
}

float4 sampleOrWhite(uint4 idxTex, uint hasTexture, float2 uv) {
    return hasTexture != 0u ? sampleBindless(idxTex, uv) : float4(1.0f, 1.0f, 1.0f, 1.0f);
}

// Reversed-Z: depth01=1.0(near) -> nearZ, depth01=0.0(far) -> farZ.
float eyeDepthFromDeviceDepth(float depth01) {
    const float nearZ = max(cameraNear, 0.0001f);
    const float farZ = max(cameraFar, nearZ + 0.0001f);
    return (nearZ * farZ) / max(nearZ + depth01 * (farZ - nearZ), 0.0001f);
}

float calcSoftParticleFade(float4 clipPos) {
    if (useDepth <= 0.5f || hasCameraDepthTex == 0u)
        return 1.0f;

    const float invW = rcp(max(abs(clipPos.w), 0.0001f));
    const float2 ndc = clipPos.xy * invW;
    const float2 screenUV = float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);

    const float sceneDepth01 = sampleBindless(idxCameraDepthTex, screenUV).r;
    const float particleDepth01 = saturate(clipPos.z * invW);
    const float sceneEye = eyeDepthFromDeviceDepth(sceneDepth01);
    const float particleEye = eyeDepthFromDeviceDepth(particleDepth01);

    return saturate((sceneEye - particleEye) / max(depthPower, 0.0001f));
}

PSInput VSMain(VSInput input) {
    PSInput ret;
    PerInstanceData inst = gInstances[firstInstanceOffset + input.instID];

    float4 worldPos = mul(float4(input.position, 1.0f), inst.world);
    float4 clipPos = mul(worldPos, matViewProj);

    ret.pos = clipPos;
    ret.clipPos = clipPos;
    ret.worldPos = worldPos.xyz;
    ret.uv = input.uv * uvRect.zw + uvRect.xy;
    ret.vertexColor = input.color;
    ret.tint = inst.tint;
    return ret;
}

float4 PSMain(PSInput input) : SV_TARGET {
    const float2 uv = input.uv;
    const float2 mainScroll = speedMainTexUVNoiseZW.xy * time;
    const float2 noiseScroll = speedMainTexUVNoiseZW.zw * time;
    const float2 flowScroll = distortionSpeedXYPowerZ.xy * time;

    float4 maskSample = sampleOrWhite(idxMaskTex, hasMaskTex, transformUV(uv, maskTexST));
    float4 flowSample = sampleOrWhite(idxFlowTex, hasFlowTex, transformUV(uv + flowScroll, flowTexST));
    float2 distortion = flowSample.rg * maskSample.rg * distortionSpeedXYPowerZ.z;
    distortion /= max(abs(mainTexST.xy), float2(0.0001f, 0.0001f));

    float4 noiseSample = sampleOrWhite(idxNoiseTex, hasNoiseTex, transformUV(uv + noiseScroll, noiseTexST));
    float4 mainSample = sampleBindless(idxMainTex, transformUV(uv + mainScroll - distortion, mainTexST));

    float textureAlpha = saturate(mainSample.a * noiseSample.a);
    float3 textureColor = mainSample.rgb * noiseSample.rgb;

    float softDepthFade = calcSoftParticleFade(input.clipPos);

    float3 outColor = textureColor;

    if (useCenterGlow > 0.5f) {
        float centerCoord = uv.x;
        float3 centerGlow = saturate(maskSample.rgb - float3(1.0f - centerCoord, 1.0f - centerCoord, 1.0f - centerCoord))
                          * maskSample.rgb;
        outColor *= centerGlow;
    }

    float4 vc = input.vertexColor * input.tint;
    outColor *= color.rgb * emission * vc.rgb;

    float alpha = textureAlpha;
    if (useDepth > 0.5f)
        alpha *= softDepthFade;

    alpha = saturate(alpha * color.a * vc.a * opacity);

    return float4(outColor, alpha);
}
