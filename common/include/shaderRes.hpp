#ifndef __ShaderRes_HPP
#define __ShaderRes_HPP

#define DXMATH_VEC_UTIL
#define DXMATH_MAT_UTIL
#define DXMATH_QUAT_UTIL

#include "mathUtil.hpp"
#include <cstdint>

namespace gfx {

namespace d3d12 {

namespace sr {

struct PhongMaterialNT {
    dx::XMFLOAT4 ambient;
    dx::XMFLOAT4 diffuse;
    dx::XMFLOAT4 specular; // a = power
    dx::XMFLOAT4 emmisive;
};

struct PhongMaterial {
    static constexpr std::uint32_t textureFlagAmbient = 0x01u;
    static constexpr std::uint32_t textureFlagDiffuse = 0x02u;
    static constexpr std::uint32_t textureFlagSpecular = 0x04u;
    static constexpr std::uint32_t textureFlagEmmisive = 0x08u;

    std::uint32_t textureFlag;
    std::uint32_t ambientMapIdx;
    std::uint32_t diffuseMapIdx;
    std::uint32_t specularMapIdx;
    std::uint32_t emmisiveMapIdx;
    float shininess;
    float padding;
    float padding2;
};

struct BasicPFD {
    dx::XMFLOAT4 globalAmbientLight;
    std::uint32_t lightCnt;
    dx::XMFLOAT3 padding;
};

struct BasicPID {
    dx::XMFLOAT4X4 wv;
    dx::XMFLOAT4X4 wvp;
    dx::XMFLOAT3X3 normalXform; // use 3x4 matrix to avoid padding
};

struct PDDPhong {
    PhongMaterial material;
    std::uint32_t instanceIndex;
    std::uint32_t padding[3];
};

struct PDDNTPhong {
    PhongMaterialNT material;
    std::uint32_t instanceIndex;
    std::uint32_t padding[3];
};

struct PFDShadow {
    dx::XMFLOAT4X4 view2LightProj;
};

struct PDDShadow {
    std::uint32_t instanceIndex;
    std::uint32_t padding[3];
};

struct PhongLight {
    static constexpr std::int32_t kTypeDirectional = 0;
    static constexpr std::int32_t kTypePoint = 1;
    static constexpr std::int32_t kTypeSpot = 2;

    dx::XMFLOAT4 ambient;
    dx::XMFLOAT4 diffuse;
    dx::XMFLOAT4 specular;
    dx::XMFLOAT3 posV;
    float falloff;
    dx::XMFLOAT3 dirV;
    float cosTheta;
    dx::XMFLOAT3 atten;
    float cosPhi;
    std::int32_t type;
    dx::XMFLOAT3 padding;
};

}   // namespace gfx::d3d12::sr

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __ShaderRes_HPP