#include "inputLayout.hpp"

#include <ranges>
#include <algorithm>

namespace gfx {

void InputLayout::configProperty(Vertex::Properties prop, SlotIdx slotIdx) {
    auto& slot = slots_[slotIdx];
    auto& elements_ = slot.elements;
    elements_.emplace_back(prop, slot.stride);
    slot.stride += Vertex::propByteWidth(prop);
    flags_[slotIdx].set(etoi(prop));
}

}   // namespace gfx