#include "pbrLighting.hlsl"

cbuffer PerConfigurationData : register(b0) {
	float viewportWidth;
	float viewportHeight;
};

struct PerInstanceData {
    float4x4 wvp;
    float4x4 wv;
    float3x3 wvNormal;
};

StructuredBuffer<PerInstanceData> gInstances: register(t0);

struct VSOutput {
	float4 pos : SV_POSITION;
	float3 posV : POSITION_V;
	float3 normalV : NORMAL_V;
	float2 texcoord : TEXCOORD;
	nointerpolation uint instanceOffset : INSTANCE_OFFSET;
};

VSOutput VSMain( float3 position : POSITION, float3 normal : NORMAL,
	float2 texcoord : TEXCOORD, uint instanceOffset : SV_InstanceID
) {
	VSOutput result;

    result.pos = mul(float4(position, 1.0f), gInstances[instanceBase + instanceOffset].wvp);
	result.posV = mul(float4(position, 1.0f), gInstances[instanceBase + instanceOffset].wv).xyz;
	result.normalV = mul(normal, gInstances[instanceBase + instanceOffset].wvNormal).xyz;
	result.texcoord = texcoord;
	result.instanceOffset = instanceOffset;

	return result;
}

float4 PSMain(VSOutput input) : SV_TARGET {
	input.normalV = normalize(input.normalV);
    return illuminate(input.posV, input.normalV, input.texcoord);
}