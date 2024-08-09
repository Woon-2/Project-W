#ifndef __VERTEX_HPP
#define __VERTEX_HPP

#include <vector>
#include <array>
#include <concepts>

#include <cstdint>
#include <cstdlib>

#include "enumUtil.hpp"

namespace gfx {

class VertexBuffer;

class Vertex {
public:
    static constexpr std::size_t numProperties = 6u;

    enum class Properties : std::uint32_t {
        Position = 0x1,
        Normal = 0x2,
        TexCoord = 0x4,
        Tangent = 0x8,
        Bitangent = 0x10,
        Color = 0x20
    };

    Vertex()
        : pStart_(nullptr), pBuf_(nullptr) {}
    Vertex(std::uint8_t* pStart, VertexBuffer* pBuf)
        : pStart_(pStart), pBuf_(pBuf) {}

    template <typename T>
    T& get(Properties prop);
    template <typename T>
    const T& get(Properties prop) const;
    template <typename T>
    void set(Properties prop, const T& val);
    template <typename T>
    void set(Properties prop, T&& val);
    void* operator[](Properties prop);

private:
    std::uint8_t* pStart_;
    VertexBuffer* pBuf_;
};

DEFINE_ENUM_LOGICAL_OP_ALL(Vertex::Properties)

class VertexBuffer {
public:
    friend class Vertex;

    static constexpr std::size_t invalidOffset = -1;
    static constexpr std::size_t invalidStride = -1;
    using offset_t = std::size_t;

    VertexBuffer()
        : offsets_{ invalidOffset, invalidOffset, invalidOffset, invalidOffset, invalidOffset, invalidOffset },
        data_(), properties_(0u), stride_(invalidStride) {}

    ~VertexBuffer() = default;
    VertexBuffer(const VertexBuffer&) = default;
    VertexBuffer(VertexBuffer&&) noexcept = default;
    VertexBuffer& operator=(const VertexBuffer&) = default;
    VertexBuffer& operator=(VertexBuffer&&) noexcept = default;

    template < std::same_as<Vertex::Properties>... Props >
    VertexBuffer(const VertexBuffer& other, Props... props)
        : offsets_{ invalidOffset, invalidOffset, invalidOffset, invalidOffset, invalidOffset, invalidOffset },
        data_(), properties_(0u), stride_(0u)
    {
        for (auto prop : { props... }) {
            stride_ += other.propByteWidth(prop);
        }

        data_.resize(stride_ * other.size());

        offset_t accOffset = 0;

        (fetchProp(other, props, accOffset), ...);
    }

    bool contains(Vertex::Properties prop) const NOEXCEPT {
        return properties_ & prop;
    }

    void configProperty(Vertex::Properties prop, offset_t offset) NOEXCEPT {
        offsets_[toIdx(prop)] = offset;
        properties_ = properties_ | prop;
    }

    void configStride(std::size_t stride) NOEXCEPT {
        stride_ = stride;
    }

    void constructProperty( Vertex::Properties prop, const std::uint8_t* data,
        std::size_t propByteWidth, std::size_t cnt, std::size_t stride
    );

    void constructRawMem(const std::uint8_t* data, std::size_t byteWidth) {
        data_.resize(byteWidth);
        std::memcpy(data_.data(), data, byteWidth);
    }

    void* rawMem() NOEXCEPT {
        return data_.data();
    }

    const void* rawMem() const NOEXCEPT {
        return data_.data();
    }

    std::size_t stride() const NOEXCEPT {
        return stride_;
    }

    std::size_t byteWidth() const NOEXCEPT {
        return data_.size();
    }

    std::size_t propByteWidth(Vertex::Properties prop) const {
        if (!contains(prop)) {
            throw;  // TODO: add exception
        }

        auto propIdx = toIdx(prop);
        if (propIdx == Vertex::numProperties - 1) {
            return stride_ - offsets_[propIdx];
        }
        return offsets_[propIdx + 1] - offsets_[propIdx];
    }

    std::size_t size() const {
        return data_.size() / stride_;
    }

    Vertex operator[](std::size_t idx) {
        return Vertex(data_.data() + idx * stride_, this);
    }

    const Vertex operator[](std::size_t idx) const {
        return Vertex( const_cast<std::uint8_t*>( data_.data() + idx * stride_ ),
            const_cast<VertexBuffer*>(this)
        );
    }

    Vertex get(std::size_t idx) {
        return (*this)[idx];
    }

    const Vertex get(std::size_t idx) const {
        return (*this)[idx];
    }

    offset_t offset(Vertex::Properties prop) const {
        return offsets_[toIdx(prop)];
    }

private:
    void fetchProp(const VertexBuffer& other, Vertex::Properties prop, offset_t& accOffset);

    static constexpr std::size_t toIdx(Vertex::Properties prop) {
        switch (prop) {
        case Vertex::Properties::Position: return 0u;
        case Vertex::Properties::Normal: return 1u;
        case Vertex::Properties::TexCoord: return 2u;
        case Vertex::Properties::Tangent: return 3u;
        case Vertex::Properties::Bitangent: return 4u;
        case Vertex::Properties::Color: return 5u;
        default: throw;  // TODO: add exception
        }
    }

    std::array<offset_t, Vertex::numProperties> offsets_;
    std::vector<std::uint8_t> data_;
    std::uint32_t properties_;
    std::size_t stride_;
};

template <typename T>
T& Vertex::get(Properties prop) {
    return *reinterpret_cast<T*>(pStart_ + pBuf_->offsets_[pBuf_->toIdx(prop)]);
}

template <typename T>
const T& Vertex::get(Properties prop) const {
    return *reinterpret_cast<const T*>(pStart_ + pBuf_->offsets_[pBuf_->toIdx(prop)]);
}

template <typename T>
void Vertex::set(Properties prop, const T& val) {
    std::memcpy(pStart_ + pBuf_->offsets_[pBuf_->toIdx(prop)], &val, sizeof(T));
}

template <typename T>
void Vertex::set(Properties prop, T&& val) {
    std::memcpy(pStart_ + pBuf_->offsets_[pBuf_->toIdx(prop)], &val, sizeof(T));
}

inline void* Vertex::operator[](Properties prop) {
    return pStart_ + pBuf_->offsets_[pBuf_->toIdx(prop)];
}

} // namespace gfx

#endif // __VERTEX_HPP