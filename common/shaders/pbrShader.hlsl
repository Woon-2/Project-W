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
};

StructuredBuffer<PerInstanceData> gInstances: register(t0);

#include "pbrLighting.hlsl"

struct VSOutput {
	float4 pos : SV_POSITION;
	float3 posV : POSITION_V;
	float3 normalV : NORMAL_V;
	float3 tangentV : TANGENT_V;
	float3 bitangentV : BITANGENT_V;
	float2 texcoord : TEXCOORD;
	nointerpolation uint instanceOffset : INSTANCE_OFFSET;
};

VSOutput VSMain( float3 position : POSITION, float3 normal : NORMAL,
	float3 tangent : TANGENT, float3 bitangent : BITANGENT,
	float2 texcoord : TEXCOORD, uint instanceOffset : SV_InstanceID
) {
	VSOutput result;

    result.pos = mul(float4(position, 1.0f), gInstances[instanceBase + instanceOffset].wvp);
	result.posV = mul(float4(position, 1.0f), gInstances[instanceBase + instanceOffset].wv).xyz;
	result.normalV = mul(normal, gInstances[instanceBase + instanceOffset].wvNormal).xyz;
	if (material.normalMapRef.x != uint(-1)) {
		result.tangentV = mul(tangent, gInstances[instanceBase + instanceOffset].wvNormal).xyz;
		result.bitangentV = mul(bitangent, gInstances[instanceBase + instanceOffset].wvNormal).xyz;
	}
	result.texcoord = texcoord;
	result.instanceOffset = instanceOffset;

	return result;
}

float4 PSMain(VSOutput input) : SV_TARGET {
	input.normalV = normalize(input.normalV);

	if (material.normalMapRef.x != uint(-1)) {
		input.tangentV = normalize(input.tangentV);
		input.bitangentV = normalize(input.bitangentV);

		float3 normal = sampleFromMapRef(material.normalMapRef, input.texcoord, samplerIdx).rgb;
		normal = normal * 2.0f - 1.0f;
		
		float3x3 TBN = float3x3(input.tangentV, input.bitangentV, input.normalV);
		input.normalV = mul(normal, TBN);
	}

    return illuminate(input.posV, input.normalV, input.texcoord);
}