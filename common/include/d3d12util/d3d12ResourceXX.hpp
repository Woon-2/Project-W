#ifndef __d3d12Resource_HPP
#define __d3d12Resource_HPP

#include "d3d12util/d3d12Low.hpp"
#include "texLoader/DDSTextureLoader12.h"
#include "coord.hpp"

#include "dxutil/dxexcept.hpp"

#include "vertex.hpp"

#include "game/animSystem.hpp"

#include <bitset>
#include <filesystem>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <memory>
#include <ranges>
#include <algorithm>
#include <iostream>
#include <any>
#include <unordered_map>
#include <typeindex>
#include <concepts>

#include "enumUtil.hpp"
#include "memUtil.hpp"
#include "TMP.hpp"

namespace gfx {

namespace d3d12 {

namespace detail {
    template <class T>
    struct ResTypeMapper {};
}

class ResourceStorage {
public:
    enum class ResType {
        Texture,
        TexArray,
        TexCube,
        RefModel,
        Skeleton,
        BVHPath,
        AnimClip,
    };

    using SlotID = std::string;
    using ResID = std::string;

    template <class T>
    using ContainerType = std::unordered_map<ResID, T>;

    class Slot {
    public:
        Slot(ResType type)
            : container_(buildContainer(type)), valueType_(type) {}

        bool contains(ResType type) const {
            return valueType_ == type;
        }

        template <class T>
        bool contains(const ResID& resID) const {
            return any_cast<const ContainerType<T>>(&container_)->contains(resID);
        }

        template <class T, class Loader, class ... Args>
            requires std::invocable<Loader, Args...>
        T& load(const ResID& resID, Loader&& loader, Args&& ... args) {
            if (valueType_ != detail::ResTypeMapper<T>::resType) [[unlikely]] {
                throw std::runtime_error("[Description] ResourceStorage::Slot::load: ResType mismatch detected.\n"
                    "Tried to load an instance of wrong resource type on ResourceStorage."
                );
            }

            auto [it, emplaced] = any_cast<ContainerType<T>>(&container_)->try_emplace(
                resID, std::invoke(loader, std::forward<Args>(args)...)
            );
            if (!emplaced) [[unlikely]] {
                throw std::runtime_error("[Description] ResourceStorage::Slot::load: resource ID already registered on the slot.");
            }
            return it->second;
        }

        template <class T, class ... Args>
        T& load(const ResID& resID, Args&& ... args) {
            if (valueType_ != detail::ResTypeMapper<T>::resType) [[unlikely]] {
                throw std::runtime_error("[Description] ResourceStorage::Slot::load: ResType mismatch detected.\n"
                    "Tried to load an instance of wrong resource type on ResourceStorage."
                );
            }

            auto [it, emplaced] = any_cast<ContainerType<T>>(&container_)->try_emplace(
                resID, std::forward<Args>(args)...
            );
            if (!emplaced) [[unlikely]] {
                throw std::runtime_error("[Description] ResourceStorage::Slot::load: resource ID already registered on the slot.");
            }
            return it->second;
        }

        template <class T>
        const T* get(const ResID& resID) const {
            if (valueType_ != detail::ResTypeMapper<T>::resType) [[unlikely]] {
                throw std::runtime_error("[Description] ResourceStorage::Slot::get: ResType mismatch detected.\n"
                    "Tried to get an instance of wrong resource type from ResourceStorage."
                );
            }

            return &any_cast<const ContainerType<T>>(&container_)->at(resID);
        }

        template <class T>
        T* get(const ResID& resID) {
            if (valueType_ != detail::ResTypeMapper<T>::resType) [[unlikely]] {
                throw std::runtime_error("[Description] ResourceStorage::Slot::get: ResType mismatch detected.\n"
                    "Tried to get an instance of wrong resource type from ResourceStorage."
                );
            }

            return &any_cast<ContainerType<T>>(&container_)->at(resID);
        }

    private:
        static AnyMoveOnly buildContainer(ResType type);

        AnyMoveOnly container_;
        ResType valueType_;
    };

    Slot& addSlot(SlotID id, ResType resType) {
        auto [it, emplaced] = slots_.try_emplace(id, resType);
        if (!emplaced) {
            throw std::runtime_error("[Description] ResourceStorage::addSlot: the slot with id \""
                + std::string(id) + "\" already exists in the storage."
            );
        }

        return it->second;
    }

    Slot& slot(SlotID id) {
        return slots_.at(id);
    }

    const Slot& slot(SlotID id) const {
        return slots_.at(id);
    }

    bool hasSlot(SlotID id) const {
        return slots_.contains(id);
    }

private:
    // each slot is a container of resources
    std::unordered_map<SlotID, Slot> slots_;
};


class UploadBuffer : public D3D12Resource {
public:
    UploadBuffer() = default;

