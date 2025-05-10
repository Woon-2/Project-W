struct PerInstanceData {
    float4x4 world;
};

StructuredBuffer<PerInstanceData> gInstances: register(t0);

cbuffer PerDrawcallData : register(b1) {
    uint instanceBase;
    uint padding[3];
};

cbuffer PerFrameData : register(b2) {
    float4x4 lightVP[3];
};

struct VSOutput {
	float4 pos : SV_POSITION;
};

VSOutput VSMain(float3 position : POSITION, uint instanceOffset : SV_InstanceID) {
	VSOutput result;

    result.pos = mul(float4(position, 1.0f), gInstances[instanceBase + instanceOffset].world);

	return result;
}

struct GS_OUTPUT
{
    float4 position : SV_Position;
    uint rtIndex : SV_RenderTargetIndex;
};

[maxvertexcount(9)]
void GS_Main(triangle VSOutput input[3], inout TriangleStream<GS_OUTPUT> triStream)
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