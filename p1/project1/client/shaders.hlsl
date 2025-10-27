struct PerInstanceData {
    float4x4 wvp;
};

cbuffer PerDrawcallData : register(b0) {
    uint idxDrawcall;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

float4 VSMain( float3 position : POSITION, float2 uv : UV,
    uint idxInst : SV_InstanceID
) : SV_POSITION {
    return mul(float4(position, 1.0f), gInstances[idxInst + idxDrawcall].wvp);
}

float4 PSMain() : SV_TARGET {
    return float4(1.f, 0.f, 0.f, 1.f);
}