    UploadBuffer(D3D12Device& device, const void* data, std::size_t byteWidth,
        std::size_t srcOffset, std::size_t destOffset,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
    );

    UploadBuffer(D3D12Device& device, const void* data, std::size_t byteWidth,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
    );

    UploadBuffer(D3D12Device& device, std::size_t byteWidth,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
    );

    void stage(const void* data, std::size_t byteWidth,
        std::size_t srcOffset = 0u, std::size_t destOffset = 0u
    );

private:
    std::uint8_t* pMappedData_;
};

class DefaultBuffer : public D3D12Resource {
public:
    DefaultBuffer() = default;

    DefaultBuffer(D3D12Device& device, std::size_t byteWidth,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
    );
};

class TextureResource : public D3D12Resource {
public:
    enum class Type {
        Texture,
        TextureArray,
        TextureCube
    };

    struct Desc {
        std::uint32_t width;
        std::uint32_t height;
        std::uint16_t arraySize;
        std::uint16_t mipLevels;
        DXGI_FORMAT format;
        DXGI_SAMPLE_DESC sampleDesc;
        D3D12_RESOURCE_FLAGS flags;
    };

    TextureResource( D3D12Device& device, const Desc& texResDesc,
        D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState
    );

    TextureResource( D3D12Device& device, const Desc& texResDesc,
        D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
        const D3D12_CLEAR_VALUE& optimizedClearValue
    );

    TextureResource( D3D12Device& device, const Desc& texResDesc,
        D3D12_HEAP_TYPE heapType
    );

    TextureResource( D3D12Device& device, const Desc& texResDesc,
        D3D12_HEAP_TYPE heapType, const D3D12_CLEAR_VALUE& optimizedClearValue
    );

    TextureResource( D3D12Device& device, D3D12GfxCmdList& cmdList, 
        const std::filesystem::path& path
    );

private:
    struct LoadDDSReturnType {
        wrl::ComPtr<ID3D12Resource> res;
        std::unique_ptr<std::uint8_t[]> ddsData;
        std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    };

    static LoadDDSReturnType loadDDS(
        D3D12Device& device, D3D12GfxCmdList& cmdList,
        const std::filesystem::path& path
    );

    std::unique_ptr<std::uint8_t[]> data_;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources_;
};

class Texture : public TextureResource {
public:
    struct Desc {
        std::uint32_t width;
        std::uint32_t height;
        std::uint16_t mipLevels;
        DXGI_FORMAT format;
        DXGI_SAMPLE_DESC sampleDesc;
        D3D12_RESOURCE_FLAGS flags;
    };

    static constexpr std::size_t idxSrv = 0u;

    Texture( D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const Desc& texResDesc, D3D12_HEAP_TYPE heapType
    ) : Texture(device, tex2dRange, texResDesc, heapType,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
        ) {}

    Texture( D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const Desc& texResDesc, D3D12_HEAP_TYPE heapType,
        const D3D12_CLEAR_VALUE& optimizedClearValue
    ) : Texture(device, tex2dRange, texResDesc, heapType,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, optimizedClearValue
        ) {}

    Texture( D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const Desc& texResDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        D3D12_HEAP_TYPE heapType
    ) : Texture(device, tex2dRange, texResDesc, srvDesc, heapType,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
        ) {}

    Texture( D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const Desc& texResDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        D3D12_HEAP_TYPE heapType, const D3D12_CLEAR_VALUE& optimizedClearValue
    ) : Texture(device, tex2dRange, texResDesc, srvDesc, heapType,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, optimizedClearValue
        ) {}

    Texture( D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const Desc& texResDesc, D3D12_HEAP_TYPE heapType,
        D3D12_RESOURCE_STATES initialState
    ) : TextureResource(device, convertDesc(texResDesc), heapType, initialState) {
        makeDefSrv(device, tex2dRange.alloc());
    }

    Texture( D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const Desc& texResDesc, D3D12_HEAP_TYPE heapType,
        D3D12_RESOURCE_STATES initialState,
        const D3D12_CLEAR_VALUE& optimizedClearValue
    ) : TextureResource(device, convertDesc(texResDesc), heapType, initialState, optimizedClearValue) {
        makeDefSrv(device, tex2dRange.alloc());
    }

    Texture( D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const Desc& texResDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState
    ) : TextureResource(device, convertDesc(texResDesc), heapType, initialState) {
        makeSrv(srvDesc, device, tex2dRange.alloc());
    }

    Texture( D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const Desc& texResDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
        const D3D12_CLEAR_VALUE& optimizedClearValue
    ) : TextureResource(device, convertDesc(texResDesc), heapType, initialState, optimizedClearValue) {
        makeSrv(srvDesc, device, tex2dRange.alloc());
    }

