#ifndef __d3d12Resource_HPP
#define __d3d12Resource_HPP

#include "d3d12util/d3d12Low.hpp"
#include "texLoader/DDSTextureLoader12.h"
#include "coord.hpp"

#include "FreeImage.h"

#include "dxutil/dxexcept.hpp"

#include "vertex.hpp"

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

#include "enumUtil.hpp"
#include "memUtil.hpp"

namespace gfx {

namespace d3d12 {

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
        D3D12_HEAP_TYPE heapType
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

class StaticTextureStorage {
public:
    void load( const std::filesystem::path& path, TextureResource::Type type,
        D3D12Device& device, D3D12GfxCmdList& cmdList, DescriptorRange<DescriptorHeapGPU>& range
    );
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
    using ResourceType = TextureResource::Type;

    enum class MapType {
        Albedo,
        Normal,
        Roughness,
        Metallic,
        MetallicSmoothness,
        Emmisive,
        AmbientOcllusion,
        Size
    };

    enum class ConstantType {
        Albedo,
        Roughness,
        Metallic,
        Emmisive,
        AmbientOcllusion,
        AlbedoConstantMapRatio,
        RoughnessConstantMapRatio,
        MetallicConstantMapRatio,
        EmmisiveConstantMapRatio,
        AmbientOcllusionConstantMapRatio,
        Size
    };

    struct MapRef {
        static constexpr std::uint32_t invalid = std::uint32_t(-1);

        std::uint32_t type;
        std::uint32_t resourceIdx;
        std::uint32_t arrayIdx;
        std::uint32_t padding;

        dx::XMUINT4 toxm() const {
            return dx::XMUINT4{ type, resourceIdx, arrayIdx, padding };
        }

        auto operator<=>(const MapRef&) const = default;
    };

    Material();
    void addMapRef(MapType type, const MapRef& mapRef);
    void addTexRes(MapType type, const Texture& tex);
    void addTexRes(MapType type, const TextureArray& tex);
    void addTexRes(MapType type, const TextureCube& tex);
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

private:
	std::bitset<etoi(Vertex::Properties::SIZE)> attribs_;
};

class IndexBuffer : public DefaultBuffer {
public:
    IndexBuffer(D3D12Device& device, D3D12GfxCmdList& cmdList,
        const void* pData, std::size_t indexCnt
    );

	void bind(D3D12GfxCmdList& cmdList) const {
		cmdList.get()->IASetIndexBuffer(&ibview());
	}

    std::size_t size() const noexcept {
        return size_;
    }

private:
    std::size_t size_;
};

struct MaterialMapKey {
    std::string state;
    std::string renderPassID;
};

inline auto operator<=>(const MaterialMapKey& lhs, const MaterialMapKey& rhs) {
    return std::tuple(lhs.state, lhs.renderPassID)
        <=> std::tuple(rhs.state, rhs.renderPassID);
}

class Bitmap {
public:
    Bitmap() = default;

	Bitmap( const std::filesystem::path& path ) {
		load( path );
	}

    Bitmap(const Bitmap&) = delete;
    Bitmap& operator=(const Bitmap&) = delete;
    Bitmap(Bitmap&& other) noexcept;
    Bitmap& operator=(Bitmap&& other) noexcept;

	~Bitmap() {
		unload();
	}

	void load( const std::filesystem::path& path );
    BYTE getGreyscalePixel( size_t x, size_t y ) const;
	void unload();

    std::size_t width() const noexcept {
        return width_;
    }

    std::size_t height() const noexcept {
        return height_;
    }

private:
    FIBITMAP* pBitmap_;
	std::size_t width_;
	std::size_t height_;
	unsigned char* bits_;
};

class RefMesh {
public:
    static constexpr const char* defaultState = "default";

    friend class RefModel;
    friend class Mesh;

    static RefMesh loadGeometryFromFile(D3D12Device& device, D3D12GfxCmdList& cmdList, FILE* pInFile);
    static void loadMaterialsFromFile(D3D12Device& device, D3D12GfxCmdList& cmdList, FILE* pInFile, RefMesh& mesh);

    const auto& vbs() const noexcept {
        return vbs_;
    }

