#include "d3d12InputLayout.hpp"

namespace gfx {

namespace d3d12 {

InputLayout::InputLayout(const gfx::InputLayout& il)
    : gfx::InputLayout(il), elemDescs_() {
    elemDescs_.reserve(il.elemCnt());
    for (const auto& elem : il) {
        dispatchElem(elem);
    }
}

void InputLayout::configPropertyAux( Vertex::Properties prop, std::string semanticName,
    std::uint32_t semanticIndex, DXGI_FORMAT format, std::uint32_t inputSlot
) {
    auxMap_.emplace( prop, ElementAux{semanticName, semanticIndex, format, inputSlot} );
}

void InputLayout::dispatchElem(Element elem) {
    auto [first, last] = auxMap_.equal_range(elem.prop);
    auto accOffset = elem.offset;

    for (auto it = first; it != last; ++it) {
        const auto& aux = it->second;

        elemDescs_.push_back( D3D12_INPUT_ELEMENT_DESC{
            .SemanticName = aux.semanticName.c_str(),
            .SemanticIndex = aux.semanticIndex,
            .Format = aux.format,
            .InputSlot = aux.inputSlot,
            .AlignedByteOffset = static_cast<std::uint32_t>( accOffset ),
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0
        } );

        accOffset += formatWidth(aux.format);
    }
}

std::size_t InputLayout::formatWidth(DXGI_FORMAT format) NOEXCEPT {
    switch (format) {
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return 16u;
    case DXGI_FORMAT_R32G32B32_FLOAT:
        return 12u;
    case DXGI_FORMAT_R32G32_FLOAT:
        return 8u;
    case DXGI_FORMAT_R32_FLOAT:
        return 4u;
    default:
        return 0;
    }
}

std::multimap<Vertex::Properties, InputLayout::ElementAux> InputLayout::auxMap_;

}   // namespace d3d12

}   // namespace gfx