    Texture(D3D12Device& device, D3D12GfxCmdList& cmdList,
        DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const std::filesystem::path& path
    ) : TextureResource(device, cmdList, path) {
        makeDefSrv(device, tex2dRange.alloc());
    }

    Texture(D3D12Device& device, D3D12GfxCmdList& cmdList,
        DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        const std::filesystem::path& path
    ) : TextureResource(device, cmdList, path) {
        makeSrv(srvDesc, device, tex2dRange.alloc());
    }

    std::size_t srvOffsetInRange() {
        return view(idxSrv).offset();
    }

private:
    static TextureResource::Desc convertDesc(const Desc& desc);
};

class TextureArray : public TextureResource {
public:
    static constexpr std::size_t idxSrv = 0u;

    TextureArray(D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dArrRange,
        const Desc& texResDesc, D3D12_HEAP_TYPE heapType
    ) : TextureResource(device, texResDesc, heapType) {
        makeDefSrv(device, tex2dArrRange.alloc());
    }

    TextureArray(D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dArrRange,
        const Desc& texResDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        D3D12_HEAP_TYPE heapType
    ) : TextureResource(device, texResDesc, heapType) {
        makeSrv(srvDesc, device, tex2dArrRange.alloc());
    }

    TextureArray(D3D12Device& device, D3D12GfxCmdList& cmdList,
        DescriptorRange<DescriptorHeapGPU>& tex2dArrRange,
        const std::filesystem::path& path
    ) : TextureResource(device, cmdList, path) {
        makeDefSrv(device, tex2dArrRange.alloc());
    }

    TextureArray(D3D12Device& device, D3D12GfxCmdList& cmdList,
        DescriptorRange<DescriptorHeapGPU>& tex2dArrRange,
        const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        const std::filesystem::path& path
    ) : TextureResource(device, cmdList, path) {
        makeSrv(srvDesc, device, tex2dArrRange.alloc());
    }

    std::size_t srvOffsetInRange() {
        return view(idxSrv).offset();
    }
};

class TextureCube : public TextureResource {
public:
    static constexpr std::size_t idxSrv = 0u;

    TextureCube(D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& texCubeRange,
        const Desc& texResDesc, D3D12_HEAP_TYPE heapType
    ) : TextureResource(device, texResDesc, heapType) {
        makeDefSrv(device, texCubeRange.alloc());
    }

    TextureCube(D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& texCubeRange,
        const Desc& texResDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        D3D12_HEAP_TYPE heapType
    ) : TextureResource(device, texResDesc, heapType) {
        makeSrv(srvDesc, device, texCubeRange.alloc());
    }

    TextureCube(D3D12Device& device, D3D12GfxCmdList& cmdList,
        DescriptorRange<DescriptorHeapGPU>& texCubeRange,
        const std::filesystem::path& path
    ) : TextureResource(device, cmdList, path) {
        makeDefSrv(device, texCubeRange.alloc());
    }

    TextureCube(D3D12Device& device, D3D12GfxCmdList& cmdList,
        DescriptorRange<DescriptorHeapGPU>& texCubeRange,
        const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        const std::filesystem::path& path
    ) : TextureResource(device, cmdList, path) {
        makeSrv(srvDesc, device, texCubeRange.alloc());
    }

    std::size_t srvOffsetInRange() {
        return view(idxSrv).offset();
    }
};

class SamplerStorage {
public:
    enum class Indices {
        NearestWrap,
		TrilinearWrap,
		NearestBorder,
		TrilinearBorder,
		NearestClamp,
		TrilinearClamp,
		NearestComparison,
		BilinearComparison
    };

    void init( D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& samRange,
        DescriptorRange<DescriptorHeapGPU>& samCmpRange
    );

    const Sampler& get(Indices idx) const NOEXCEPT {
        return storedSamplers_[etoi(idx)];
    }

    const Sampler& operator[](Indices idx) const NOEXCEPT {
        return get(idx);
    }

private:
    std::vector<Sampler> storedSamplers_;
};

class Material {
public:
    using ResourceType = TextureResource::Type;

    enum class MapType {
        Height,
        Albedo,
        Normal,
        Roughness,
        Metallic,
        MetallicSmoothness,
        Emmisive,
        AmbientOcclusion,
        Shadow,
        Size
    };

    enum class ConstantType {
        Albedo,
        Roughness,
        Metallic,
        Emmisive,
        AmbientOcclusion,
        AlbedoConstantMapRatio,
        RoughnessConstantMapRatio,
        MetallicConstantMapRatio,
        EmmisiveConstantMapRatio,
        AmbientOcclusionConstantMapRatio,
        TileSize,
        TileOffset,
        Size
    };

