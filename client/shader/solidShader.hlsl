cbuffer PerInstanceData : register(b0) {
    matrix wvp : WVP;
    float4 color : COLOR;
};

float4 VSMain(float3 pos : POSITION) : SV_POSITION {
    return mul(float4(pos, 1.f), wvp);
}

float4 PSMain() : SV_TARGET {
    return color;
}