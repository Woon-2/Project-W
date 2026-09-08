// shadowMapSkinned.hlsl
// Non-CSM single-cascade shadow map shader (skinned meshes).
// Kept for reference; active CSM path uses shadowMapSkinnedCSM.hlsl.
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
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);
StructuredBuffer<float4x4> gBoneData: register(t2);

float4x4 blendBoneTransform(uint rootBoneOffset, uint4 boneIndices, float4 boneWeights) {
    return
        gBoneData[rootBoneOffset + boneIndices.x] * boneWeights.x +
        gBoneData[rootBoneOffset + boneIndices.y] * boneWeights.y +
        gBoneData[rootBoneOffset + boneIndices.z] * boneWeights.z +
        gBoneData[rootBoneOffset + boneIndices.w] * boneWeights.w;
}

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
    return mul(mul(animatedPos, gInstances[idxInst + firstInstanceOffset].world), lightVP);
}
// No PSMain: depth-only pass, NumRenderTargets = 0
