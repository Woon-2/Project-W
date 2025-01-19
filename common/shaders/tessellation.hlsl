#include "samplers.hlsl"

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

struct PerInstanceData {
	float4x4 wvp;
    float4x4 wv;
    float3x3 wvNormal;
};

cbuffer PerDrawcallData : register(b1) {
    Material material;
    uint4 heightMapRef;
    uint instanceBase;
    uint shadowSamplerIdx;
    uint heightMapSamplerIdx;
    uint samplerIdx;
};

cbuffer PerFrameData : register(b2) {
    float3 globalAmbient;
    float padding0;
    uint4 shadowMapRef;
    float4x4 lightVP;
    uint lightCnt;
    uint3 padding1;
};

StructuredBuffer<PerInstanceData> gInstances: register(t0);

#include "pbrLighting.hlsl"

struct VSOutPut 
{
    float3 pos : POSITION;
	float2 texcoord : TEXCOORD;
	nointerpolation uint instanceOffset : INSTANCE_OFFSET;
}

VSOutput VSMain( float3 position : POSITION, float2 texcoord : TEXCOORD, uint instanceOffset : SV_InstanceID) {
	VSOutput result;

	result.pos = position;
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
	float3 pos : POSITION;
	float2 texcoord : TEXCOORD;
};

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(4)]
[patchconstantfunc("HSConstant")]
[maxtessfactor(64.0f)]
HSOutput HSMain(InputPatch<VSOutput, 4> input, uint i : SV_OutputControlPointID) {
	HSOutput result;

	result.pos = input[i].pos;
	result.texcoord = input[i].texcoord;
	
    return result;
}

HSConstantOutput HSConstant(InputPatch<VSOutput, 4> input) 
{
	HSConstantOutput output;

	// 정점 위치를 뷰 공간으로 전환
	intput[0].pos = mul(float4(intput[0].pos, 1.0f), gInstances[instanceBase + instanceOffset].wv).xyz;
	intput[1].pos = mul(float4(intput[1].pos, 1.0f), gInstances[instanceBase + instanceOffset].wv).xyz;
	intput[2].pos = mul(float4(intput[2].pos, 1.0f), gInstances[instanceBase + instanceOffset].wv).xyz;
	intput[3].pos = mul(float4(intput[3].pos, 1.0f), gInstances[instanceBase + instanceOffset].wv).xyz;

    float Len00 = length(input[0].pos);
    float Len01 = length(input[1].pos);
    float Len10 = length(input[2].pos);
    float Len11 = length(input[3].pos);

	float zMin = 0.1f;
	float zMax = 1000.f;

	// 다시 볼만한 코드
    float Distance00 = clamp((Len00 - zMin) / (zMax - zMin), 0.0, 1.0);
    float Distance01 = clamp((Len01 - zMin) / (zMax - zMin), 0.0, 1.0);
    float Distance10 = clamp((Len10 - zMin) / (zMax - zMin), 0.0, 1.0);
    float Distance11 = clamp((Len11 - zMin) / (zMax - zMin), 0.0, 1.0);

    const int MIN_TESS_LEVEL = 1;
    const int MAX_TESS_LEVEL = 7;

    float TessLevel0 = lerp( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(Distance10, Distance00) );
    float TessLevel1 = lerp( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(Distance00, Distance01) );
    float TessLevel2 = lerp( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(Distance01, Distance11) );
    float TessLevel3 = lerp( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(Distance11, Distance10) );

    output.tessEdgeFactors[0] = TessLevel0;
    output.tessEdgeFactors[1] = TessLevel1;
    output.tessEdgeFactors[2] = TessLevel2;
    output.tessEdgeFactors[3] = TessLevel3;

    output.tessInsideFactors[0] = max(TessLevel1, TessLevel3);
    output.tessInsideFactors[1] = max(TessLevel0, TessLevel2);

	output.instanceOffset = input[0].instanceOffset;

	return output;
}

struct DSOutput {
	float4 pos : SV_POSITION;
	float3 normalV : NORMAL_V;
	float2 texcoord : TEXCOORD;
	nointerpolation uint instanceOffset : INSTANCE_OFFSET;
};

[domain("quad")]
DSOutput DSMain(HSConstantOutput input, float2 uv : SV_DomainLocation, const OutputPatch<HSOutput, 4> patch) 
{
	DSOutput output;

	// get the results from the tessellator
    // float u = gl_TessCoord.x;
    // float v = gl_TessCoord.y;
	// SV_DomainLocation에 좌표계가 포함되어있음

    // get the texture coordinate for each vertex
	// 각 Vertex는 OutputPatch에 해당함
    float2 t00 = patch[0].texcoord;     // bottom left
    float2 t01 = patch[1].texcoord;     // bottom right
    float2 t10 = patch[2].texcoord;     // top left
    float2 t11 = patch[3].texcoord;     // top right

    float2 t0 = lerp(t00, t01, uv.u); 	 // interpolate bottom
    float2 t1 = lerp(t10, t11, uv.u);    // interpolate top
    output.texcoord = lerp(t0, t1, uv.v);          // final interpolation

    // sample the height from the height map
	float height = sampleFromMapRef(heightMapRef, output.texcoord, heightMapSamplerIdx).r;

    // get the position for each vertex
    float3 p00 = patch[0].pos;
    float3 p01 = patch[1].pos;
    float3 p10 = patch[2].pos;
    float3 p11 = patch[3].pos;

    // same interpolation as the previous one
    float3 p0 = lerp(p00, p01, uv.u);
    float3 p1 = lerp(p10, p11, uv.u);
    output.pos = lerp(p0, p1, uv.v);
    
    output.pos.y += height;  // add the sampled height

    // transform from local to clip space
	output.pos = mul(float4(output.pos, 1.0f), gInstances[instanceBase + instanceOffset].wvp).xyz;

	float lHeight = sampleFromMapRef(heightMapRef, output.texcoord, int2(-1, 0) heightMapSamplerIdx).r;
	float tHeight = sampleFromMapRef(heightMapRef, output.texcoord, int2(0, 1) heightMapSamplerIdx).r;
	float rHeight = sampleFromMapRef(heightMapRef, output.texcoord, int2(1, 0) heightMapSamplerIdx).r;
	float bHeight = sampleFromMapRef(heightMapRef, output.texcoord, int2(0, -1) heightMapSamplerIdx).r;

	float3 normalM = cross(rHeight - lHeight, tHeight - bHeight);
	const float epsilon = 0.0005f;
	if (length(normalM < epsilon)) {
		normalM = float3(0.f, 1.f, 0.f);
	} 
	else {
		normalM = normalize(normalM);
	}

	output.normalV = mul( float4(normalM, 0.f), wvNormal ).xyz;

	return output;
}

float4 PSMain(DSOutput input) : SV_TARGET {
	input.normalV = normalize(input.normalV);

    return illuminate(input.posV, input.normalV, input.texcoord);
}
