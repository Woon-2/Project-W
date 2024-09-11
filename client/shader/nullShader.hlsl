float4 VSMain(float3 pos : POSITION) : SV_POSITION
{
    return float4(pos.x * 0.5f, pos.y * 0.5f, pos.z * 0.125f + 0.5f, 1.f);
}

float4 PSMain(float4 pos : SV_POSITION) : SV_TARGET
{
    return float4(0.74f, 0.74f, 0.16f, 1);
}