cbuffer PerDrawcallData : register(b0) {
    float4x4 wvp;
    float4 color;
};

float4 VSMain(float3 position : POSITION, float2 uv : UV) : SV_POSITION {
    return mul(float4(position, 1.0f), wvp);
}

float4 PSMain() : SV_TARGET {
    return color;
}