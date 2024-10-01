#include "Vertex.hpp"

#include <ranges>
#include <algorithm>

namespace gfx {

void VertexBuffer::constructProperty( Vertex::Properties prop, const void* data,
    std::size_t elemByteWidth, std::size_t cnt, std::size_t stride
) {
    if (!contains(prop)) {
        throw;  // TODO: add exception
    }

    if (stride_ == invalidStride) {
        throw;  // TODO: add exception
    }

    auto offset = offsets_[etoi(prop)];
    if (offset == invalidOffset) {
        throw;  // TODO: add exception
    }

    data_.resize(stride_ * cnt);

    for (std::size_t i = 0; i < cnt; ++i) {
        std::ranges::copy_n( static_cast<const std::uint8_t*>(data) + i * stride,
            elemByteWidth, data_.begin() + i * stride_ + offset
        );
    }
}

void VertexBuffer::fetchProp( const VertexBuffer& other, Vertex::Properties prop,
    offset_t& accOffset
) {
    configProperty(prop, accOffset);

    const auto pbw = Vertex::propByteWidth(prop);
    const auto otherOff = other.offsets_[etoi(prop)];

    for (std::size_t i = 0; i < other.size(); ++i) {
        std::ranges::copy_n( other.data_.data() + i * other.stride_ + otherOff,
            pbw, data_.begin() + i * stride_ + accOffset
        );
    }

    accOffset += pbw;
}

}   // namespace gfx