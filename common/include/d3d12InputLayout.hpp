#ifndef __D3D12INPUTLAYOUT_HPP
#define __D3D12INPUTLAYOUT_HPP

#include "inputLayout.hpp"

#include <d3d12.h>

#include <vector>
#include <string>
#include <concepts>
#include <cstdint>
#include <map>

#include "config.hpp"

namespace gfx {

namespace d3d12 {

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
private:
    struct ElementAux {
        std::string semanticName;
        std::uint32_t semanticIndex;
        DXGI_FORMAT format;
        std::uint32_t inputSlot;
    };

public:
    using gfx::InputLayout::Element;

    /**
     * @brief Default constructor.    
     * Creates an empty input layout with no elements and an invalid stride.
     */
    InputLayout()
        : gfx::InputLayout(), elemDescs_() {}

    /**
     * @brief Constructs an input layout with a stride and elements.
     * @tparam Elems Elements.
     * @param stride The specification of stride between vertices.
     * @param elems The elements of the input layout.
     * @details The elements are sorted by their offsets.
     * @note To create D3D12 input element descriptors,     
     * it is required to configure auxiliary information with InputLayout::configPropertyAux before creating InputLayout instances.
     * @see Element InputLayout::configProperty
     */
    template < std::same_as<Element>... Elems >
    InputLayout(std::size_t stride, Elems... elems)
        : gfx::InputLayout(stride, elems...), elemDescs_() {
        elemDescs_.reserve(sizeof...(elems));
        dispatchElems(elems...);
    }
    /**
     * @brief Constructs an input layout with a gfx::InputLayout.      
     * It is constructed as if the auxiliary information is complemented to the original input elements.
     * @param il The input layout to copy.
     * @details The input layout is copied with the same elements and stride.
     * @note To create D3D12 input element descriptors,
     * it is required to configure auxiliary information with InputLayout::configPropertyAux before creating InputLayout instances. 
     */
    InputLayout(const gfx::InputLayout& il);

    /**
     * @brief Configures a property with an offset, creating an element.
     * @param prop The property to configure.
     * @param offset The offset of the property.
     * @note To create D3D12 input element descriptors,    
     * it is required to configure auxiliary information with InputLayout::configPropertyAux before creating InputLayout instances.
     * @see configPropertyAux Element
     */
    void configProperty(Vertex::Properties prop, VertexBuffer::offset_t offset) {
        gfx::InputLayout::configProperty(prop, offset);
        dispatchElem(Element{prop, offset});
    }
    /**
     * @brief Maps auxiliary information that is needed for D3D12 shader input layout to a specific property statically.
     * @param prop The property to map.
     * @param semanticName The semantic name of the property.
     * @param semanticIndex The semantic index of the property.
     * @param format The format of the property.
     * @param inputSlot The input slot of the property.
     * @note This function should be called before creating any InputLayout instances or calling InputLayout::configProperty.
     * @see configProperty
     */
    static void configPropertyAux( Vertex::Properties prop, std::string semanticName,
        std::uint32_t semanticIndex, DXGI_FORMAT format, std::uint32_t inputSlot
    );

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