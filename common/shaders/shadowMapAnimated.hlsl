struct PerInstanceData {
    float4x4 world;
    uint animIdx0;  // animIdx represents the first bone index of the animation
    uint animIdx1;
    float animWeight0;
    float animWeight1;
};

StructuredBuffer<PerInstanceData> gInstances: register(t0);
StructuredBuffer<float4x4> gToBoneLocal: register(t3);
StructuredBuffer<float4x4> gBones: register(t4);

cbuffer PerDrawcallData : register(b1) {
    uint instanceBase;
    uint padding[3];
};

cbuffer PerFrameData : register(b2) {
    float4x4 lightVP;
};

struct VSOutput {
	float4 pos : SV_POSITION;
};

float4x4 blendBoneTransform(uint animIdx, uint4 boneIndices, float4 weights) {
    uint idx0 = animIdx + boneIndices.x;
    uint idx1 = animIdx + boneIndices.y;
    uint idx2 = animIdx + boneIndices.z;
    uint idx3 = animIdx + boneIndices.w;

    return 
        mul(gToBoneLocal[idx0], gBones[idx0]) * weights.x +
        mul(gToBoneLocal[idx1], gBones[idx1]) * weights.y +
        mul(gToBoneLocal[idx2], gBones[idx2]) * weights.z +
        mul(gToBoneLocal[idx3], gBones[idx3]) * weights.w;
}

VSOutput VSMain( float3 position : POSITION, uint4 boneIndices : BONE_INDICES,
    float4 boneWeights : BONE_WEIGHTS, uint instanceOffset : SV_InstanceID
) {
	VSOutput result;

    float4x4 anim0 = blendBoneTransform(gInstances[instanceBase + instanceOffset].animIdx0, boneIndices, boneWeights);
    float4x4 anim1 = blendBoneTransform(gInstances[instanceBase + instanceOffset].animIdx1, boneIndices, boneWeights);
    float4x4 anim = anim0 * gInstances[instanceBase + instanceOffset].animWeight0
        + anim1 * gInstances[instanceBase + instanceOffset].animWeight1;

    float4 animPos = mul(float4(position, 1.0f), anim);

    result.pos = mul(
        mul(animPos, gInstances[instanceBase + instanceOffset].world),
        lightVP
    );

	return result;
}