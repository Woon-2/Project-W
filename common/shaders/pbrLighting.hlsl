struct Light {
    float4 color;
    float3 posV;
    float falloff;
    float3 dirV;
    float cosTheta;
    float3 atten;
    float cosPhi;
    int type;
    float intensity;
    float2 padding;
};

struct Material {
    float3 albedoConstant;
    float roughnessConstant;
    float metallicConstant;
    float3 padding;
    uint4 albedoMapRef;
    uint4 roughnessMapRef;
    uint4 metallicMapRef;
};