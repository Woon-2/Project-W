#ifndef __INPUT_LAYOUT_HPP
#define __INPUT_LAYOUT_HPP

#include "vertex.hpp"

#include <concepts>
#include <vector>

namespace gfx {

class InputLayout {
public:
    struct Element {
        Vertex::Properties prop;
        VertexBuffer::offset_t offset;
    };

    template < std::same_as<Element>... Elems >
    InputLayout(std::size_t stride, Elems... elems)
        : elements_(), stride_(stride) {
        // For offset-based sorting, we do not pass elements directly into the vector.
        (configProperty(elems.prop, elems.offset), ...);
    }

    void configProperty(Vertex::Properties prop, VertexBuffer::offset_t offset);
    void configStride(std::size_t stride) NOEXCEPT {
        stride_ = stride;
    }

    friend VertexBuffer convert(const VertexBuffer& vb, const InputLayout& il);

private:
    std::vector<Element> elements_;
    std::size_t stride_;
};

}   // namespace gfx

#endif // __INPUT_LAYOUT_HPP