    const auto& ibs() const noexcept {
        return ibs_;
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

    void MU_CALLCONV map(const MaterialMapKey& key,
        Material::ConstantType constantType, mu::Vec4 constant
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

    const auto& materialMap() const noexcept {
        return materialMap_;
    }

    void bind(D3D12GfxCmdList& cmdList, std::size_t ibIdx = 0) const;
    void draw(D3D12GfxCmdList& cmdList, std::size_t instanceCnt, std::size_t ibIdx = 0) const;

    std::string name_;
    std::vector<VertexBuffer> vbs_;
    std::vector<IndexBuffer> ibs_;
    std::map<MaterialMapKey, Material> materialMap_;
    D3D12_PRIMITIVE_TOPOLOGY topology_;
};

class RefModel {
public:
    class Node {
    public:
        friend class RefModel;
        Node(const RefModel* pRefModel = nullptr)
            : coord_(), name_(), meshes_(), children_(), pRefModel_(pRefModel) {}
        ~Node() = default;
        Node(const Node& other);
        Node(Node&& other) noexcept;
        Node& operator=(const Node& other);
        Node& operator=(Node&& other) noexcept;

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

    RefModel(const std::map<Material::MapRef, std::filesystem::path>& pathMap,
        const StaticTextureStorage& sts
    );

    ~RefModel() = default;
    RefModel(const RefModel& other) = delete;
    RefModel(RefModel&& other) noexcept;
    RefModel& operator=(const RefModel& other) = delete;
    RefModel& operator=(RefModel&& other) noexcept;

    static RefModel loadTerrainSubsetFromHeightmap( const Bitmap& heightmap,
        D3D12Device& device, D3D12GfxCmdList& cmdList,
        int xStart, int zStart, int width, int length, mu::Vec3 scale,
        const Material::MapRef& albedoMapRef
    );

    static RefModel loadHierarchyFromFile( const std::filesystem::path& path,
        D3D12Device& device, D3D12GfxCmdList& cmdList, const StaticTextureStorage& sts
    );

    void map(const Material::MapRef& mapRef, const DescriptorGPU& descriptor) {
        textureMap_[mapRef] = descriptor;
    }

    auto& nodes() noexcept { return nodeStorage_; }
    const auto& nodes() const noexcept { return nodeStorage_; }
    Node* root() noexcept { return pRoot_; }
    const Node* root() const noexcept { return pRoot_; }

private:
    static void loadNodesFromFile( D3D12Device& device, D3D12GfxCmdList& cmdList,
        FILE* pInFile, Node& node, RefModel& model
    );

    std::vector<Node> nodeStorage_;
    std::map<Material::MapRef, DescriptorGPU> textureMap_;
    Node* pRoot_;
};

class RefModelStorage {
public:
    using ID = std::string;

    void loadModel( const std::filesystem::path& path, const ID& key,
        const StaticTextureStorage& sts,
        D3D12Device& device, D3D12GfxCmdList& cmdList
    );

    const RefModel& get(const ID& key) const {
        return map_.at(key);
    }

    RefModel& get(const ID& key) {
        return map_.at(key);
    }

    const RefModel& operator[](const ID& key) const {
        return get(key);
    }

    RefModel& operator[](const ID& key) {
        return get(key);
    }

private:
    std::map<ID, RefModel> map_;
};

class Mesh {
public:
    friend class Shader;

    Mesh(const RefMesh& refMesh, const char* initialState)
        : Mesh(refMesh, std::string_view(initialState)) {}

    Mesh(const RefMesh& refMesh, std::string_view initialState)
        : Mesh(refMesh, std::string(initialState)) {}

    Mesh(const RefMesh& refMesh, std::string&& initialState)
        : state_(std::move(initialState)), materialTable_(), pRefMesh_(&refMesh),
        pCurMaterial_(nullptr) {
        for (auto& [key, material] : refMesh.materialMap()) {
            materialTable_[key] = material;
        }
    }

    std::string_view state() const noexcept {
        return state_;
    }

    void setState(const char* state) noexcept {
        state_ = state;
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

    void map(const MaterialMapKey& key, const Material& mat) {
        materialTable_[key] = mat;
    }

    void map(const MaterialMapKey& key, const Material&& mat) {
        materialTable_[key] = std::move(mat);
    }

    Material& material(const std::string& renderPass) {
        return materialTable_.at(MaterialMapKey{ .state = state_, .renderPassID = renderPass });
    }

    const Material& material(const std::string& renderPass) const {
        return materialTable_.at(MaterialMapKey{ .state = state_, .renderPassID = renderPass });
    }

    const RefMesh& refMesh() const noexcept {
        return *pRefMesh_;
    }

private:
    void draw(D3D12GfxCmdList& cmdList, std::size_t instanceCnt) const {
        pRefMesh_->draw(cmdList, instanceCnt);
    }

    std::string state_;
    std::map<MaterialMapKey, Material> materialTable_;
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
        void emplaceMesh(const RefMesh& refMesh, const char* initialState) {
            emplaceMesh(refMesh, std::string_view(initialState));
        }
        void emplaceMesh(const RefMesh& refMesh, std::string&& initialState);
        void emplaceMesh(const RefMesh& refMesh, std::string_view initialState) {
            emplaceMesh(refMesh, std::string(initialState));
        }
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
    Model(const RefModel& ref, const char* initialState)
        : Model(ref, std::string_view(initialState)) {}
    Model(const RefModel& ref, std::string_view initialState)
        : Model(ref, std::string(initialState)) {}
    Model(const RefModel& ref, std::string&& initialState);
    Model(const Model& other);
    Model(Model&& other) noexcept;
    Model& operator=(const Model& other);
    Model& operator=(Model&& other) noexcept;

    auto& nodes() noexcept { return nodeStorage_; }
    const auto& nodes() const noexcept { return nodeStorage_; }

    std::string_view state() const noexcept { return state_; }
    void setState(const char* state) noexcept {
        setState(std::string_view(state));
    }
    void setState(std::string_view state) {
        setState(std::string(state));
    }
    void setState(std::string&& state);

    template <class StrLike>
    void markRenderPass(StrLike&& renderPass) {
        markedRenderPasses_.emplace_back(std::forward<StrLike>(renderPass));
    }

    template <class StrLike>
    bool willDrawOnRenderPass(StrLike&& renderPass) const {
        if (markedRenderPasses_.empty()) {
            return true;
        }
        return std::ranges::find(markedRenderPasses_, renderPass)
            != markedRenderPasses_.end();
    }

private:
    std::vector<Node> nodeStorage_;
    std::vector<std::string> markedRenderPasses_;
    std::string state_;
    Node* pRoot_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif	// __d3d12Resource_HPP