    enum class ColorSpace {
        SRGB,
        Linear
    };

    struct MapRef {
        static constexpr std::uint32_t invalid = std::uint32_t(-1);

        std::uint32_t type;
        std::uint32_t resourceIdx;
        std::uint32_t arrayIdx;
        std::uint32_t colorSpace;

        dx::XMUINT4 toxm() const {
            return dx::XMUINT4{ type, resourceIdx, arrayIdx, colorSpace };
        }

        auto operator<=>(const MapRef&) const = default;
    };

    Material();
    void addMapRef(MapType type, const MapRef& mapRef);
    void addTexRes(MapType type, const Texture& tex);
    void addTexRes(MapType type, const TextureArray& tex);
    void addTexRes(MapType type, const TextureCube& tex);
    void MU_CALLCONV addConstant(ConstantType type, mu::Vec2 constant);
    void MU_CALLCONV addConstant(ConstantType type, mu::Vec3 constant);
    void MU_CALLCONV addConstant(ConstantType type, mu::Vec4 constant);
    void addConstant(ConstantType type, float constant);

    const MapRef& mapRef(MapType type) const {
        return mapRefs_[etoi(type)];
    }

    auto& mapRefs() noexcept {
        return mapRefs_;
    }

    const auto& mapRefs() const noexcept {
        return mapRefs_;
    }

    const RawMemory<16>& constant(ConstantType type) const {
        return constants_[etoi(type)];
    }

    template <class T>
    const T& constant(ConstantType type) const {
        return *reinterpret_cast<const T*>(&constants_[etoi(type)]);
    }

    bool contains(MapType type) const {
        return mapRef(type).resourceIdx != Material::MapRef::invalid;
    }
    
private:
    std::vector<MapRef> mapRefs_;
    std::vector<RawMemory<16>> constants_;
};

class VertexBuffer : public DefaultBuffer {
public:
    VertexBuffer() = default;

    VertexBuffer(D3D12Device& device, D3D12GfxCmdList& cmdList,
        std::vector<std::uint8_t>&& pData, std::size_t byteWidth, std::size_t stride,
		std::bitset<etoi(Vertex::Properties::SIZE)> attribs
    );

    VertexBuffer(D3D12Device& device, D3D12GfxCmdList& cmdList,
        gfx::VertexBuffer&& vertexBuffer
    ) : VertexBuffer(device, cmdList, std::move(vertexBuffer).vertices(), vertexBuffer.byteWidth(),
        vertexBuffer.stride(), vertexBuffer.propFlag()
    ) {}

    template <std::ranges::range R>
		requires std::same_as<std::ranges::range_value_t<R>, VertexBuffer>
    static void bind(D3D12GfxCmdList& cmdList, std::size_t slot, const R& vbs) {
		auto views = std::vector<D3D12_VERTEX_BUFFER_VIEW>(vbs.size());
		std::ranges::transform( vbs, views.begin(), [](const auto& vb) {
			return vb.vbview();
		} );
        DX_THROW_FAILED_VOID( cmdList.get()->IASetVertexBuffers(
            static_cast<UINT>(slot), static_cast<UINT>( views.size() ), views.data()
        ) );
	}

	const auto& attributes() const noexcept { return attribs_; }
    const void* cpuMem() const noexcept { return cpuMem_.data(); }
    void releaseCpuMem() noexcept { cpuMem_.clear(); cpuMem_.shrink_to_fit(); }

private:
    std::vector<std::uint8_t> cpuMem_;
	std::bitset<etoi(Vertex::Properties::SIZE)> attribs_;
};

class IndexBuffer : public DefaultBuffer {
public:
    IndexBuffer()
        : DefaultBuffer(), size_(0u), indexFormat_(DXGI_FORMAT_UNKNOWN) {}

    IndexBuffer(D3D12Device& device, D3D12GfxCmdList& cmdList,
        const void* pData, DXGI_FORMAT indexFormat, std::size_t indexCnt
    );

    IndexBuffer(D3D12Device& device, D3D12GfxCmdList& cmdList,
        std::vector<std::uint8_t>&& pData, DXGI_FORMAT indexFormat, std::size_t indexCnt
    );

	void bind(D3D12GfxCmdList& cmdList) const {
		cmdList.get()->IASetIndexBuffer(&ibview());
	}

    const void* cpuMem() const noexcept { return cpuMem_.data(); }
    void releaseCpuMem() noexcept { cpuMem_.clear(); cpuMem_.shrink_to_fit(); }

    std::size_t size() const noexcept {
        return size_;
    }

private:
    static std::size_t indexByteWidth(DXGI_FORMAT indexFormat);

