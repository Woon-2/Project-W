#ifndef __INPUT_LAYOUT_HPP
#define __INPUT_LAYOUT_HPP

#include "vertex.hpp"

#include <concepts>
#include <vector>
#include <bitset>
#include <ranges>
#include <algorithm>
#include <cassert>

#include "config.hpp"

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

    struct Slot {
        std::vector<Element> elements;
        std::size_t stride = 0;
    };

    using SlotIdx = std::size_t;
    
    struct ElemDesc {
        Element elem;
        SlotIdx slotIdx;
    };

    InputLayout()
        : slots_() {}

    template < std::same_as<Slot>... Slots >
    InputLayout(Slots&& ... slots)
        : slots_(std::forward<Slot>(slots)...) {}

    void configSlots(std::size_t slotCnt) {
        slots_.resize(slotCnt);
    }

    void configProperty(Vertex::Properties prop, SlotIdx idx);

    bool has(Vertex::Properties prop) const NOEXCEPT {
        return slotIdx(prop) != flags_.size();
    }

    bool has(SlotIdx idx, Vertex::Properties prop) const NOEXCEPT {
        return flags_[idx].test(etoi(prop));
    }

    SlotIdx slotIdx(Vertex::Properties prop) const NOEXCEPT {
        return std::ranges::distance( flags_.begin(),
            std::ranges::find_if( flags_, [prop](const auto& flag) {
                return flag.test(etoi(prop));
            } )
        );
    }

    Slot& slot(SlotIdx idx) {
        assert(idx < slots_.size());
        return slots_[idx];
    }

    const Slot& slot(SlotIdx idx) const {
        assert(idx < slots_.size());
        return slots_[idx];
    }

    std::size_t slotCnt() const NOEXCEPT {
        return slots_.size();
    }

    std::size_t stride(SlotIdx idx) const NOEXCEPT {
        return slots_[idx].stride;
    }

    const ElemDesc elemDesc(Vertex::Properties prop) const {
        auto idx = slotIdx(prop);
        assert(idx != flags_.size());
        return ElemDesc {
            // if slotIdx succeeded, then the element must exist in the slot.
            .elem = *std::ranges::find_if( slot(idx).elements, [prop](const auto& elem) {
                return elem.prop == prop;
            } ),
            .slotIdx = idx
        };
    }

    auto begin() NOEXCEPT {
        return slots_.begin();
    }

    auto end() NOEXCEPT {
        return slots_.end();
    }

    auto begin() const NOEXCEPT {
        return slots_.begin();
    }

    auto end() const NOEXCEPT {
        return slots_.end();
    }

    auto cbegin() const NOEXCEPT {
        return slots_.cbegin();
    }

    auto cend() const NOEXCEPT {
        return slots_.cend();
    }

private:
    std::vector<Slot> slots_;
    std::vector<std::bitset<etoi(Vertex::Properties::SIZE)>> flags_;
};

}   // namespace gfx

#endif // __INPUT_LAYOUT_HPP