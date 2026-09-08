struct PerInstanceData {
    float4x4 world;
    uint rootBoneOffset;
    int bakedClipId;
    int bakedClipFrame;
    int padding;
};

cbuffer PerDrawcallData : register(b0) {
    uint firstInstanceOffset;
    uint3 padding;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP;       // maps CAMERA-RELATIVE world positions (posW - camPos) to light NDC
    uint     cascadeIdx;
    uint3    _pfd0;
    float3   camPos;        // camera world position for the camera-relative shadow rebase
    float    _pfd1;
}

StructuredBuffer<PerInstanceData> gInstances : register(t0);
StructuredBuffer<float4x4> gBoneData: register(t2);

#include "bindless.hlsli"

float4x4 loadBakedMatrix(int clipIdx, int frameIdx, int boneIdx) {
    float4x4 M;
    int vIdx = boneIdx * 4;
    // as a texture stores float4 values, we need to load 4x4 matrix as 4 float4 values.
    M[0] = loadBindless(
        int4(IDX_RANGE_TEXTURE2D, clipIdx, 0, 0),
        int2(frameIdx, vIdx + 0), 0
    );
    M[1] = loadBindless(
        int4(IDX_RANGE_TEXTURE2D, clipIdx, 0, 0),
        int2(frameIdx, vIdx + 1), 0
    );
    M[2] = loadBindless(
        int4(IDX_RANGE_TEXTURE2D, clipIdx, 0, 0),
        int2(frameIdx, vIdx + 2), 0
    );
    M[3] = loadBindless(
        int4(IDX_RANGE_TEXTURE2D, clipIdx, 0, 0),
        int2(frameIdx, vIdx + 3), 0
    );

    return M;
}

float4x4 blendBakedTransform(int clipIdx, int frameIdx, uint4 boneIndices, float4 boneWeights)
{
    return
        loadBakedMatrix(clipIdx, frameIdx, boneIndices.x) * boneWeights.x +
        loadBakedMatrix(clipIdx, frameIdx, boneIndices.y) * boneWeights.y +
        loadBakedMatrix(clipIdx, frameIdx, boneIndices.z) * boneWeights.z +
        loadBakedMatrix(clipIdx, frameIdx, boneIndices.w) * boneWeights.w;
}


float4x4 blendBoneTransform(uint rootBoneOffset, uint4 boneIndices, float4 boneWeights) {
    return
        gBoneData[rootBoneOffset + boneIndices.x] * boneWeights.x +
        gBoneData[rootBoneOffset + boneIndices.y] * boneWeights.y +
        gBoneData[rootBoneOffset + boneIndices.z] * boneWeights.z +
        gBoneData[rootBoneOffset + boneIndices.w] * boneWeights.w;
}

// VS: applies bone transform and projects directly into light clip space for the current cascade.
// No GS: each cascade is rendered in a separate pass (separate Texture2D per cascade).
float4 VSMain(
    float3 position : POSITION,
    int4 boneIndices: BONE_INDICES,
    float4 boneWeights: BONE_WEIGHTS,
    uint idxInst : SV_InstanceID
) : SV_Position {
    uint idx = idxInst + firstInstanceOffset;
    float4x4 anim;

    if (gInstances[idx].bakedClipId == -1) {
        anim = blendBoneTransform(
            gInstances[idx].rootBoneOffset,
            (uint4)boneIndices, boneWeights
        );
    }
    else {
        anim = blendBakedTransform(
            gInstances[idx].bakedClipId,
            gInstances[idx].bakedClipFrame,
            (uint4)boneIndices, boneWeights
        );
    }
    float4 animatedPos = mul(float4(position, 1.0f), anim);
    float3 posW = mul(animatedPos, gInstances[idx].world).xyz;
    // Camera-relative shadow space: rebase by camPos to match the receiver side.
    return mul(float4(posW - camPos, 1.0f), lightVP);
}
// No PSMain: depth-only pass, NumRenderTargets = 0
