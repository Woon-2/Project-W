cbuffer PerDrawcallData : register(b0) {
    float4 color;
};

float4 VSMain(float3 position : POSITION) : SV_POSITION {
    return float4(position, 1.0f);
}

float4 PSMain() : SV_TARGET {
    return color;
}