    std::vector<std::uint8_t> cpuMem_;
    std::size_t size_;
    DXGI_FORMAT indexFormat_;
};

class RefMesh;

class RefSubmesh {
public:
    friend class RefMesh;
    friend class RefModel;
    friend class Submesh;

    RefSubmesh( RefMesh* parent = nullptr,
        D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    ) : parent_(parent), ib_(), material_(), topology_(topology) {}

    RefSubmesh(const RefSubmesh& other) = delete;
    RefSubmesh(RefSubmesh&& other) noexcept;
    RefSubmesh& operator=(const RefSubmesh& other) = delete;
    RefSubmesh& operator=(RefSubmesh&& other) noexcept;
    ~RefSubmesh() = default;

    const RefMesh* parent() const noexcept {
        return parent_;
    }

    const auto& ib() const noexcept {
        return ib_;
    }

    D3D12_PRIMITIVE_TOPOLOGY topology() const noexcept {
        return topology_;
    }

    Material& material() noexcept {
        return material_;
    }

    const Material& material() const noexcept {
        return material_;
    }

    template <class T>
    const T& constant(Material::ConstantType type) const {
        return material_.constant<T>(type);
    }

// private:
    void draw(D3D12GfxCmdList& cmdList, std::size_t instanceCnt, std::size_t vbLayoutIdx) const;

    RefMesh* parent_;
    IndexBuffer ib_;
    Material material_;
    D3D12_PRIMITIVE_TOPOLOGY topology_;
};

class RefModel {
public:
    class Node {
    public:
        friend class RefModel;
        Node(const RefModel* pRefModel = nullptr)
            : coord_(), name_(), meshes_(), children_(), pRefModel_(pRefModel) {}
        Node(const Node& other) = delete;
        Node(Node&& other) noexcept;
        Node& operator=(const Node& other) = delete;
        Node& operator=(Node&& other) noexcept;
        ~Node() = default;

        void addMesh(RefMesh&& mesh);
        void addChild(Node* child);
        auto& coord() noexcept { return coord_; }
        const auto& coord() const noexcept { return coord_; }
        auto& meshes() noexcept { return meshes_; }
        const auto& meshes() const noexcept { return meshes_; }
        auto& children() noexcept { return children_; }
        const auto& children() const noexcept { return children_; }

    private:
        gfx::coord::System coord_;
        std::string name_;
        std::vector<RefMesh> meshes_;
        std::vector<Node*> children_;
        const RefModel* pRefModel_;
    };

    RefModel()
        : nodeStorage_(), textureMap_(), pRoot_(nullptr) {}

    ~RefModel() = default;
    RefModel(const RefModel& other) = delete;
    RefModel(RefModel&& other) noexcept;
    RefModel& operator=(const RefModel& other) = delete;
    RefModel& operator=(RefModel&& other) noexcept;

    static RefModel loadHierarchyFromFile( const std::filesystem::path& geometryPath,
        D3D12Device& device, D3D12GfxCmdList& cmdList,
        const ResourceStorage::Slot& texSlot,
        const ResourceStorage::Slot& texArraySlot,
        const ResourceStorage::Slot& texCubeSlot
    );

    void arrangeVBs( D3D12Device& device, D3D12GfxCmdList& cmdList,
        std::size_t layoutIdx, const std::vector<std::vector<Vertex::Properties>>& vbProps
    );

    void map(const Material::MapRef& mapRef, const DescriptorGPU& descriptor) {
        textureMap_[mapRef] = descriptor;
    }

    auto& nodes() noexcept { return nodeStorage_; }
    const auto& nodes() const noexcept { return nodeStorage_; }
    Node* root() noexcept { return pRoot_; }
    const Node* root() const noexcept { return pRoot_; }

    bool hasSkeleton() const noexcept {
        return pSkeleton_ != nullptr;
    }
    void linkSkeleton(const Skeleton* skeleton) noexcept {
        pSkeleton_ = skeleton;
    }
    const Skeleton* skeleton() const noexcept {
        return pSkeleton_;
    }

protected:
    static void loadNodesFromFile( D3D12Device& device, D3D12GfxCmdList& cmdList,
        FILE* pInFile, Node& node, RefModel& model
    );

    std::vector<Node> nodeStorage_;
    std::map<Material::MapRef, DescriptorGPU> textureMap_;
    Node* pRoot_;
    const Skeleton* pSkeleton_;
};

class RefMesh {
public:
    friend class RefModel;

    RefMesh(RefModel::Node* parent = nullptr)
        : name_(), vbLayouts_(), submeshes_(), parent_(parent) {}

    RefMesh(const RefMesh& other) = delete;
    RefMesh(RefMesh&& other) noexcept;
    RefMesh& operator=(const RefMesh& other) = delete;
    RefMesh& operator=(RefMesh&& other) noexcept;
    ~RefMesh() = default;

