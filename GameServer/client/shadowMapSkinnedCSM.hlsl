struct PerInstanceData {
    float4x4 world;
    uint rootBoneOffset;
    uint3 padding;
};

cbuffer PerDrawcallData : register(b0) {
    uint firstInstanceOffset;
    uint3 padding;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP;
    uint     cascadeIdx;
    uint3    _pfd0;
}

StructuredBuffer<PerInstanceData> gInstances : register(t0);
StructuredBuffer<float4x4> gBoneData: register(t2);

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
    float4x4 anim = blendBoneTransform(
        gInstances[idxInst + firstInstanceOffset].rootBoneOffset,
        boneIndices, boneWeights
    );
    float4 animatedPos = mul(float4(position, 1.0f), anim);
    float4 posW = mul(animatedPos, gInstances[idxInst + firstInstanceOffset].world);
    return mul(posW, lightVP);
}
// No PSMain: depth-only pass, NumRenderTargets = 0
