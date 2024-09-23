cbuffer PerDrawCallData : register(b0)
{
    uint gInstanceIndex;
    float3 padding;
    Material gMaterial;
};

uint getInstanceIdx() {
    return gInstanceIndex;
}