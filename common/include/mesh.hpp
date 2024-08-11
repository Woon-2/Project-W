#ifndef __MESH_HPP
#define __MESH_HPP

#include "vertex.hpp"
#include "inputLayout.hpp"

#include <vector>
#include <cstdint>
#include <ranges>
#include <algorithm>
#include <filesystem>

#include "assimp/scene.h"

namespace gfx {

class Mesh {
public:
    using IndexCont = std::vector<std::uint32_t>;

    template <std::ranges::range R>
    Mesh(VertexBuffer vb, R&& ib)
        : vb_(std::move(vb)), ib_(std::begin(ib), std::end(ib)) {}

    Mesh(VertexBuffer vb, IndexCont&& ib)
        : vb_(std::move(vb)), ib_(std::move(ib)) {}

    bool contains(Vertex::Properties prop) const NOEXCEPT {
        return vb_.contains(prop);
    }

    VertexBuffer& vb() NOEXCEPT {
        return vb_;
    }

    const VertexBuffer& vb() const NOEXCEPT {
        return vb_;
    }

    const IndexCont& ib() const NOEXCEPT {
        return ib_;
    }

    IndexCont& ib() NOEXCEPT {
        return ib_;
    }

private:
    VertexBuffer vb_;
    IndexCont ib_;
};

namespace detail {
Mesh getMeshFromAiNode(const aiNode* node, const aiScene* scene, unsigned int meshIdx);
}   // namespace gfx::detail

Mesh loadMesh(const std::filesystem::path& path);
Mesh loadMesh(const std::filesystem::path& path, const InputLayout& il);

}   // namespace gfx

#endif // __MESH_HPP