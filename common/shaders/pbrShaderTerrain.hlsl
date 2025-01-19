#include "pbrLighting.hlsl"

cbuffer PerConfigurationData : register(b0) {
	float viewportWidth;
	float viewportHeight;
};

struct PerInstanceData {
    float4x4 wv;
	float4x4 proj;
    float3x3 wvNormal;
};

StructuredBuffer<PerInstanceData> gInstances: register(t0);

struct VSOutput {
	float3 posV : POSITION_V;
	float3 normalV : NORMAL_V;
	float2 texcoord : TEXCOORD;
	nointerpolation uint instanceOffset : INSTANCE_OFFSET;
};

VSOutput VSMain( float3 position : POSITION, float3 normal : NORMAL,
	float2 texcoord : TEXCOORD, uint instanceOffset : SV_InstanceID
) {
	VSOutput result;

	result.posV = mul(float4(position, 1.0f), gInstances[instanceBase + instanceOffset].wv).xyz;
	result.normalV = mul(normal, gInstances[instanceBase + instanceOffset].wvNormal).xyz;
	result.texcoord = texcoord;
	result.instanceOffset = instanceOffset;

	return result;
}

struct HSConstantOutput {
	float tessEdgeFactors[4] : SV_TessFactor;
	float tessInsideFactors[2] : SV_InsideTessFactor;
	nointerpolation uint instanceOffset : INSTANCE_OFFSET;
};

struct HSOutput {
	float3 posV : POSITION_V;
	float3 normalV : NORMAL_V;
	float2 texcoord : TEXCOORD;
};

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(16)]
[patchconstantfunc("HSConstant")]
[maxtessfactor(64.0f)]
HSOutput HSMain(InputPatch<VSOutput, 16> input, uint i : SV_OutputControlPointID) {
	HSOutput output;

	output.posV = input[i].posV;
	output.normalV = input[i].normalV;
	output.texcoord = input[i].texcoord;
	
    return output;
}

HSConstantOutput HSConstant(InputPatch<VSOutput, 16> input) {
	float3 center = ( input[0].posV + input[3].posV + input[12].posV + input[15].posV ) / 4.f;
	float dist = length(center);

	// camera's perspective configuration
	float zMin = 0.1f;
	float zMax = 1000.f;

	// float tessFactor = 1.f;
	float tessFactor = pow(2, lerp(4.f, 2.f, saturate((zMax - dist) / (zMax - zMin)))) - 1.f;

	HSConstantOutput output;
	output.tessEdgeFactors[0] = tessFactor;
	output.tessEdgeFactors[1] = tessFactor;
	output.tessEdgeFactors[2] = tessFactor;
	output.tessEdgeFactors[3] = tessFactor;
	output.tessInsideFactors[0] = tessFactor;
	output.tessInsideFactors[1] = tessFactor;
	output.instanceOffset = input[0].instanceOffset;

	return output;
}

static float4x4 gMtxCatmullRom = {
	0.f, 2.f, 0.f, 0.f,
	-1.f, 0.f, 1.f, 0.f,
	2.f, -5.f, 4.f, -1.f,
	-1.f, 3.f, -3.f, 1.f
};

float3 catmullRom(float3 p0, float3 p1, float3 p2, float3 p3, float t) {
	float t2 = t*t;
	float4 tVec = { 1.f, t, t2, t*t2 };
	float4x4 tMat = { float4(p0, 1.f), float4(p1, 1.f), float4(p2, 1.f), float4(p3, 1.f) };
	return mul(tVec, mul(gMtxCatmullRom, tMat)).xyz * 0.5f;
}

float3 catmullRomN(float3 p0, float3 p1, float3 p2, float3 p3, float t) {
	float t2 = t*t;
	float4 tVec = { 1.f, t, t2, t*t2 };
	float4x4 tMat = { float4(p0, 0.f), float4(p1, 0.f), float4(p2, 0.f), float4(p3, 0.f) };
	return mul(tVec, mul(gMtxCatmullRom, tMat)).xyz * 0.5f;
}

struct DSOutput {
	float4 pos : SV_POSITION;
	float3 posV : POSITION_V;
	float3 normalV : NORMAL_V;
	float2 texcoord : TEXCOORD;
	nointerpolation uint instanceOffset : INSTANCE_OFFSET;
};

[domain("quad")]
DSOutput DSMain(HSConstantOutput input, float2 uv : SV_DomainLocation, const OutputPatch<HSOutput, 16> patch) {
	DSOutput result;

	float3 p0 = catmullRom(patch[0].posV, patch[1].posV, patch[2].posV, patch[3].posV, uv.x);
	float3 p1 = catmullRom(patch[4].posV, patch[5].posV, patch[6].posV, patch[7].posV, uv.x);
	float3 p2 = catmullRom(patch[8].posV, patch[9].posV, patch[10].posV, patch[11].posV, uv.x);
	float3 p3 = catmullRom(patch[12].posV, patch[13].posV, patch[14].posV, patch[15].posV, uv.x);
	result.posV = catmullRom(p0, p1, p2, p3, uv.y);

	float3 n0 = catmullRomN(patch[0].normalV, patch[1].normalV, patch[2].normalV, patch[3].normalV, uv.x);
	float3 n1 = catmullRomN(patch[4].normalV, patch[5].normalV, patch[6].normalV, patch[7].normalV, uv.x);
	float3 n2 = catmullRomN(patch[8].normalV, patch[9].normalV, patch[10].normalV, patch[11].normalV, uv.x);
	float3 n3 = catmullRomN(patch[12].normalV, patch[13].normalV, patch[14].normalV, patch[15].normalV, uv.x);

	result.normalV = normalize(catmullRomN(n0, n1, n2, n3, uv.y));

	result.pos = mul(float4(result.posV, 1.0f), gInstances[instanceBase + input.instanceOffset].proj);
	result.texcoord = lerp(
		lerp(patch[0].texcoord, patch[3].texcoord, uv.x),
		lerp(patch[12].texcoord, patch[15].texcoord, uv.x),
		uv.y
	);
	result.instanceOffset = input.instanceOffset;

	return result;
}


float4 PSMain(DSOutput input) : SV_TARGET {
	input.normalV = normalize(input.normalV);
    return accumulateLighting(input.posV, input.normalV, input.texcoord);
}