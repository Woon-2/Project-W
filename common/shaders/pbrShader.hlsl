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

// referenced from
// https://thebookofshaders.com/10/
float nrand(float2 n) {
    return frac(sin(dot(n.xy, float2(12.9898, 78.233))) * 43758.5453);
}

float n2rand(float2 n,float c) {
    float t = frac(c*255);
    float nrnd0 = nrand(n + 0.07 * t);
    float nrnd1 = nrand(n + 0.11 * t);
    return (nrnd0 + nrnd1)-1.0f;
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

    float4 source = illuminate(input.posV, input.normalV, input.texcoord);
	float3 final = source.rgb;

	// tone mapping
	final = final / (1 + final);
	final = pow(abs(final), 1.f / 2.2f);
    float2 seed = input.pos.xy / float2(viewportWidth, viewportHeight);
    float3 noise = float3( n2rand(seed, final.r), n2rand(seed, final.g), n2rand(seed, final.b) ) / 255.f;
    final += noise;

	return float4(final, source.a);
}