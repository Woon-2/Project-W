struct PerInstanceData {
    float4x4 wvp;
    float4 color;
};

struct VSOutput {
    float4 pos : SV_Position;
    nointerpolation uint idxInst : INSTANCE_OFFSET;
};

cbuffer PerDrawcallData : register(b0) {
    uint idxDrawcall;
    uint3 padding;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

VSOutput VSMain(float3 position : POSITION, uint idxInst : SV_InstanceID) {
    VSOutput ret;
    
    ret.pos = mul(float4(position, 1.0f), gInstances[idxInst + idxDrawcall].wvp);
    ret.idxInst = idxInst;
    
    return ret;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    return gInstances[input.idxInst + idxDrawcall].color;
}