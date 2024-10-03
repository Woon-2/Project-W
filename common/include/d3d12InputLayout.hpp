#ifndef __D3D12INPUTLAYOUT_HPP
#define __D3D12INPUTLAYOUT_HPP

#include "inputLayout.hpp"

#include <directx/d3dx12.h>
#include <directx/d3d12.h>

#include <vector>
#include <string>
#include <concepts>
#include <cstdint>
#include <map>

#include "config.hpp"

namespace gfx {

namespace d3d12 {

namespace detail {

struct InputElementAux {
    std::string semanticName;
    std::uint32_t semanticIndex;
    DXGI_FORMAT format;
};


class InputElementAuxMap {
public:
    InputElementAuxMap()
        : auxMap_() {
        init();
    }

    const InputElementAux& aux(Vertex::Properties prop) const {
        return auxMap_.at(prop);
    }

private:
    void init();

    std::map<Vertex::Properties, InputElementAux> auxMap_;
};

} // namespace gfx::d3d12::detail

/**
 * @brief A class representing an input layout in D3D12.    
 * Like gfx::InputLayout, it's a collection of vertex properties and their offsets.    
 * Additionally, it contains auxiliary information for D3D12 input element descriptors.
 * @details The InputLayout's configuration goes with two parts:    
 * - The configuration of properties and offsets, which is the same as gfx::InputLayout.    
 * - The configuration of auxiliary information for D3D12 input element descriptors.     
 *   The information includes semantic name, semantic index, format, and input slot.  
 * The former configuration is done with InputLayout::configProperty,    
 * while the latter is done with InputLayout::configPropertyAux.    
 *    
 * Auxiliary information is mapped with a specific Vertex::Properties statically,    
 * and shared among all InputLayout instances.     
 * When InputLayout::configProperty is called, it queries the auxiliary information using the property as a key in the map,    
 * and creates a D3D12 input element descriptor with the information all together.
 * The created input element descriptor can be acquired by calling InputLayout::make or casting to D3D12_INPUT_LAYOUT_DESC.
 * 
 * @note So, the configuration of auxiliary information should be done     
 * before creating any InputLayout instances or calling InputLayout::configProperty.
 * @see gfx::InputLayout Vertex VertexBuffer Shader ShaderBuilder
 */
class InputLayout : public gfx::InputLayout {
public:
    using gfx::InputLayout::Element;

    InputLayout()
        : gfx::InputLayout(), elemDescs_() {}

    template < std::same_as<Slot>... Slots >
    InputLayout(Slots&&... slots)
        : gfx::InputLayout( std::forward<Slots>(slots)... ), elemDescs_() {
        // elemDescs_.reserve(sizeof...(elems));
        // dispatchElems(elems...);
    }

    InputLayout(const gfx::InputLayout& il);

    void configProperty(Vertex::Properties prop, SlotIdx idx) {
        gfx::InputLayout::configProperty(prop, idx);
        dispatchElem( ElemDesc{ .elem = slot(idx).elements.back(), .slotIdx = idx } );
    }

    /**
     * @brief Creates a D3D12 input layout descriptor with configured elements.    
     * The descriptor is used for creating a Shader.
     * @return `const D3D12_INPUT_LAYOUT_DESC` The input layout descriptor.
     * @see Shader ShaderBuilder
     */
    const D3D12_INPUT_LAYOUT_DESC make() const NOEXCEPT {
        return D3D12_INPUT_LAYOUT_DESC{
            .pInputElementDescs = elemDescs_.data(),
            .NumElements = static_cast<std::uint32_t>( elemDescs_.size() )
        };
    }
    /**
     * @brief Casts to D3D12 input layout descriptor.
     * @return `const D3D12_INPUT_LAYOUT_DESC` The input layout descriptor.
     * @see make Shader ShaderBuilder
     */
    operator const D3D12_INPUT_LAYOUT_DESC() const NOEXCEPT {
        return make();
    }

private:
    void dispatchElem(const ElemDesc& elem);

    static std::size_t formatWidth(DXGI_FORMAT format) NOEXCEPT;

    static detail::InputElementAuxMap auxMap_;
    std::vector<D3D12_INPUT_ELEMENT_DESC> elemDescs_;
};

} // namespace d3d12

} // namespace gfx

#endif // __D3D12INPUTLAYOUT_HPP