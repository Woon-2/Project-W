#include "bindless.hlsl"

struct PerInstanceData {
    float4x4 world;
    uint animIdx0;  // animIdx represents the presampled matrices texture's bindless index.
    uint animIdx1;
    float animWeight0;
    float animWeight1;
    int sampleIdx0; // the time index to sample matrix as texels' x coordinate.
                    // (x: time, y: bone)
    int sampleIdx1;
    uint boneCnt;
    uint skeletonIdx; // skeletonIdx represents the first toLocalMatrix index.
                    // animation matrices are spread over the animation instances,
                    // but toLocal matrices are shared by animation instances which shares the same skeleton.
};

StructuredBuffer<PerInstanceData> gInstances: register(t0);
StructuredBuffer<float4x4> gToBoneLocal: register(t3);

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

float4x4 loadPresampledMatrix(uint animIdx, int sampleIdx, int boneIdx) {
    float4x4 M;
    int vIdx = boneIdx * 4;
    // as a texture stores float4 values, we need to load 4x4 matrix as 4 float4 values.
    M[0] = loadFromMapRef( uint4(MAP_TYPE_TEXTURE2D, animIdx, 0, 0),
        int2(sampleIdx, vIdx + 0), 0
    );
    M[1] = loadFromMapRef( uint4(MAP_TYPE_TEXTURE2D, animIdx, 0, 0),
        int2(sampleIdx, vIdx + 1), 0
    );
    M[2] = loadFromMapRef( uint4(MAP_TYPE_TEXTURE2D, animIdx, 0, 0),
        int2(sampleIdx, vIdx + 2), 0
    );
    M[3] = loadFromMapRef( uint4(MAP_TYPE_TEXTURE2D, animIdx, 0, 0),
        int2(sampleIdx, vIdx + 3), 0
    );

    return M;
}

float4x4 blendPresampledBoneTransform( uint animIdx, int sampleIdx, uint boneCnt,
    uint skIdx, uint4 boneIndices, float4 weights
) {
    float4x4 M0 = loadPresampledMatrix(animIdx, sampleIdx, boneIndices.x);
    float4x4 M1 = loadPresampledMatrix(animIdx, sampleIdx, boneIndices.y);
    float4x4 M2 = loadPresampledMatrix(animIdx, sampleIdx, boneIndices.z);
    float4x4 M3 = loadPresampledMatrix(animIdx, sampleIdx, boneIndices.w);

    return 
        mul(gToBoneLocal[skIdx + boneIndices.x], M0) * weights.x +
        mul(gToBoneLocal[skIdx + boneIndices.y], M1) * weights.y +
        mul(gToBoneLocal[skIdx + boneIndices.z], M2) * weights.z +
        mul(gToBoneLocal[skIdx + boneIndices.w], M3) * weights.w;
}

VSOutput VSMain( float3 position : POSITION, uint4 boneIndices : BONE_INDICES,
    float4 boneWeights : BONE_WEIGHTS, uint instanceOffset : SV_InstanceID
) {
	VSOutput result;

    float4x4 anim0 = blendPresampledBoneTransform(
        gInstances[instanceBase + instanceOffset].animIdx0,
        gInstances[instanceBase + instanceOffset].sampleIdx0,
        gInstances[instanceBase + instanceOffset].boneCnt,
        gInstances[instanceBase + instanceOffset].skeletonIdx,
        boneIndices,
        boneWeights
    );

    float4x4 anim1 = blendPresampledBoneTransform(
        gInstances[instanceBase + instanceOffset].animIdx1,
        gInstances[instanceBase + instanceOffset].sampleIdx1,
        gInstances[instanceBase + instanceOffset].boneCnt,
        gInstances[instanceBase + instanceOffset].skeletonIdx,
        boneIndices,
        boneWeights
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