    void arrangeVBs( D3D12Device& device, D3D12GfxCmdList& cmdList,
        std::size_t layoutIdx, const std::vector<std::vector<Vertex::Properties>>& vbProps
    );

    const auto& vbs(std::size_t layoutIdx) const noexcept {
        return vbLayouts_[layoutIdx];
    }

    std::size_t vbLayoutCnt() const noexcept {
        return vbLayouts_.size();
    }

    const auto& submeshes() const noexcept {
        return submeshes_;
    }

    const RefModel::Node* parent() const noexcept {
        return parent_;
    }
    
    static RefMesh loadGeometryFromFile(D3D12Device& device, D3D12GfxCmdList& cmdList, FILE* pInFile);
    static void loadMaterialsFromFile(D3D12Device& device, D3D12GfxCmdList& cmdList,
        const std::map<Material::MapRef, DescriptorGPU>& textureMap,
        FILE* pInFile, RefMesh& mesh
    );

// private:
    static void loadMaterialFromFile( D3D12Device& device, D3D12GfxCmdList& cmdList,
        const std::map<Material::MapRef, DescriptorGPU>& textureMap,
        FILE* pInFile, RefMesh& mesh, std::size_t materialIdx
    );

    std::string name_;
    std::vector< std::vector<VertexBuffer> > vbLayouts_;
    std::vector<RefSubmesh> submeshes_;
    RefModel::Node* parent_;
};

class Mesh;

class Submesh {
public:
    friend class Shader;
    friend class Mesh;

    Submesh(Mesh* parent = nullptr, const RefSubmesh* pRefSubmesh = nullptr);
    Submesh(const Submesh& other) = default;
    Submesh(Submesh&& other) noexcept;
    Submesh& operator=(const Submesh& other) = default;
    Submesh& operator=(Submesh&& other) noexcept;
    ~Submesh() = default;

    const Mesh* parent() const noexcept {
        return parent_;
    }

    void draw(D3D12GfxCmdList& cmdList, std::size_t instanceCnt, std::size_t vbLayoutIdx) const {
        pRefSubmesh_->draw(cmdList, instanceCnt, vbLayoutIdx);
    }

    Material& material() noexcept {
        return material_;
    }

    const Material& material() const noexcept {
        return material_;
    }

    void setMaterial(Material& material) noexcept {
        material_ = material;
    }

    const RefSubmesh* refSubmesh() const noexcept {
        return pRefSubmesh_;
    }

private:
    Mesh* parent_;
    const RefSubmesh* pRefSubmesh_;
    Material material_;
};

class Mesh;

class Model {
public:
    class Node {
    public:
        friend class Model;
        Node(const Model* pModel = nullptr)
            : coord_(), meshes_(), children_(), pModel_(pModel) {}
        Node(const Node& other);
        Node(Node&& other) noexcept;
        Node& operator=(const Node& other);
        Node& operator=(Node&& other) noexcept;
        ~Node() = default;

        void addMesh(Mesh&& mesh);
        void emplaceMesh(const RefMesh& refMesh);
        void addChild(Node* child);
        auto& coord() noexcept { return coord_; }
        const auto& coord() const noexcept { return coord_; }
        auto& meshes() noexcept { return meshes_; }
        const auto& meshes() const noexcept { return meshes_; }

    private:
        gfx::coord::System coord_;
        std::vector<Mesh> meshes_;
        std::vector<Node*> children_;
        const Model* pModel_;
    };

    Model() = default;
    ~Model() = default;
    Model(const RefModel& ref);
    Model(const Model& other);
    Model(Model&& other) noexcept;
    Model& operator=(const Model& other);
    Model& operator=(Model&& other) noexcept;

    auto& nodes() noexcept { return nodeStorage_; }
    const auto& nodes() const noexcept { return nodeStorage_; }

    Node* root() noexcept { return pRoot_; }
    const Node* root() const noexcept { return pRoot_; }

    void markRenderPass(const std::string& renderPass) {
        markedRenderPasses_.push_back(renderPass);
    }

    const auto& markedRenderPasses() const noexcept {
        return markedRenderPasses_;
    }

private:
    std::vector<std::string> markedRenderPasses_;
    std::vector<Node> nodeStorage_;
    Node* pRoot_;
};

class Mesh {
public:
    friend class Model;

    Mesh(const RefMesh& refMesh, Model::Node* parent = nullptr);
    Mesh(const Mesh& other);
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(const Mesh& other);
    Mesh& operator=(Mesh&& other) noexcept;
    ~Mesh() = default;

    const Model::Node* parent() const noexcept {
        return parent_;
    }

    const RefMesh* refMesh() const noexcept {
        return pRefMesh_;
    }

    auto& submeshes() noexcept {
        return submeshes_;
    }

