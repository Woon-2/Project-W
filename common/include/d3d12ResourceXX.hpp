#ifndef __d3d12Resource_HPP
#define __d3d12Resource_HPP

#include "d3d12Low.hpp"
#include "texLoader/DDSTextureLoader12.h"
#include "coord.hpp"

#include "dxexcept.hpp"

#include "vertex.hpp"

#include <bitset>
#include <filesystem>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <memory>

#include "enumUtil.hpp"
#include "memUtil.hpp"

namespace gfx {

namespace d3d12 {

class UploadBuffer : public D3D12Resource {
public:
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
    DefaultBuffer(D3D12Device& device, std::size_t byteWidth,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
    );
};

class TextureResource : public D3D12Resource {
public:
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
        D3D12_HEAP_TYPE heapType
    );
    TextureResource( D3D12Device& device, D3D12GfxCmdList& cmdList, 
        const std::filesystem::path& path, UploadBuffer& upBuf
    );

private:
    struct LoadDDSReturnType {
        wrl::ComPtr<ID3D12Resource> res;
        std::unique_ptr<std::uint8_t[]> ddsData;
        std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    };

    static LoadDDSReturnType loadDDS(
        D3D12Device& device, D3D12GfxCmdList& cmdList,
        const std::filesystem::path& path, UploadBuffer& upBuf
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
    ) : TextureResource(device, convertDesc(texResDesc), heapType) {
        makeDefSrv(device, tex2dRange.alloc());
    }

    Texture( D3D12Device& device, DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const Desc& texResDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        D3D12_HEAP_TYPE heapType
    ) : TextureResource(device, convertDesc(texResDesc), heapType) {
        makeSrv(srvDesc, device, tex2dRange.alloc());
    }

    Texture(D3D12Device& device, D3D12GfxCmdList& cmdList,
        DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const std::filesystem::path& path, UploadBuffer& upBuf
    ) : TextureResource(device, cmdList, path, upBuf) {
        makeDefSrv(device, tex2dRange.alloc());
    }

    Texture(D3D12Device& device, D3D12GfxCmdList& cmdList,
        DescriptorRange<DescriptorHeapGPU>& tex2dRange,
        const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        const std::filesystem::path& path, UploadBuffer& upBuf
    ) : TextureResource(device, cmdList, path, upBuf) {
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
        const std::filesystem::path& path, UploadBuffer& upBuf
    ) : TextureResource(device, cmdList, path, upBuf) {
        makeDefSrv(device, tex2dArrRange.alloc());
    }

    TextureArray(D3D12Device& device, D3D12GfxCmdList& cmdList,
        DescriptorRange<DescriptorHeapGPU>& tex2dArrRange,
        const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        const std::filesystem::path& path, UploadBuffer& upBuf
    ) : TextureResource(device, cmdList, path, upBuf) {
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
        const std::filesystem::path& path, UploadBuffer& upBuf
    ) : TextureResource(device, cmdList, path, upBuf) {
        makeDefSrv(device, texCubeRange.alloc());
    }

    TextureCube(D3D12Device& device, D3D12GfxCmdList& cmdList,
        DescriptorRange<DescriptorHeapGPU>& texCubeRange,
        const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        const std::filesystem::path& path, UploadBuffer& upBuf
    ) : TextureResource(device, cmdList, path, upBuf) {
        makeSrv(srvDesc, device, texCubeRange.alloc());
    }

    std::size_t srvOffsetInRange() {
        return view(idxSrv).offset();
    }
};

class StaticTextureStorage {
public:
    void load(const std::filesystem::path& path);
    const DescriptorGPU& get(const std::filesystem::path& path) const;
    DescriptorGPU& get(const std::filesystem::path& path);
    const DescriptorGPU& operator[](const std::filesystem::path& path) const;
    DescriptorGPU& operator[](const std::filesystem::path& path);
    bool contains(const std::filesystem::path& path) const;

private:
    std::map<std::filesystem::path, DescriptorGPU> map_;
    std::vector<Texture> storedTexs_;
    std::vector<TextureArray> storedTexArrs_;
    std::vector<TextureCube> storedTexCubes_;
};

class Material {
public:
    enum class ResourceType {
        Texture,
        TextureArray,
        TextureCube
    };

    enum class MapType {
        Diffuse,
        Normal,
        Emmissive,
        Size
    };

    enum class ConstantType {
        Shininess,
        SpecularColor,
        Size
    };

    struct MapRef {
        static constexpr std::uint32_t invalid = std::uint32_t(-1);

