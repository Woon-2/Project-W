cbuffer PerDrawCallData : register(b0)
{
    uint gInstanceIndex;
    float3 padding;
};

uint getInstanceIdx() {
    return gInstanceIndex;
}