#include "bindless.hlsli"

// DirectX12 port of Unity Shader Graphs/HS_Blend_CG for Smoke24bcg.
// Smoke24bcg material defaults:
//   _MainTex=Smoke24, _Emission=2, _Opacity=1, _Textureopacity=0
//   _Use_fresnel=0, _Usecenterglow=0, _Usedepth=1, _Depthpower=0.5
//   _Noise/_Flow/_Mask are unassigned and should be treated as white textures.

struct PerInstanceData {
    float4x4 world;  // row-major (transposed before upload from CPU)
    float4   tint;   // particle start color * color over lifetime
    float4   stretchAxisAndMode; // xyz=world-space stretch axis, w=1 when enabled
    float    rotation;
    float3   pad;
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
    float4 uvRect; // xy=offset, zw=scale

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
    float4x4 matViewProj;  // row-major (transposed before upload from CPU)
    float3   cameraPosW;
    float    pad1;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

struct VSInput {
    float3 position : POSITION;
    float  size     : SIZE;
    uint   instID   : SV_InstanceID;
};

struct VSOutput {
    float3 worldPos : TEXCOORD0;
    float2 size     : TEXCOORD1;
    float  rotation : TEXCOORD2;
    float4 tint     : COLOR;
    float3 stretchAxisW : TEXCOORD3;
    float  stretchMode  : TEXCOORD4;
};

struct PSInput {
    float4 pos          : SV_Position;
    float4 clipPos      : TEXCOORD0;
    float3 worldPos     : TEXCOORD1;
    float3 normalW      : TEXCOORD2;
    float2 uv           : TEXCOORD3;
    float4 vertexColor  : COLOR;
};

float2 transformUV(float2 uv, float4 st) {
    return uv * st.xy + st.zw;
}

float4 sampleOrWhite(uint4 idxTex, uint hasTexture, float2 uv) {
    return hasTexture != 0u ? sampleBindless(idxTex, uv) : float4(1.0f, 1.0f, 1.0f, 1.0f);
}

float eyeDepthFromDeviceDepth(float depth01) {
    const float nearZ = max(cameraNear, 0.0001f);
    const float farZ = max(cameraFar, nearZ + 0.0001f);
    return (nearZ * farZ) / max(farZ - depth01 * (farZ - nearZ), 0.0001f);
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

VSOutput VSMain(VSInput input) {
    VSOutput ret;
    PerInstanceData inst = gInstances[firstInstanceOffset + input.instID];

    float4 worldPos = mul(float4(input.position, 1.0f), inst.world);
    float scaleX = length(float3(inst.world._m00, inst.world._m10, inst.world._m20));
    float scaleY = length(float3(inst.world._m01, inst.world._m11, inst.world._m21));

    ret.worldPos = worldPos.xyz;
    ret.size = float2(input.size * scaleX, input.size * scaleY);
    ret.rotation = inst.rotation;
    ret.tint = inst.tint;
    ret.stretchAxisW = inst.stretchAxisAndMode.xyz;
    ret.stretchMode = inst.stretchAxisAndMode.w;
    return ret;
}

[maxvertexcount(4)]
void GSMain(point VSOutput input[1], inout TriangleStream<PSInput> triStream) {
    float3 pos = input[0].worldPos;
    float3 vLook = normalize(cameraPosW - pos);

    float3 worldUp = (abs(vLook.y) < 0.999f) ? float3(0.0f, 1.0f, 0.0f)
                                              : float3(1.0f, 0.0f, 0.0f);
    float3 vRight = normalize(cross(worldUp, vLook));
    float3 vUp = cross(vLook, vRight);

    float3 vRightR = vRight;
    float3 vUpR = vUp;

    float3 stretchAxis = input[0].stretchAxisW;
    float3 stretchAxisOnPlane = stretchAxis - dot(stretchAxis, vLook) * vLook;
    float stretchLenSq = dot(stretchAxisOnPlane, stretchAxisOnPlane);
    if (input[0].stretchMode > 0.5f && stretchLenSq > 0.000001f) {
        vUpR = stretchAxisOnPlane * rsqrt(stretchLenSq);
        vRightR = normalize(cross(vUpR, vLook));
    } else {
        float c = cos(input[0].rotation);
        float s = sin(input[0].rotation);
        vRightR = c * vRight + s * vUp;
        vUpR = -s * vRight + c * vUp;
    }

    float halfWidth = input[0].size.x * 0.5f;
    float halfHeight = input[0].size.y * 0.5f;

    float3 p[4];
    p[0] = pos - halfWidth * vRightR - halfHeight * vUpR;
    p[1] = pos + halfWidth * vRightR - halfHeight * vUpR;
    p[2] = pos - halfWidth * vRightR + halfHeight * vUpR;
    p[3] = pos + halfWidth * vRightR + halfHeight * vUpR;

    float2 baseUV[4] = {
        float2(0.0f, 1.0f),
        float2(1.0f, 1.0f),
        float2(0.0f, 0.0f),
        float2(1.0f, 0.0f)
    };

    PSInput output;
    [unroll]
    for (int i = 0; i < 4; ++i) {
        float4 clipPos = mul(float4(p[i], 1.0f), matViewProj);
        output.pos = clipPos;
        output.clipPos = clipPos;
        output.worldPos = p[i];
        output.normalW = vLook;
        output.uv = baseUV[i] * uvRect.zw + uvRect.xy;
        output.vertexColor = input[0].tint;
        triStream.Append(output);
    }
}

float4 PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET {
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

    float3 viewDir = normalize(cameraPosW - input.worldPos);
    float fresnel = pow(saturate(1.0f - dot(normalize(input.normalW), viewDir)), fresnelPower);
    fresnel = saturate(fresnel * fresnelScale) * (isFrontFace ? 1.0f : 0.0f);

    float softDepthFade = calcSoftParticleFade(input.clipPos);
    float fresnelDepth = 1.0f + fresnel - softDepthFade;

    float3 fresnelColorInput = (useOnlyColor > 0.5f)
                              ? float3(textureAlpha, textureAlpha, textureAlpha)
                              : textureColor;
    float3 outColor = (useFresnel > 0.5f)
                    ? max(fresnelColorInput, float3(fresnelDepth, fresnelDepth, fresnelDepth))
                    : textureColor;

    if (useCenterGlow > 0.5f) {
        float centerCoord = uv.x;
        float3 centerGlow = saturate(maskSample.rgb - float3(1.0f - centerCoord, 1.0f - centerCoord, 1.0f - centerCoord))
                          * maskSample.rgb;
        outColor *= centerGlow;
    }

    outColor *= color.rgb * emission * input.vertexColor.rgb;

    float fresnelAlpha = saturate(textureAlpha * textureOpacity + fresnelDepth);
    if (multiplyTexture > 0.5f)
        fresnelAlpha *= textureAlpha;

    float outAlpha = (useFresnel > 0.5f) ? fresnelAlpha : textureAlpha;
    if (useDepth > 0.5f)
        outAlpha *= softDepthFade;

    outAlpha = saturate(outAlpha * color.a * input.vertexColor.a * opacity);

    return float4(outColor, outAlpha);
}
