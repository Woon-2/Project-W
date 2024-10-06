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
#include <optional>

#include "enumUtil.hpp"

namespace gfx {

namespace d3d12 {

class MeshView {
public:
    MeshView(const Mesh* pMesh)
        : pMesh_(pMesh), pShader_(nullptr), protocol_(std::nullopt) {}

    MeshView(const Mesh* pMesh, const Shader* pShader, rp::Protocol protocol)
        : pMesh_(pMesh), pShader_(pShader), protocol_(protocol) {}

    const Shader* shader() const NOEXCEPT { return pShader_; }
    void setShader(const Shader* pShader) NOEXCEPT { pShader_ = pShader; }

    const Mesh* mesh() const NOEXCEPT { return pMesh_; }
    void setMesh(const Mesh* pMesh) NOEXCEPT { pMesh_ = pMesh; }

    bool hasProtocol() const NOEXCEPT { return protocol_.has_value(); }
    rp::Protocol protocol() const NOEXCEPT { return protocol_.value(); }
    void setProtocol(rp::Protocol protocol) NOEXCEPT { protocol_ = protocol; }

    auto operator<=>(const MeshView& other) const NOEXCEPT {
        auto first = etoi(protocol_.value_or(gfx::rp::Protocol::Null))
            <=> etoi(other.protocol_.value_or(gfx::rp::Protocol::Null));
        if (first != 0) return first;
        return pMesh_ <=> other.pMesh_;
    }

private:
    const Mesh* pMesh_;
    const Shader* pShader_;
    std::optional<rp::Protocol> protocol_;
};

struct Fragment {
    const MeshView meshView;
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