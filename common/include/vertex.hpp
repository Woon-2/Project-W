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

/**
 * @brief A view for a vertex in a vertex buffer.    
 * It provides a way to access the properties of a vertex.    
 * It can be only created by VertexBuffer's Vertex accessors.
 * @see VertexBuffer
 */
class Vertex {
public:
    friend class VertexBuffer;
    static constexpr std::size_t numProperties = 6u;

    /**
     * @brief Properties of a vertex.
     * @details Each property is a flag that represents a property of a vertex.    
     * The flags can be combined with logical operators.     
     * The combined flags can be used to specify what properties a vertex contains.
     * @see VertexBuffer
     */
    enum class Properties : std::uint32_t {
        Position = 0x1,
        Normal = 0x2,
        TexCoord = 0x4,
        Tangent = 0x8,
        Bitangent = 0x10,
        Color = 0x20
    };

    /**
     * @brief Gets the property of the vertex.
     * @tparam T Type of the property value, the caller has to specify the type to interpret the property value.
     * @param prop Property to get.
     * @return `T&` Reference to the property value.
     * @see Vertex::set Vertex::operator[]
     */
    template <typename T>
    T& get(Properties prop);
    template <typename T>
    /**
     * @brief Gets the property of the vertex.
     * @tparam T Type of the property value, the caller has to specify the type to interpret the property value.
     * @param prop Property to get.
     * @return `const T&` Reference to the property value.
     * @see Vertex::set Vertex::operator[]
     */
    const T& get(Properties prop) const;
    /**
     * @brief Sets the property of the vertex.
     * @tparam T Type of the property value, as it is deduced from the argument, the caller doesn't have to specify the type.
     * @param prop Property to set.
     * @param val Value to set.
     * @see Vertex::get Vertex::operator[]
     */
    template <typename T>
    void set(Properties prop, const T& val);
    /**
     * @brief Sets the property of the vertex.
     * @tparam T Type of the property value, as it is deduced from the argument, the caller doesn't have to specify the type.
     * @param prop Property to set.
     * @param val Value to set.
     * @see Vertex::get Vertex::operator[]
     */
    template <typename T>
    void set(Properties prop, T&& val);
    /**
     * @brief Accesses the property of the vertex.
     * @param prop Property to get.
     * @return `void*` Pointer to the property value.
     * @see Vertex::get Vertex::set
     */
    void* operator[](Properties prop);

private:
    Vertex()
        : pStart_(nullptr), pBuf_(nullptr) {}
    Vertex(std::uint8_t* pStart, VertexBuffer* pBuf)
        : pStart_(pStart), pBuf_(pBuf) {}


    std::uint8_t* pStart_;
    VertexBuffer* pBuf_;
};

DEFINE_ENUM_LOGICAL_OP_ALL(Vertex::Properties)

/**
 * @brief A buffer that contains vertices laid out in contiguous memory.    
 * It provides a way to configure the properties of the vertices and access the vertices.     
 * Configuration of the properties has to be done before the vertices are filled,     
 * as the configuration determines the memory layout of the vertices.
 * @details To create an API-specific vertex buffer, get raw memory pointer by calling VertexBuffer::rawMem,     
 * take the byte width of the buffer by calling VertexBuffer::byteWidth,    
 * and take the stride of the buffer by calling VertexBuffer::stride,    
 * then they becomes the source of the API-specific vertex buffer.
 * @see Vertex Vertex::Properties Mesh
 */
class VertexBuffer {
public:
    friend class Vertex;

    static constexpr std::size_t invalidOffset = -1;
    static constexpr std::size_t invalidStride = -1;
    using offset_t = std::size_t;

    /**
     * @brief Constructs an empty vertex buffer.
     * @details The memory layout of the vertices is in invalid state.     
     * The properties of the vertices have to be configured before the vertices are filled.
     * @see VertexBuffer::configProperty VertexBuffer::configStride VertexBuffer::constructProperty VertexBuffer::constructRawMem
     */
    VertexBuffer()
        : offsets_{ invalidOffset, invalidOffset, invalidOffset, invalidOffset, invalidOffset, invalidOffset },
        data_(), properties_(0u), stride_(invalidStride) {}