    const auto& submeshes() const noexcept {
        return submeshes_;
    }

private:
    Model::Node* parent_;
    const RefMesh* pRefMesh_;
    std::vector<Submesh> submeshes_;
};

class ScreenQuad {
public:
    ScreenQuad() NOEXCEPT
        : pTex_(nullptr) {}

    ScreenQuad(const Texture* pTex) NOEXCEPT
        : pTex_(pTex) {}

    void link(const Texture* pTex) NOEXCEPT {
        pTex_ = pTex;
    }

    void unlink() NOEXCEPT {
        pTex_ = nullptr;
    }

    void draw(D3D12GfxCmdList& cmdList) const;

    Material::MapRef mapRef() const {
        return Material::MapRef{
            .type = etoi(Material::ResourceType::Texture),
            .resourceIdx = static_cast<std::uint32_t>( pTex_->view(pTex_->idxSrv).offset() ),
            .arrayIdx = 0u,
            .colorSpace = etoi(Material::ColorSpace::SRGB)
        };
    }

private:
    const Texture* pTex_;
};

class LevelChunkModel {
public:
    struct PatchVertex {
        dx::XMFLOAT3 pos;
        dx::XMFLOAT2 texCoord;
    };

    friend class LevelRegionModel;
    static void initChunkMesh(D3D12Device& device, D3D12GfxCmdList& cmdList);
    void draw(D3D12GfxCmdList& cmdList) const;

    void markRenderPass(const std::string& renderPass) {
        markedRenderPasses_.push_back(renderPass);
    }

    const auto& markedRenderPasses() const noexcept {
        return markedRenderPasses_;
    }

    mu::Mat4x4 MU_CALLCONV idxToWorld() const;
    const dx::XMUINT2 idx() const noexcept {
        return idx_;
    }

    Material& material() noexcept {
        return material_;
    }

    const Material& material() const noexcept {
        return material_;
    }

private:
    void load( const ResourceStorage::Slot& heightmapSlot,
        std::map<Material::MapRef, DescriptorGPU>& textureMap, std::istream& is
    );

    static VertexBuffer sChunkVb;
    static IndexBuffer sChunkIb;
    
    Material material_;
    std::vector<std::string> markedRenderPasses_;
    dx::XMUINT2 idx_;
};

class LevelRegionModel {
public:
    LevelRegionModel() = default;
    LevelRegionModel(const ResourceStorage::Slot& heightmapSlot, std::istream& is);

    LevelChunkModel& get(const dx::XMUINT2& idx);
    const LevelChunkModel& get(const dx::XMUINT2& idx) const;

private:
    std::vector<LevelChunkModel> chunks_;
};

namespace detail {
    template <>
    struct ResTypeMapper<Texture> {
        static constexpr auto resType = ResourceStorage::ResType::Texture;
    };

    template <>
    struct ResTypeMapper<TextureArray> {
        static constexpr auto resType = ResourceStorage::ResType::TexArray;
    };

    template <>
    struct ResTypeMapper<TextureCube> {
        static constexpr auto resType = ResourceStorage::ResType::TexCube;
    };

    template <>
    struct ResTypeMapper<RefModel> {
        static constexpr auto resType = ResourceStorage::ResType::RefModel;
    };

    template <>
    struct ResTypeMapper<Skeleton> {
        static constexpr auto resType = ResourceStorage::ResType::Skeleton;
    };

    template <>
    struct ResTypeMapper<std::filesystem::path> {
        static constexpr auto resType = ResourceStorage::ResType::BVHPath;
    };

