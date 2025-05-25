#include "bindless.hlsl"

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
	float4x4 world;
    float4x4 wv;
};

cbuffer PerDrawcallData : register(b1) {
    Material material;
    uint4 heightMapRef;
    uint instanceBase;
    uint shadowSamplerIdx;
    uint heightMapSamplerIdx;
    uint samplerIdx;
    float2 tileScale;
    float2 padding;
};

cbuffer PerFrameData : register(b2) {
    float4x4 lightVP[3];
};

StructuredBuffer<PerInstanceData> gInstances: register(t0);

struct VSOutput 
{
    float3 pos : POSITION;
	float2 texcoord : TEXCOORD;
	nointerpolation uint instanceOffset : INSTANCE_OFFSET;
};

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
[partitioning("pow2")]
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
	float3 pos00 = mul(float4(input[0].pos, 1.0f), gInstances[instanceBase + input[0].instanceOffset].wv).xyz;
	float3 pos01 = mul(float4(input[1].pos, 1.0f), gInstances[instanceBase + input[1].instanceOffset].wv).xyz;
	float3 pos10 = mul(float4(input[2].pos, 1.0f), gInstances[instanceBase + input[2].instanceOffset].wv).xyz;
	float3 pos11 = mul(float4(input[3].pos, 1.0f), gInstances[instanceBase + input[3].instanceOffset].wv).xyz;

    float Len00 = length(pos00);
    float Len01 = length(pos01);
    float Len10 = length(pos10);
    float Len11 = length(pos11);

	float zMin = 0.01f;
	float zMax = 160.f;

	// 다시 볼만한 코드
    float Distance00 = clamp((Len00 - zMin) / (zMax - zMin), 0.0, 1.0);
    float Distance01 = clamp((Len01 - zMin) / (zMax - zMin), 0.0, 1.0);
    float Distance10 = clamp((Len10 - zMin) / (zMax - zMin), 0.0, 1.0);
    float Distance11 = clamp((Len11 - zMin) / (zMax - zMin), 0.0, 1.0);
    Distance00 *= Distance00;
    Distance01 *= Distance01;
    Distance10 *= Distance10;
    Distance11 *= Distance11;

    const int MIN_TESS_LEVEL = 1;
    const int MAX_TESS_LEVEL = 10;

    float TessLevel0 = min( lerp( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(Distance10, Distance00) ), 6.f );
    float TessLevel1 = min( lerp( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(Distance00, Distance01) ), 6.f );
    float TessLevel2 = min( lerp( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(Distance01, Distance11) ), 6.f );
    float TessLevel3 = min( lerp( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(Distance11, Distance10) ), 6.f );

    output.tessEdgeFactors[0] = pow(2.f, TessLevel0) - 1.f;
    output.tessEdgeFactors[1] = pow(2.f, TessLevel1) - 1.f;
    output.tessEdgeFactors[2] = pow(2.f, TessLevel2) - 1.f;
    output.tessEdgeFactors[3] = pow(2.f, TessLevel3) - 1.f;

    output.tessInsideFactors[0] = max(TessLevel1, TessLevel3);
    output.tessInsideFactors[1] = max(TessLevel0, TessLevel2);

	output.instanceOffset = input[0].instanceOffset;

	return output;
}

struct DSOutput {
	float4 pos : SV_POSITION;
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

    float2 t0 = lerp(t00, t01, uv.x); 	 // interpolate bottom
    float2 t1 = lerp(t10, t11, uv.x);    // interpolate top
    float2 texcoord = lerp(t0, t1, uv.y);          // final interpolation

    // sample the height from the height map
	float4 heightPixel = sampleLevelFromMapRef(heightMapRef, texcoord, 0.f, heightMapSamplerIdx);
    float height = heightPixel.a / 16777216.f
        + heightPixel.b / 65536.f
        + heightPixel.g / 256.f
        + heightPixel.r;
    height *= 200.f;

    // get the position for each vertex
    float3 p00 = patch[0].pos;
    float3 p01 = patch[1].pos;
    float3 p10 = patch[2].pos;
    float3 p11 = patch[3].pos;

    // same interpolation as the previous one
    float3 p0 = lerp(p00, p01, uv.x);
    float3 p1 = lerp(p10, p11, uv.x);
    output.pos = float4( lerp(p0, p1, uv.y), 1.f );
    
    output.pos.y += height;  // add the sampled height

    // transform from local to light space
	output.pos = mul(output.pos, gInstances[instanceBase + input.instanceOffset].world);

	return output;
}

struct GS_OUTPUT
{
    float4 position : SV_Position;
    uint rtIndex : SV_RenderTargetArrayIndex;
};

[maxvertexcount(9)]
void GSMain(triangle DSOutput input[3], inout TriangleStream<GS_OUTPUT> triStream)
{    
    for (int cascadeLevel = 0; cascadeLevel < 3; cascadeLevel++)
    {
    
        for (int vIdx = 0; vIdx < 3; vIdx++)
        {
            GS_OUTPUT output;
            output.position = mul(input[vIdx].pos, lightVP[cascadeLevel]);
            output.rtIndex = cascadeLevel;
            triStream.Append(output);
        }
    
        triStream.RestartStrip();
    }
}