struct PerInstanceData {
    float4x4 world;
};

StructuredBuffer<PerInstanceData> gInstances: register(t0);

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

VSOutput VSMain(float3 position : POSITION, uint instanceOffset : SV_InstanceID) {
	VSOutput result;

    result.pos = mul(
        mul(float4(position, 1.0f), gInstances[instanceBase + instanceOffset].world),
        lightVP
    );

	return result;
}