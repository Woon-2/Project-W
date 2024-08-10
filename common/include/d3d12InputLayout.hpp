#ifndef __D3D12INPUTLAYOUT_HPP
#define __D3D12INPUTLAYOUT_HPP

#include "inputLayout.hpp"

#include <d3d12.h>

#include <vector>
#include <string>
#include <concepts>
#include <cstdint>
#include <map>

namespace gfx {

namespace d3d12 {

class InputLayout : private gfx::InputLayout {
private:
    struct ElementAux {
        std::string semanticName;
        std::uint32_t semanticIndex;
        DXGI_FORMAT format;
        std::uint32_t inputSlot;
    };

public:
    using gfx::InputLayout::Element;

    InputLayout()
        : gfx::InputLayout(), elemDescs_() {}

    template < std::same_as<Element>... Elems >
    InputLayout(std::size_t stride, Elems... elems)
        : gfx::InputLayout(stride, elems...), elemDescs_() {
        elemDescs_.reserve(sizeof...(elems));
        dispatchElems(elems...);
    }

    InputLayout(const gfx::InputLayout& il);

    static void configPropertyAux( Vertex::Properties prop, std::string semanticName,
        std::uint32_t semanticIndex, DXGI_FORMAT format, std::uint32_t inputSlot
    );

    const D3D12_INPUT_LAYOUT_DESC make() const NOEXCEPT {
        return D3D12_INPUT_LAYOUT_DESC{
            .pInputElementDescs = elemDescs_.data(),
            .NumElements = static_cast<std::uint32_t>( elemDescs_.size() )
        };
    }

    operator const D3D12_INPUT_LAYOUT_DESC() const NOEXCEPT {
        return make();
    }

private:
    void dispatchElems() {}

    template < std::same_as<Element>... Elems >
    void dispatchElems(Element elem, Elems... rest) {
        dispatchElem(elem);
        dispatchElems(rest...);
    }

    void dispatchElem(Element elem);

    static std::size_t formatWidth(DXGI_FORMAT format) NOEXCEPT;

    static std::multimap<Vertex::Properties, ElementAux> auxMap_;
    std::vector<D3D12_INPUT_ELEMENT_DESC> elemDescs_;
};

} // namespace d3d12

} // namespace gfx

#endif // __D3D12INPUTLAYOUT_HPP