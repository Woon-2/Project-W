struct PerInstanceData {
    float4x4 world;
};

struct VSOutput {
    float4 pos : SV_Position;
};

cbuffer PerDrawcallData : register(b0) {
    uint idxDrawcall;
    uint3 padding;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP;
}

StructuredBuffer<PerInstanceData> gInstances : register(t0);

VSOutput VSMain(
    float3 position : POSITION,
    uint idxInst : SV_InstanceID
) {
    VSOutput ret;
    
    ret.pos = mul(
        mul(float4(position, 1.0f), gInstances[idxInst + idxDrawcall].world),
        lightVP
    );
    
    return ret;
}