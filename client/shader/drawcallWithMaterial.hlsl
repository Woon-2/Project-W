cbuffer PerDrawCallData : register(b0)
{
    Material gMaterial;
    uint gInstanceIndex;
    float3 padding;
};

uint getInstanceIdx() {
    return gInstanceIndex;
}