        std::uint32_t type;
        std::uint32_t resourceIdx;
        std::uint32_t arrayIdx;
        std::uint32_t padding;

        auto operator<=>(const MapRef&) const = default;
    };

    Material();
    void addMapRef(MapType type, const MapRef& mapRef);
    void addTexRes(MapType type, const Texture& tex);
    void addTexRes(MapType type, const TextureArray& tex);
    void addTexRes(MapType type, const TextureCube& tex);
    void MU_CALLCONV addConstant(ConstantType type, mu::Vec3 constant);
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

private:
    std::vector<MapRef> mapRefs_;
    std::vector<RawMemory<16>> constants_;
};

class VertexBuffer : public DefaultBuffer {
public:
    VertexBuffer(D3D12Device& device, D3D12GfxCmdList& cmdList,
        const void* pData, std::size_t byteWidth, std::size_t stride,
		std::bitset<etoi(Vertex::Properties::SIZE)> attribs
    );

    VertexBuffer(D3D12Device& device, D3D12GfxCmdList& cmdList,
        const gfx::VertexBuffer& vertexBuffer
    ) : VertexBuffer(device, cmdList, vertexBuffer.rawMem(), vertexBuffer.byteWidth(),
        vertexBuffer.stride(), vertexBuffer.propFlag()
    ) {}

    void completeInit() {
        upBuf_.get()->Release();
    }

    template <std::ranges::range R>
		requires std::same_as<std::ranges::range_value_t<R>, VertexBuffer>
    static void bind(D3D12GfxCmdList& cmdList, std::size_t slot, const R& vbs) {
		auto views = std::vector<D3D12_VERTEX_BUFFER_VIEW>(vbs.size());
		std::ranges::transform(vbs, views.begin(), [](const auto& vb) {
			return vb.vbview_;
		});
        DX_THROW_FAILED_VOID( cmdList.get()->IASetVertexBuffers(
            static_cast<UINT>(slot), static_cast<UINT>( views.size() ), views.data()
        ) );
	}

	const auto& attributes() const noexcept { return attribs_; }

private:
	UploadBuffer upBuf_;
	D3D12_VERTEX_BUFFER_VIEW vbview_;
	std::bitset<etoi(Vertex::Properties::SIZE)> attribs_;
};

class IndexBuffer : public DefaultBuffer {
public:
    IndexBuffer(D3D12Device& device, D3D12GfxCmdList& cmdList,
        const void* pData, std::size_t indexCnt, std::size_t byteWidth
    );

    void completeInit() {
		upBuf_.get()->Release();
    }

	void bind(D3D12GfxCmdList& cmdList) const {
		cmdList.get()->IASetIndexBuffer(&ibview_);
	}

    std::size_t size() const noexcept {
        return size_;
    }

private:
    UploadBuffer upBuf_;
	D3D12_INDEX_BUFFER_VIEW ibview_;
    std::size_t size_;
};

struct MaterialMapKey {
    std::string state;
    std::string renderPassID;
};

auto operator<=>(const MaterialMapKey& lhs, const MaterialMapKey& rhs) {
    return std::tuple(lhs.state, lhs.renderPassID)
        <=> std::tuple(rhs.state, rhs.renderPassID);
}

class RefMesh {
public:
    friend class RefModel;
    friend class Mesh;

    const auto& vbs() const noexcept {
        return vbs_;
    }

    const auto& ib() const noexcept {
        return ib_;
    }

    D3D12_PRIMITIVE_TOPOLOGY topology() const noexcept {
        return topology_;
    }

    void map( const MaterialMapKey& key, Material::MapType mapType,
        const Material::MapRef& mapRef
    ) {
        materialMap_[key].addMapRef(mapType, mapRef);
    }

    void map(const MaterialMapKey& key, Material::ConstantType constantType,
        const float constant
    ) {
        materialMap_[key].addConstant(constantType, constant);
    }

    void MU_CALLCONV map(const MaterialMapKey& key,
        Material::ConstantType constantType, mu::Vec3 constant
    ) {
        materialMap_[key].addConstant(constantType, constant);
    }

    bool mapped(const MaterialMapKey& key) const {
        return materialMap_.contains(key);
    }

    const Material::MapRef& mapRef(const MaterialMapKey& key, Material::MapType type) const {
        return materialMap_.at(key).mapRef(type);
    }

    const RawMemory<16>& constant(const MaterialMapKey& key, Material::ConstantType type) const {
        return materialMap_.at(key).constant(type);
    }

    template <class T>
    const T& constant(const MaterialMapKey& key, Material::ConstantType type) const {
        return materialMap_.at(key).constant<T>(type);
    }

private:
    auto& materialMap() noexcept {
        return materialMap_;
    }

