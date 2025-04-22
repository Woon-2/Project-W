// MapRef.x: resource type, MapRef.y: resource index, MapRef.z: array index

struct Material {
    float4 albedoConstant;
    float roughnessConstant;
    float metallicConstant;
    float albedoConstantMapRatio;
    float roughnessConstantMapRatio;
    float metallicConstantMapRatio;
    float3 emmisiveConstant;
    float emmisiveConstantMapRatio;
    float ambientOcclusionConstant;
    float ambientOcclusionConstantMapRatio;
    float padding;
    uint4 albedoMapRef;
    uint4 roughnessMapRef;
    uint4 normalMapRef;
    uint4 metallicMapRef;
    uint4 metallicSmoothnessMapRef;
    uint4 emmisiveMapRef;
    uint4 ambientOcclusionMapRef;
};

cbuffer PerConfigurationData : register(b0) {
	float viewportWidth;
	float viewportHeight;
};

cbuffer PerDrawcallData : register(b1) {
    Material material;
    uint instanceBase;
    uint samplerIdx;
    uint shadowSamplerIdx;
};

cbuffer PerFrameData : register(b2) {
    float3 globalAmbient;
    float padding0;
    uint4 shadowMapRef;
    float4x4 lightVP;
    uint lightCnt;
    uint3 padding1;
};

struct PerInstanceData {
    float4x4 wvp;
	float4x4 world;
    float4x4 wv;
    float3x3 wvNormal;
    uint animIdx0;  // animIdx represents the first bone index of the animation in keyframe mode,
                    // and the presampled matrices texture's bindless index in presampled mode.
    uint animIdx1;
    float animWeight0;
    float animWeight1;
    int sampleIdx0; // for presampled mode, this is the time index to sample matrix as texels' x coordinate.
                      // (x: time, y: bone)
    int sampleIdx1;
    uint boneCnt;
    bool usePresampled;
};

StructuredBuffer<PerInstanceData> gInstances: register(t0);
StructuredBuffer<float4x4> gBones: register(t2);

#include "pbrLighting.hlsl"

struct VSOutput {
	float4 pos : SV_POSITION;
	float3 posV : POSITION_V;
	float4 posL : POSITION_L;
	float3 normalV : NORMAL_V;
	float3 tangentV : TANGENT_V;
	float3 bitangentV : BITANGENT_V;
	float2 texcoord : TEXCOORD;
	nointerpolation uint instanceOffset : INSTANCE_OFFSET;
};

static float4x4 gmtxTexturize = {
	0.5f, 0.0f, 0.0f, 0.0f,
	0.0f, -0.5f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.5f, 0.5f, 0.0f, 1.0f
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
    uint4 boneIndices, float4 weights
) {
    float4x4 M0 = loadPresampledMatrix(animIdx, sampleIdx, boneIndices.x);
    float4x4 M1 = loadPresampledMatrix(animIdx, sampleIdx, boneIndices.y);
    float4x4 M2 = loadPresampledMatrix(animIdx, sampleIdx, boneIndices.z);
    float4x4 M3 = loadPresampledMatrix(animIdx, sampleIdx, boneIndices.w);

    return 
        M0 * weights.x +
        M1 * weights.y +
        M2 * weights.z +
        M3 * weights.w;
}

float4x4 blendBoneTransform(uint animIdx, uint4 boneIndices, float4 weights) {
    return 
        gBones[animIdx + boneIndices.x] * weights.x +
        gBones[animIdx + boneIndices.y] * weights.y +
        gBones[animIdx + boneIndices.z] * weights.z +
        gBones[animIdx + boneIndices.w] * weights.w;
}

VSOutput VSMain( float3 position : POSITION, float3 normal : NORMAL,
	float3 tangent : TANGENT, float3 bitangent : BITANGENT,
	float2 texcoord : TEXCOORD, uint4 boneIndices : BONE_INDICES,
    float4 boneWeights : BONE_WEIGHTS, uint instanceOffset : SV_InstanceID
) {
	VSOutput result;

    float4x4 anim0;
    float4x4 anim1;

    if (gInstances[instanceBase + instanceOffset].usePresampled) {
        anim0 = blendPresampledBoneTransform(
            gInstances[instanceBase + instanceOffset].animIdx0,
            gInstances[instanceBase + instanceOffset].sampleIdx0,
            gInstances[instanceBase + instanceOffset].boneCnt,
            boneIndices, boneWeights
        );
        anim1 = blendPresampledBoneTransform(
            gInstances[instanceBase + instanceOffset].animIdx1,
            gInstances[instanceBase + instanceOffset].sampleIdx1,
            gInstances[instanceBase + instanceOffset].boneCnt,
            boneIndices, boneWeights
        );
    }
    else {
        anim0 = blendBoneTransform(
            gInstances[instanceBase + instanceOffset].animIdx0,
            boneIndices, boneWeights
        );
        anim1 = blendBoneTransform(
            gInstances[instanceBase + instanceOffset].animIdx1,
            boneIndices, boneWeights
        );
    }
    float4x4 anim = anim0 * gInstances[instanceBase + instanceOffset].animWeight0
        + anim1 * gInstances[instanceBase + instanceOffset].animWeight1;

    float4 animPos = mul(float4(position, 1.0f), anim);
    // we assume that the animation matrix doesn't have any scaling, so we can use the normal matrix directly.
    float4 animNormal = mul(float4(normal, 0.0f), anim);

    result.pos = mul(animPos, gInstances[instanceBase + instanceOffset].wvp);
    result.posV = mul(animPos, gInstances[instanceBase + instanceOffset].wv).xyz;
    result.posL = mul( mul(animPos, gInstances[instanceBase + instanceOffset].world), lightVP );

    result.normalV = mul(animNormal, gInstances[instanceBase + instanceOffset].wvNormal).xyz;
    if (material.normalMapRef.x != uint(-1)) {
        float4 animTangent = mul(float4(tangent, 0.0f), animNormal);
        float4 animBitangent = mul(float4(bitangent, 0.0f), animNormal);
        result.tangentV = mul(animTangent, gInstances[instanceBase + instanceOffset].wvNormal).xyz;
        result.bitangentV = mul(animBitangent, gInstances[instanceBase + instanceOffset].wvNormal).xyz;
    }

    result.texcoord = texcoord;
    result.instanceOffset = instanceOffset;

	return result;
}

float4 PSMain(VSOutput input) : SV_TARGET {
	input.normalV = normalize(input.normalV + 1e-5f);

	if (material.normalMapRef.x != uint(-1)) {
		input.tangentV = normalize(input.tangentV);
		input.bitangentV = normalize(input.bitangentV);

		float3 normal = sampleFromMapRef(material.normalMapRef, input.texcoord, samplerIdx).rgb;
		normal = normal * 2.0f - 1.0f;
		
		float3x3 TBN = float3x3(input.tangentV, input.bitangentV, input.normalV);
		input.normalV = mul(normal, TBN);
	}

    return illuminate(input.posV, input.posL, input.normalV, input.texcoord);
}