    template <>
    struct ResTypeMapper<AnimClip> {
        static constexpr auto resType = ResourceStorage::ResType::AnimClip;
    };
}

// load texture with default srv desc from file
Texture& loadTextureAt( ResourceStorage::Slot& texSlot,
    const ResourceStorage::ResID& resID,
    D3D12Device& device, D3D12GfxCmdList& cmdList,
    DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    const std::filesystem::path& path
);

// load texture with custom srv desc from file
Texture& loadTextureAt( ResourceStorage::Slot& texSlot,
    const ResourceStorage::ResID& resID,
    D3D12Device& device, D3D12GfxCmdList& cmdList,
    DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    const std::filesystem::path& path,
    const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
);

// load texture manually with default srv desc, default state, and no clear value
Texture& loadTextureAt( ResourceStorage::Slot& texSlot, const ResourceStorage::ResID& resID,
    D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    const Texture::Desc& resDesc, D3D12_HEAP_TYPE heapType
);

// load texture manually with custom srv desc, default state, and no clear value
Texture& loadTextureAt( ResourceStorage::Slot& texSlot, const ResourceStorage::ResID& resID,
    D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    const Texture::Desc& resDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
    D3D12_HEAP_TYPE heapType
);

// load texture manually with default srv desc, custom state, and no clear value
Texture& loadTextureAt( ResourceStorage::Slot& texSlot, const ResourceStorage::ResID& resID,
    D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    const Texture::Desc& resDesc, D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_STATES initialState
);

// load texture manually with custom srv desc, custom state, and no clear value
Texture& loadTextureAt( ResourceStorage::Slot& texSlot, const ResourceStorage::ResID& resID,
    D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    const Texture::Desc& resDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
    D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState
);

// load texture manually with default srv desc, default state, and clear value
Texture& loadTextureAt( ResourceStorage::Slot& texSlot, const ResourceStorage::ResID& resID,
    D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    const Texture::Desc& resDesc, D3D12_HEAP_TYPE heapType,
    const D3D12_CLEAR_VALUE& optimizedClearValue
);

// load texture manually with default srv desc, custom state, and clear value
Texture& loadTextureAt( ResourceStorage::Slot& texSlot, const ResourceStorage::ResID& resID,
    D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    const Texture::Desc& resDesc, D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE& optimizedClearValue
);

// load texture manually with custom srv desc, default state, and clear value
Texture& loadTextureAt( ResourceStorage::Slot& texSlot, const ResourceStorage::ResID& resID,
    D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    const Texture::Desc& resDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
    D3D12_HEAP_TYPE heapType, const D3D12_CLEAR_VALUE& optimizedClearValue
);

// load texture manually with custom srv desc, custom state, and clear value
Texture& loadTextureAt( ResourceStorage::Slot& texSlot, const ResourceStorage::ResID& resID,
    D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    const Texture::Desc& resDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
    D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
    const D3D12_CLEAR_VALUE& optimizedClearValue
);

RefModel& loadRefModelAt( ResourceStorage::Slot& modelSlot,
    const ResourceStorage::ResID& resID,
    D3D12Device& device, D3D12GfxCmdList& cmdList,
    const std::filesystem::path& geometryPath,
    const ResourceStorage::Slot& texSlot,
    const ResourceStorage::Slot& texArraySlot,
    const ResourceStorage::Slot& texCubeSlot
);

Skeleton& loadSkeletonAt( ResourceStorage::Slot& skeletonSlot,
    const ResourceStorage::ResID& resID,
    const std::filesystem::path& skeletonPath
);

RefModel& loadSkeletalRefModelAt( ResourceStorage::Slot& modelSlot,
    ResourceStorage::Slot& skeletonSlot,
    const ResourceStorage::ResID& modelID,
    const ResourceStorage::ResID& skeletonID,
    D3D12Device& device, D3D12GfxCmdList& cmdList,
    const std::filesystem::path& geometryPath,
    const ResourceStorage::Slot& texSlot,
    const ResourceStorage::Slot& texArraySlot,
    const ResourceStorage::Slot& texCubeSlot,
    const std::filesystem::path& skeletonPath
);

Skeleton& loadSkeletonAndAnimAt( ResourceStorage::Slot& skeletonSlot,
    ResourceStorage::Slot& animSlot,
    const std::filesystem::path& skAnimPath
);

RefModel& loadSkeletalRefModelAndAnimAt( ResourceStorage::Slot& modelSlot,
    ResourceStorage::Slot& skeletonSlot,
    ResourceStorage::Slot& animSlot,
    const ResourceStorage::ResID& modelID,
    const ResourceStorage::ResID& skeletonID,
    D3D12Device& device, D3D12GfxCmdList& cmdList,
    const std::filesystem::path& geometryPath,
    const ResourceStorage::Slot& texSlot,
    const ResourceStorage::Slot& texArraySlot,
    const ResourceStorage::Slot& texCubeSlot,
    const std::filesystem::path& skAnimPath
);

struct ShadowMapInfo {
    Texture* pTex;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
};

// shadow map expects depth format as resource format.
// it makes srv at view(0) and dsv at view(1).
// the ShadowMapInfo's texture pointer will be nullptr,
// the caller should set the pointer with pair's first element's address.
std::pair<Texture, ShadowMapInfo> makeShadowMap(
    const Texture::Desc& shadowMapDesc,
    D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    DescriptorRange<DescriptorHeapCPU>& dsvRange
);

// shadow map expects depth format as resource format.
// it makes srv at view(0) and dsv at view(1).
ShadowMapInfo loadShadowMapAt(
    ResourceStorage::Slot& shadowMapSlot,
    const ResourceStorage::ResID& resID,
    D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
    DescriptorRange<DescriptorHeapCPU>& dsvRange,
    const Texture::Desc& shadowMapDesc
);

}   // namespace gfx::d3d12

}   // namespace gfx

#endif	// __d3d12Resource_HPP