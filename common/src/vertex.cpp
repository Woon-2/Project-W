#include "Vertex.hpp"

namespace gfx {

void VertexBuffer::constructProperty( Vertex::Properties prop, const std::uint8_t* data,
    std::size_t elemByteWidth, std::size_t cnt
) {
    if (!contains(prop)) {
        throw;  // TODO: add exception
    }

    if (stride_ == invalidStride) {
        throw;  // TODO: add exception
    }

    auto offset = offsets_[toIdx(prop)];
    if (offset == invalidOffset) {
        throw;  // TODO: add exception
    }

    data_.resize(stride_ * cnt);

    for (std::size_t i = 0; i < cnt; ++i) {
        std::memcpy(data_.data() + i * stride_ + offset, data + i * elemByteWidth, elemByteWidth);
    }
}

}   // namespace gfx