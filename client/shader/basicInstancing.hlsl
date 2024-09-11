struct PerInstanceData {
    matrix wv : WV;
    matrix wvp : WVP;
    float3x3 normalXform : NORMALXFORM;
    uint matIdx : MATIDX;
    float3 padding;
};

StructuredBuffer<PerInstanceData> gPerInstanceData : register(t0);

PerInstanceData getInstanceData(uint instID) {
    return gPerInstanceData[getInstanceIdx() + instID];
}