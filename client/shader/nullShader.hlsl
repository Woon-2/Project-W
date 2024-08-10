float4 VSMain(float3 pos : POSITION) : SV_POSITION
{
    return float4(pos, 1.f);
}

float4 PSMain(float4 pos : SV_POSITION) : SV_TARGET
{
    return float4(0.74f, 0.74f, 0.16f, 1);
}