cbuffer PerDrawCallData : register(b0)
{
    uint gInstanceIndex;
};

uint getInstanceIdx() {
    return gInstanceIndex;
}