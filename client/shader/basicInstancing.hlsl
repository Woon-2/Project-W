struct PerInstanceData {
    matrix wv : WV;
    matrix wvp : WVP;
    float3x3 normalXform : NORMALXFORM;
};

StructuredBuffer<PerInstanceData> gPerInstanceData : register(t0);

PerInstanceData getInstanceData(uint instID) {
    return gPerInstanceData[getInstanceIdx() + instID];
}