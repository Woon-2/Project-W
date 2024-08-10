#include "Vertex.hpp"

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

    auto offset = offsets_[toIdx(prop)];
    if (offset == invalidOffset) {
        throw;  // TODO: add exception
    }

    data_.resize(stride_ * cnt);

    for (std::size_t i = 0; i < cnt; ++i) {
        std::memcpy( data_.data() + i * stride_ + offset,
            static_cast<const std::uint8_t*>(data) + i * stride, elemByteWidth
        );
    }
}

void VertexBuffer::fetchProp( const VertexBuffer& other, Vertex::Properties prop,
    offset_t& accOffset
) {
    configProperty(prop, accOffset);

    auto pbw = other.propByteWidth(prop);
    auto otherOff = other.offsets_[other.toIdx(prop)];

    for (std::size_t i = 0; i < other.size(); ++i) {
        std::memcpy( data_.data() + i * stride_ + accOffset,
            other.data_.data() + i * other.stride_ + otherOff, pbw
        );
    }

    accOffset += pbw;
}

}   // namespace gfx