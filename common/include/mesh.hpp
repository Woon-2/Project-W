#ifndef __MESH_HPP
#define __MESH_HPP

#include "vertex.hpp"
#include "inputLayout.hpp"

#include <bitset>
#include <ranges>
#include <algorithm>
#include <type_traits>
#include <vector>
#include <cstdint>

#include <filesystem>

#include "assimp/scene.h"

#include "config.hpp"

namespace gfx {

class Mesh {
public:
    using Index = std::uint32_t;
    template <class T>
    using Cont = std::vector<T>;

    using VBFlags = std::vector<VertexBuffer::VBFlag>;

    Mesh() = default;

    template < std::ranges::range VBS, std::ranges::range IB,
        class VBIt = std::conditional_t<
            std::is_lvalue_reference_v< std::remove_const_t<VBS> >,
            typename VBS::const_iterator,
            std::move_iterator<typename VBS::iterator> 
        >
    > requires std::same_as<std::ranges::range_value_t<VBS>, VertexBuffer>
            && std::same_as<std::ranges::range_value_t<IB>, Index>
    Mesh(VBS&& vbs, IB&& ib)
        : vbs_( VBIt(std::begin(vbs)), VBIt(std::end(vbs)) ),
        ib_(std::begin(ib), std::end(ib)) {}

    template < std::ranges::range VBS,
        class VBIt = std::conditional_t<
            std::is_lvalue_reference_v< std::remove_const_t<VBS> >,
            typename VBS::const_iterator,
            std::move_iterator<typename VBS::iterator> 
        >
    > requires std::same_as<std::ranges::range_value_t<VBS>, VertexBuffer>
    Mesh(VBS&& vbs, Cont<Index>&& ib)
        : vbs_( VBIt(std::begin(vbs)), VBIt(std::end(vbs)) ),
        ib_(std::move(ib)) {}

    template <std::ranges::range IB>
        requires std::same_as<std::ranges::range_value_t<IB>, Index>
    Mesh(Cont<VertexBuffer>&& vbs, IB&& ib)
        : vbs_( std::move(vbs) ),
        ib_(std::begin(ib), std::end(ib)) {}

    Mesh(Cont<VertexBuffer>&& vbs, Cont<Index>&& ib)
        : vbs_( std::move(vbs) ),
        ib_( std::move(ib) ) {}

    bool supports(const InputLayout& il) const NOEXCEPT;
    Cont<VertexBuffer>& vbs() NOEXCEPT {
        return vbs_;
    }
    const Cont<VertexBuffer>& vbs() const NOEXCEPT {
        return vbs_;
    }

    const std::vector<std::size_t> vbIndices(const InputLayout& il) const;

    Cont<Index>& ib() NOEXCEPT {
        return ib_;
    }
    const Cont<Index>& ib() const NOEXCEPT {
        return ib_;
    }

    void pushVb(const VertexBuffer& vb) {
        vbs_.push_back(vb);
    }

    void pushVb(VertexBuffer&& vb) {
        vbs_.push_back(std::move(vb));
    }

    void setIb(const Cont<Index>& ib) {
        ib_ = ib;
    }

    void setIb(Cont<Index>&& ib) {
        ib_ = std::move(ib);
    }

    void clear() {
        vbs_.clear();
        ib_.clear();
    }

    const VBFlags& flags() const NOEXCEPT {
        return flags_;
    }

private:
    Cont<VertexBuffer> vbs_;
    Cont<Index> ib_;
    VBFlags flags_;
};

}   // namespace gfx

#endif // __MESH_HPP