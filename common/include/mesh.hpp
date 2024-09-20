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

#include "config.hpp"

namespace gfx {

/**
 * @brief A class representing a mesh independent of rendering API.        
 * Mesh consists of a vertex buffer and an index buffer.
 * @details Vertex buffer is a VertexBuffer object,    
 * and index buffer is a C++ standard contiguous container of 32-bit unsigned integers.
 * @see VertexBuffer loadMesh
 */
class Mesh {
public:
    using IndexCont = std::vector<std::uint32_t>;

    /**
     * @brief Default constructor.    
     * Creates an empty mesh with no vertex buffer and no index buffer.
     */
    Mesh() = default;

    /**
     * @brief Constructs a mesh with a vertex buffer and an index buffer.
     * @tparam R A range of 32-bit unsigned integers.
     * @param vb Vertex buffer.
     * @param ib Index buffer.
     * @details The index buffer is copied from the range.
     * @see VertexBuffer
     */
    template <std::ranges::range R>
    Mesh(VertexBuffer vb, R&& ib)
        : vb_(std::move(vb)), ib_(std::begin(ib), std::end(ib)) {}
    /**
     * @brief Constructs a mesh with a vertex buffer and an index buffer.
     * @param vb Vertex buffer.
     * @param ib Index buffer.
     * @details The index buffer is moved from the container.
     * @see VertexBuffer
     */
    Mesh(VertexBuffer vb, IndexCont&& ib)
        : vb_(std::move(vb)), ib_(std::move(ib)) {}

    bool contains(Vertex::Properties prop) const NOEXCEPT {
        return vb_.contains(prop);
    }

    /**
     * @brief Accesses the vertex buffer.
     * @return VertexBuffer& A reference to the vertex buffer.
     * @see VertexBuffer
     */
    VertexBuffer& vb() NOEXCEPT {
        return vb_;
    }
    /**
     * @brief Accesses the vertex buffer.
     * @return const VertexBuffer& A const reference to the vertex buffer.
     * @see VertexBuffer
     */
    const VertexBuffer& vb() const NOEXCEPT {
        return vb_;
    }
    /**
     * @brief Accesses the index buffer.
     * @return IndexCont& A reference to the index buffer.
     */
    const IndexCont& ib() const NOEXCEPT {
        return ib_;
    }
    /**
     * @brief Accesses the index buffer.
     * @return IndexCont& A reference to the index buffer.
     */
    IndexCont& ib() NOEXCEPT {
        return ib_;
    }

private:
    VertexBuffer vb_;
    IndexCont ib_;
};

}   // namespace gfx

#endif // __MESH_HPP