#ifndef __DrawInfo_HPP
#define __DrawInfo_HPP

#include "d3d12mesh.hpp"
#include "d3d12material.hpp"

#define DXMATH_VEC_UTIL
#define DXMATH_MAT_UTIL
#define DXMATH_QUAT_UTIL
#include "mathUtil.hpp"

#include <span>
#include <cstdint>

namespace gfx {

namespace d3d12 {

struct Fragment {
    const Mesh* pMesh;
    const Material* pMaterial;
    std::span<mu::Mat4x4> worlds;
};

struct UniDrawinfo {
    enum class Type {
        Fragment,
        Light
    };
    Type type;
    void* pData;
    std::size_t size;
};

}   // namespace gfx::d3d12

} // namespace gfx

#endif // __DrawInfo_HPP