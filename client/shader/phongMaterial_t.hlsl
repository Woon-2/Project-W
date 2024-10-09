struct Material
{
    uint textureFlag;
    uint ambientMapIdx;
    uint diffuseMapIdx;
    uint specularMapIdx;
    uint emmisiveMapIdx;
    float shininess;
    float2 padding;
};