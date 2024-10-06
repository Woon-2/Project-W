#include "d3d12InputLayout.hpp"

namespace gfx {

namespace d3d12 {

namespace detail {

void InputElementAuxMap::init() {
    auxMap_.try_emplace(Vertex::Properties::Position3D, InputElementAux{
        .semanticName = "POSITION",
        .semanticIndex = 0,
        .format = DXGI_FORMAT_R32G32B32_FLOAT
    } );
    auxMap_.try_emplace(Vertex::Properties::Normal3D, InputElementAux{
        .semanticName = "NORMAL",
        .semanticIndex = 0,
        .format = DXGI_FORMAT_R32G32B32_FLOAT
    } );
    auxMap_.try_emplace(Vertex::Properties::Color3D, InputElementAux{
        .semanticName = "COLOR",
        .semanticIndex = 0,
        .format = DXGI_FORMAT_R32G32B32_FLOAT
    } );
    auxMap_.try_emplace(Vertex::Properties::Color4D, InputElementAux{
        .semanticName = "COLOR",
        .semanticIndex = 0,
        .format = DXGI_FORMAT_R32G32B32A32_FLOAT
    } );
    auxMap_.try_emplace(Vertex::Properties::TexCoord2D0, InputElementAux{
        .semanticName = "TEXCOORD",
        .semanticIndex = 0,
        .format = DXGI_FORMAT_R32G32_FLOAT
    } );
    auxMap_.try_emplace(Vertex::Properties::Tangent3D, InputElementAux{
        .semanticName = "TANGENT",
        .semanticIndex = 0,
        .format = DXGI_FORMAT_R32G32B32_FLOAT
    } );
    auxMap_.try_emplace(Vertex::Properties::Bitangent3D, InputElementAux{
        .semanticName = "BITANGENT",
        .semanticIndex = 0,
        .format = DXGI_FORMAT_R32G32B32_FLOAT
    } );
}

} // namespace gfx::d3d12::detail

InputLayout::InputLayout(const gfx::InputLayout& il)
    : gfx::InputLayout(il), elemDescs_() {
    elemDescs_.reserve(il.elemCnt());
    for (auto itSlot = il.begin(); itSlot != il.end(); ++itSlot) {
        for (const auto& elem : itSlot->elements) {
            dispatchElem( ElemDesc{
                .elem = elem,
                .slotIdx = static_cast<SlotIdx>( std::distance(il.begin(), itSlot) )
            } );
        }
    }
}

void InputLayout::dispatchElem(const ElemDesc& elemDesc) {
    const auto& aux = auxMap_.aux(elemDesc.elem.prop);

    elemDescs_.emplace_back( 
        /* .SemanticName = */ aux.semanticName.c_str(),
        /* .SemanticIndex = */ aux.semanticIndex,
        /* .Format = */ aux.format,
        /* .InputSlot = */ static_cast<UINT>( elemDesc.slotIdx ),
        /* .AlignedByteOffset = */ static_cast<UINT>( elemDesc.elem.offset ),
        /* .InputSlotClass = */ D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        /* .InstanceDataStepRate = */ 0
    );
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

detail::InputElementAuxMap InputLayout::auxMap_;

}   // namespace d3d12

}   // namespace gfx