    ~VertexBuffer() = default;
    VertexBuffer(const VertexBuffer&) = default;
    VertexBuffer(VertexBuffer&&) noexcept = default;
    VertexBuffer& operator=(const VertexBuffer&) = default;
    VertexBuffer& operator=(VertexBuffer&&) noexcept = default;

    /**
     * @brief Copys other vertex buffer with only the specified properties.     
     * The order of properties laid out in the memory becomes same as the order of the specified properties as the arguments.
     * @tparam Props Property Enumerations.
     * @param other Vertex buffer to copy.
     * @param props Properties to copy.
     * @see Vertex::Properties
     */
    template < std::same_as<Vertex::Properties>... Props >
    VertexBuffer(const VertexBuffer& other, Props... props)
        : offsets_{ invalidOffset, invalidOffset, invalidOffset, invalidOffset, invalidOffset, invalidOffset },
        data_(), properties_(0u), stride_(0u) {
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

    /**
     * @brief Configures the property of the vertex.
     * @param prop Property to configure.
     * @param offset Offset of the property in the vertex memory layout.
     * @see Vertex::Properties VertexBuffer::configStride VertexBuffer::constructProperty VertexBuffer::constructRawMem
     */
    void configProperty(Vertex::Properties prop, offset_t offset) NOEXCEPT {
        offsets_[toIdx(prop)] = offset;
        properties_ = properties_ | prop;
    }
    /**
     * @brief Configures the stride of the vertex buffer.
     * @param stride Stride of the vertex buffer.
     * @see VertexBuffer::configProperty VertexBuffer::constructProperty VertexBuffer::constructRawMem
     */
    void configStride(std::size_t stride) NOEXCEPT {
        stride_ = stride;
    }
    /**
     * @brief Constructs the property of the vertex by copying consecutive memory blocks from the given data.
     * @param prop Property to construct.
     * @param data Data to copy.
     * @param propByteWidth Byte width of the property.
     * @param cnt Number of memory blocks to copy.
     * @param stride The stride between the memory blocks.
     * @details i-th vertex's property is copied from the i-th memory block in the data as follows:    
     * copy `propByteWidth` from [`data` + i * `stride`] to [`vertex buffer's address` + i * `vertex buffer's stride` + `configured property's offset`].
     * @note The properties have to be configured before the vertices are filled.
     * @see Vertex::Properties VertexBuffer::configProperty VertexBuffer::configStride VertexBuffer::constructRawMem
     */
    void constructProperty( Vertex::Properties prop, const void* data,
        std::size_t propByteWidth, std::size_t cnt, std::size_t stride
    );
    /**
     * @brief Constructs the vertices by copying from a raw memory.
     * @param data Raw memory to copy.
     * @param byteWidth Byte width of the raw memory.
     * @note The properties have to be configured before the vertices are filled.
     * @see VertexBuffer::configProperty VertexBuffer::configStride VertexBuffer::constructProperty
     */
    void constructRawMem(const void* data, std::size_t byteWidth) {
        data_.resize(byteWidth);
        std::memcpy(data_.data(), data, byteWidth);
    }
    /**
     * @brief Gets the raw memory pointer of the vertex buffer.
     * @return `void*` Raw memory pointer.
     * @note Combined with VertexBuffer::stride and VertexBuffer::byteWidth,    
     * it can be used to create an API-specific vertex buffer.
     * @see VertexBuffer::stride VertexBuffer::byteWidth Mesh
     */
    void* rawMem() NOEXCEPT {
        return data_.data();
    }
    /**
     * @brief Gets the raw memory pointer of the vertex buffer.
     * @return `const void*` Raw memory pointer.
     * @note Combined with VertexBuffer::stride and VertexBuffer::byteWidth,    
     * it can be used to create an API-specific vertex buffer.
     * @see VertexBuffer::stride VertexBuffer::byteWidth Mesh
     */
    const void* rawMem() const NOEXCEPT {
        return data_.data();
    }
    /**
     * @brief Gets the stride of the vertex buffer.
     * @return `std::size_t` Stride of the vertex buffer.
     * @note Combined with VertexBuffer::rawMem and VertexBuffer::byteWidth,    
     * it can be used to create an API-specific vertex buffer.    
     * @see VertexBuffer::rawMem VertexBuffer::byteWidth Mesh
     */
    std::size_t stride() const NOEXCEPT {
        return stride_;
    }
    /**
     * @brief Gets the byte width of the vertex buffer.
     * @return `std::size_t` Byte width of the vertex buffer.
     * @note Combined with VertexBuffer::rawMem and VertexBuffer::stride,    
     * it can be used to create an API-specific vertex buffer.
     * @see VertexBuffer::rawMem VertexBuffer::stride Mesh VertexBuffer::propByteWidth VertexBuffer::size
     */
    std::size_t byteWidth() const NOEXCEPT {
        return data_.size();
    }
    /**
     * @brief Gets the byte width of the property.
     * @param prop Property to get the byte width.
     * @return `std::size_t` Byte width of the property.
     * @see Vertex::Properties VertexBuffer::byteWidth VertexBuffer::size
     */
    std::size_t propByteWidth(Vertex::Properties prop) const {
        if (!contains(prop)) {
            throw;  // TODO: add exception
        }

        auto propIdx = toIdx(prop);
        if (propIdx == Vertex::numProperties - 1) {
            return stride_ - offsets_[propIdx];
        }
        if (offsets_[propIdx + 1] == invalidOffset) {
            return stride_ - offsets_[propIdx];
        }

        return offsets_[propIdx + 1] - offsets_[propIdx];
    }
    /**
     * @brief Gets the number of vertices in the vertex buffer.
     * @return `std::size_t` Number of vertices.
     * @see VertexBuffer::byteWidth VertexBuffer::propByteWidth
     */
    std::size_t size() const {
        return data_.size() / stride_;
    }
    /**
     * @brief Gets the vertex view in the vertex buffer.
     * @param idx Index of the vertex.
     * @return `Vertex` Vertex in the vertex buffer.
     * @details It creates Vertex with memory [`vertex buffer's address` + `idx` * `vertex buffer's stride`].
     * @see Vertex VertexBuffer::get VertexBuffer::stride VertexBuffer::rawMem
     */
    Vertex operator[](std::size_t idx) {
        return Vertex(data_.data() + idx * stride_, this);
    }
    /**
     * @brief Gets the vertex view in the vertex buffer.
     * @param idx Index of the vertex.
     * @return `const Vertex` Vertex in the vertex buffer.
     * @details It creates Vertex with memory [`vertex buffer's address` + `idx` * `vertex buffer's stride`].
     * @see Vertex VertexBuffer::get VertexBuffer::stride VertexBuffer::rawMem
     */
    const Vertex operator[](std::size_t idx) const {
        return Vertex( const_cast<std::uint8_t*>( data_.data() + idx * stride_ ),
            const_cast<VertexBuffer*>(this)
        );
    }
    /**
     * @brief Gets the vertex view in the vertex buffer.     
     * it is the same as calling VertexBuffer::operator[].
     * @param idx Index of the vertex.
     * @return `Vertex` Vertex in the vertex buffer.
     * @see Vertex VertexBuffer::operator[]
     */
    Vertex get(std::size_t idx) {
        return (*this)[idx];
    }
    /**
     * @brief Gets the vertex view in the vertex buffer.
     * @param idx Index of the vertex.
     * @return `const Vertex` Vertex in the vertex buffer.
     * @see Vertex VertexBuffer::operator[]
     */
    const Vertex get(std::size_t idx) const {
        return (*this)[idx];
    }
    /**
     * @brief Gets the offset of the property in the vertex memory layout.
     * @param prop Property to get the offset.
     * @return `offset_t` Offset of the property.
     * @see Vertex::Properties VertexBuffer::configProperty VertexBuffer::constructProperty VertexBuffer::constructRawMem
     */
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