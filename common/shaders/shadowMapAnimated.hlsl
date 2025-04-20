struct PerInstanceData {
    float4x4 world;
    uint animIdx0;  // animIdx represents the first bone index of the animation
    uint animIdx1;
    float animWeight0;
    float animWeight1;
    uint skeletonIdx; // skeletonIdx represents the first toLocalMatrix index.
                    // animation matrices are spread over the animation instances,
                    // but toLocal matrices are shared by animation instances which shares the same skeleton.
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

float4x4 blendBoneTransform(uint animIdx, uint skIdx, uint4 boneIndices, float4 weights) {
    return 
        mul(gToBoneLocal[skIdx + boneIndices.x], gBones[animIdx + boneIndices.x]) * weights.x +
        mul(gToBoneLocal[skIdx + boneIndices.y], gBones[animIdx + boneIndices.y]) * weights.y +
        mul(gToBoneLocal[skIdx + boneIndices.z], gBones[animIdx + boneIndices.z]) * weights.z +
        mul(gToBoneLocal[skIdx + boneIndices.w], gBones[animIdx + boneIndices.w]) * weights.w;
}

VSOutput VSMain( float3 position : POSITION, uint4 boneIndices : BONE_INDICES,
    float4 boneWeights : BONE_WEIGHTS, uint instanceOffset : SV_InstanceID
) {
	VSOutput result;

    float4x4 anim0 = blendBoneTransform(
        gInstances[instanceBase + instanceOffset].animIdx0,
        gInstances[instanceBase + instanceOffset].skeletonIdx,
        boneIndices, boneWeights
    );
    float4x4 anim1 = blendBoneTransform(
        gInstances[instanceBase + instanceOffset].animIdx1,
        gInstances[instanceBase + instanceOffset].skeletonIdx,
        boneIndices, boneWeights
    );
    float4x4 anim = anim0 * gInstances[instanceBase + instanceOffset].animWeight0
        + anim1 * gInstances[instanceBase + instanceOffset].animWeight1;

    float4 animPos = mul(float4(position, 1.0f), anim);

    result.pos = mul(
        mul(animPos, gInstances[instanceBase + instanceOffset].world),
        lightVP
    );

	return result;
}