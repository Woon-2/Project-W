#include "mesh.hpp"

#include "gfxExcept.hpp"

namespace gfx {

bool Mesh::supports(const InputLayout& il) const NOEXCEPT {
    for (const auto& slot : il) {
        auto satisfiesSlot = std::ranges::find_if( vbs_, [&](const VertexBuffer& vb) {
            return vb.stride() == slot.stride && std::ranges::all_of(
                slot.elements, [&](const InputLayout::Element& elem) {
                    return vb.contains(elem.prop) && vb.offset(elem.prop) == elem.offset;
                }
            );
        } ) != vbs_.end();
        if (!satisfiesSlot) {
            return false;
        }
    }
    return true;
}

const std::vector<std::size_t> Mesh::vbIndices(const InputLayout& il) const {
    auto ret = std::vector<std::size_t>();
    ret.reserve(il.slotCnt());

    for (const auto& slot : il) {
        auto itVB = std::ranges::find_if( vbs_, [&](const VertexBuffer& vb) {
            return vb.stride() == slot.stride && std::ranges::all_of(
                slot.elements, [&](const InputLayout::Element& elem) {
                    return vb.contains(elem.prop) && vb.offset(elem.prop) == elem.offset;
                }
            );
        } );
        if (itVB == vbs_.end()) {
            throw GFX_EXCEPT("InputLayout not supported by mesh");
        }
        ret.push_back( static_cast<std::size_t>( std::distance(vbs_.begin(), itVB) ) );
    }

    return ret;
}

}   // namespace gfx