    void bind(D3D12GfxCmdList& cmdList) const;
    void draw(D3D12GfxCmdList& cmdList, std::size_t instanceCnt) const;

    std::vector<VertexBuffer> vbs_;
    IndexBuffer ib_;
    std::map<MaterialMapKey, Material> materialMap_;
    D3D12_PRIMITIVE_TOPOLOGY topology_;
};

class RefModel {
public:
    class Node {
    public:
        friend class RefModel;
        Node(const RefModel* pRefModel = nullptr)
            : coord_(), meshes_(), children_(), pRefModel_(pRefModel) {}
        void addMesh(RefMesh&& mesh);
        void addChild(Node* child);
        auto& coord() noexcept { return coord_; }
        const auto& coord() const noexcept { return coord_; }

    private:
        gfx::coord::System coord_;
        std::vector<RefMesh> meshes_;
        std::vector<Node*> children_;
        const RefModel* pRefModel_;
    };

    RefModel(const std::map<Material::MapRef, std::filesystem::path>& pathMap,
        const StaticTextureStorage& sts
    );

    RefModel(const RefModel& other) = delete;
    RefModel(RefModel&& other) noexcept;
    RefModel& operator=(const RefModel& other) = delete;
    RefModel& operator=(RefModel&& other) noexcept;

    void map(const Material::MapRef& mapRef, const DescriptorGPU& descriptor) {
        textureMap_[mapRef] = descriptor;
    }

    auto& nodes() noexcept { return nodeStorage_; }
    const auto& nodes() const noexcept { return nodeStorage_; }

private:
    std::vector<Node> nodeStorage_;
    std::map<Material::MapRef, DescriptorGPU> textureMap_;
    Node* pRoot_;
};

class Mesh {
public:
    friend class Shader;

    Mesh(RefMesh& refMesh, std::string_view initialState)
        : state_(initialState), materialTable_(), pRefMesh_(&refMesh),
        pCurMaterial_(nullptr) {
        for (auto& [key, material] : refMesh.materialMap()) {
            materialTable_[key] = &material;
        }
    }

    std::string_view state() const noexcept {
        return state_;
    }

    void setState(std::string&& state) noexcept {
        state_ = std::move(state);
    }

    void setState(std::string_view state) noexcept {
        state_ = state;
    }

    bool hasMapped(const MaterialMapKey& key) const {
        return materialTable_.contains(key);
    }

    void map(const MaterialMapKey& key, Material* pMaterial) {
        materialTable_[key] = pMaterial;
    }

    Material& material(const std::string& renderPass) {
        return *materialTable_.at(MaterialMapKey{ .state = state_, .renderPassID = renderPass });
    }

    const Material& material(const std::string& renderPass) const {
        return *materialTable_.at(MaterialMapKey{ .state = state_, .renderPassID = renderPass });
    }

    const RefMesh& refMesh() const noexcept {
        return *pRefMesh_;
    }

private:
    void bind(D3D12GfxCmdList& cmdList) const {
        pRefMesh_->bind(cmdList);
    }

    void draw(D3D12GfxCmdList& cmdList, std::size_t instanceCnt) const {
        pRefMesh_->draw(cmdList, instanceCnt);
    }

    std::string state_;
    std::map<MaterialMapKey, Material*> materialTable_;
    const RefMesh* pRefMesh_;
    Material* pCurMaterial_;
};

class Model {
public:
    class Node {
    public:
        friend class Model;
        Node(const Model* pModel = nullptr)
            : coord_(), meshes_(), children_(), pModel_(pModel) {}
        void addMesh(Mesh&& mesh);
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

    auto& nodes() noexcept { return nodeStorage_; }
    const auto& nodes() const noexcept { return nodeStorage_; }

    std::string_view state() const noexcept { return state_; }
    void setState(std::string_view state) { state_ = state; }
    void setState(std::string&& state) { state_ = std::move(state); }

private:
    std::vector<Node> nodeStorage_;
    std::string state_;
    Node* pRoot_;
};


// =============================================================

class PhongMaterial {
public:
    struct ShaderLayout {
        Material::MapRef diffuseMapRef;
        Material::MapRef normalMapRef;
        Material::MapRef emmisiveMapRef;
        dx::XMFLOAT3 specularColor;
        float shininess;
    };

    PhongMaterial(const Material& source);

    const ShaderLayout& get() const noexcept {
        return shaderLayout_;
    }

private:
    ShaderLayout shaderLayout_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif	// __d3d12Resource_HPP