#ifndef __VERTEX_HPP
#define __VERTEX_HPP

#include <vector>
#include <array>
#include <concepts>
#include <bitset>

#include <cstdint>
#include <cstdlib>

#include "enumUtil.hpp"

#include "config.hpp"

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

    /**
     * @brief Properties of a vertex.
     * @details Each property is a flag that represents a property of a vertex.    
     * The flags can be combined with logical operators.     
     * The combined flags can be used to specify what properties a vertex contains.
     * @see VertexBuffer
     */
    enum class Properties : std::uint32_t {
        Position3D,
        Normal3D,
        TexCoord2D0,
        TexCoord2D1,
        Tangent3D,
        Bitangent3D,
        Color3D,
        Color4D,
        BoneWeights4D,
        BoneIndices4D,
        SIZE
    };

    static constexpr std::size_t propByteWidth(Properties prop) NOEXCEPT {
        return propByteWidths[etoi(prop)];
    }

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
    static constexpr std::size_t propByteWidths[etoi(Properties::SIZE)] = {
        /* Position3D */ 3 * sizeof(float),
        /* Normal3D */ 3 * sizeof(float),
        /* TexCoord2D0 */ 2 * sizeof(float),
        /* TexCoord2D1 */ 2 * sizeof(float),
        /* Tangent3D */ 3 * sizeof(float),
        /* Bitangent3D */ 3 * sizeof(float),
        /* Color3D */ 3 * sizeof(float),
        /* Color4D */ 4 * sizeof(float),
        /* BoneWeights4D */ sizeof(float) * 4,
        /* BoneIndices4D */ sizeof(std::uint32_t) * 4
    };

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
    using VBFlag = std::bitset<etoi(Vertex::Properties::SIZE)>;

    /**
     * @brief Constructs an empty vertex buffer.
     * @details The memory layout of the vertices is in invalid state.     
     * The properties of the vertices have to be configured before the vertices are filled.
     * @see VertexBuffer::configProperty VertexBuffer::configStride VertexBuffer::constructProperty VertexBuffer::constructRawMem
     */
    VertexBuffer()
        : offsets_{ invalidOffset, invalidOffset, invalidOffset, invalidOffset, invalidOffset, invalidOffset },
        data_(), stride_(invalidStride), properties_(0u) {}

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
        data_(), stride_(0u), properties_(0u) {
        for (auto prop : { props... }) {
            stride_ += Vertex::propByteWidth(prop);
        }

        data_.resize(stride_ * other.size());

        offset_t accOffset = 0;

        (fetchProp(other, props, accOffset), ...);
    }

    bool contains(Vertex::Properties prop) const NOEXCEPT {
        return properties_.test(etoi(prop));
    }

    /**
     * @brief Configures the property of the vertex.
     * @param prop Property to configure.
     * @param offset Offset of the property in the vertex memory layout.
     * @see Vertex::Properties VertexBuffer::configStride VertexBuffer::constructProperty VertexBuffer::constructRawMem
     */
    void configProperty(Vertex::Properties prop, offset_t offset) NOEXCEPT {
        offsets_[etoi(prop)] = offset;
        properties_.set(etoi(prop));
    }
    /**
     * @brief Configures the stride of the vertex buffer.
     * @param stride Stride of the vertex buffer.
     * @see VertexBuffer::configProperty VertexBuffer::constructProperty VertexBuffer::constructRawMem
     */
    void configStride(std::size_t stride) NOEXCEPT {
        stride_ = stride;
    }

    void constructNullProperty(Vertex::Properties prop, std::size_t cnt);

    void constructProperty( Vertex::Properties prop, const void* data,
        std::size_t cnt, std::size_t stride
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

    std::vector<std::uint8_t>&& vertices() && noexcept {
        return std::move(data_);
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
        return offsets_[etoi(prop)];
    }

    const VBFlag& propFlag() const NOEXCEPT {
        return properties_;
    }

private:
    void fetchProp(const VertexBuffer& other, Vertex::Properties prop, offset_t& accOffset);

    std::array<offset_t, etoi(Vertex::Properties::SIZE)> offsets_;
    std::vector<std::uint8_t> data_;
    std::size_t stride_;
    VBFlag properties_;
};

template <typename T>
T& Vertex::get(Properties prop) {
    return *reinterpret_cast<T*>(pStart_ + pBuf_->offsets_[etoi(prop)]);
}

template <typename T>
const T& Vertex::get(Properties prop) const {
    return *reinterpret_cast<const T*>(pStart_ + pBuf_->offsets_[etoi(prop)]);
}

template <typename T>
void Vertex::set(Properties prop, const T& val) {
    std::memcpy(pStart_ + pBuf_->offsets_[etoi(prop)], &val, sizeof(T));
}

template <typename T>
void Vertex::set(Properties prop, T&& val) {
    std::memcpy(pStart_ + pBuf_->offsets_[etoi(prop)], &val, sizeof(T));
}

inline void* Vertex::operator[](Properties prop) {
    return pStart_ + pBuf_->offsets_[etoi(prop)];
}

} // namespace gfx

#endif // __VERTEX_HPP