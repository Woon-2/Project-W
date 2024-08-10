#include "inputLayout.hpp"

#include <ranges>
#include <algorithm>

namespace gfx {

void InputLayout::configProperty(Vertex::Properties prop, VertexBuffer::offset_t offset) {
    auto hole = std::ranges::upper_bound(elements_, offset, std::less<>{}, &Element::offset);
    elements_.insert(hole, Element{prop, offset});
}


VertexBuffer convert(const VertexBuffer& vb, const InputLayout& il) {
    auto ret = VertexBuffer();
    ret.configStride(il.stride_);

    for (const auto& elem : il.elements_) {
        ret.configProperty(elem.prop, elem.offset);
    }

    for (const auto& elem : il.elements_) {
        ret.constructProperty( elem.prop, static_cast<const std::uint8_t*>( vb.rawMem() )
            + vb.offset(elem.prop), vb.propByteWidth(elem.prop), vb.size(), vb.stride()
        );
    }

    return ret;
}

}   // namespace gfx