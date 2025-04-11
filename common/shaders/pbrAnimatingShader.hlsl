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
    uint padding;
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
    uint animIdx0;  // animIdx represents the first bone index of the animation
    uint animIdx1;
    float4 animWeight0;
    float4 animWeight1;
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

    float4x4 anim0 = blendBoneTransform(gInstances[instanceBase + instanceOffset].animIdx0, boneIndices, boneWeights);
    float4x4 anim1 = blendBoneTransform(gInstances[instanceBase + instanceOffset].animIdx1, boneIndices, boneWeights);
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