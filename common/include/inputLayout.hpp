#ifndef __INPUT_LAYOUT_HPP
#define __INPUT_LAYOUT_HPP

#include "vertex.hpp"

#include <concepts>
#include <vector>

namespace gfx {

/**
 * @brief A class representing an input layout independent of rendering API.    
 * Input layout is a collection of vertex properties and their offsets.    
 * It can be used to convert a vertex buffer to another vertex buffer with a different input layout.
 * @see Vertex VertexBuffer loadMesh loadModel
 */
class InputLayout {
public:
    /**
     * @brief A struct representing an element of an input layout.
     * It consists of a vertex property and an offset.
     * @see Vertex::Properties
     */
    struct Element {
        Vertex::Properties prop;
        VertexBuffer::offset_t offset;
    };

    /**
     * @brief Default constructor.    
     * Creates an empty input layout with no elements and an invalid stride.
     */
    InputLayout()
        : elements_(), stride_(VertexBuffer::invalidStride) {}

    /**
     * @brief Constructs an input layout with a stride and elements.
     * @tparam Elems Elements.
     * @param stride The specification of stride between vertices.
     * @param elems The elements of the input layout.
     * @details The elements are sorted by their offsets.
     * @see Element InputLayout::configProperty
     */
    template < std::same_as<Element>... Elems >
    InputLayout(std::size_t stride, Elems... elems)
        : elements_(), stride_(stride) {
        // For offset-based sorting, we do not pass elements directly into the vector.
        (configProperty(elems.prop, elems.offset), ...);
    }

    /**
     * @brief Configures a property with an offset, creating an element.
     * @param prop The property to configure.
     * @param offset The offset of the property.
     * @see Element
     */
    void configProperty(Vertex::Properties prop, VertexBuffer::offset_t offset);
    /**
     * @brief Configures the specification of stride between vertices.
     * @param stride The stride to configure.
     * @see stride
     */
    void configStride(std::size_t stride) NOEXCEPT {
        stride_ = stride;
    }
    /**
     * @brief Gets the stride value
     * @return `std::size_t` The stride.
     * @see configStride
     */
    std::size_t stride() const NOEXCEPT {
        return stride_;
    }

    /**
     * @brief Gets the count of elements.
     * @return `std::size_t` The count of elements.
     */
    std::size_t elemCnt() const NOEXCEPT {
        return elements_.size();
    }
    /**
     * @brief Gets the begin iterator of the container of elements.
     * @details As Inputlayout exposes begin and end,    
     * it is iterable and therefore can be used in range-based algorithms.
     * @return `std::vector<Element>::const_iterator` The begin iterator.
     */
    auto begin() const NOEXCEPT {
        return elements_.begin();
    }
    /**
     * @brief Gets the begin iterator of the container of elements.
     * @return `std::vector<Element>::iterator` The begin iterator.
     */
    auto begin() NOEXCEPT {
        return elements_.begin();
    }
    /**
     * @brief Gets the end iterator of the container of elements.
     * @return `std::vector<Element>::const_iterator` The end iterator.
     */
    auto end() const NOEXCEPT {
        return elements_.end();
    }
    /**
     * @brief Gets the end iterator of the container of elements.
     * @return `std::vector<Element>::iterator` The end iterator.
     */
    auto end() NOEXCEPT {
        return elements_.end();
    }

    friend VertexBuffer convert(const VertexBuffer& vb, const InputLayout& il);

private:
    std::vector<Element> elements_;
    std::size_t stride_;
};

}   // namespace gfx

#endif // __INPUT_LAYOUT_HPP