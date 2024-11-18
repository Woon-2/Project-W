#include "d3d12util/d3d12ResourceXX.hpp"

namespace gfx {

namespace d3d12 {

TextureResource::Desc Texture::convertDesc(const Desc& desc) {
    return TextureResource::Desc{
        .width = desc.width,
        .height = desc.height,
        .arraySize = 1u,
        .mipLevels = desc.mipLevels,
        .format = desc.format,
        .sampleDesc = desc.sampleDesc,
        .flags = desc.flags
    };
}

UploadBuffer::UploadBuffer( D3D12Device& device, const void* data,
    std::size_t byteWidth, std::size_t srcOffset, std::size_t destOffset,
    D3D12_RESOURCE_FLAGS flags
) : UploadBuffer(device, byteWidth, flags) {
    stage(data, byteWidth, srcOffset, destOffset);
}

UploadBuffer::UploadBuffer( D3D12Device& device, const void* data,
    std::size_t byteWidth, D3D12_RESOURCE_FLAGS flags
) : UploadBuffer(device, data, byteWidth, 0u, 0u, flags) {}

UploadBuffer::UploadBuffer( D3D12Device& device, std::size_t byteWidth,
    D3D12_RESOURCE_FLAGS flags
) : D3D12Resource( device, D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Width = static_cast<UINT64>(byteWidth),
        .Height = static_cast<UINT>(1),
        .DepthOrArraySize = static_cast<UINT16>(1),
        .MipLevels = static_cast<UINT16>(1),
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = DXGI_SAMPLE_DESC{
            .Count = 1,
            .Quality = 0
        },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = flags
    }, D3D12_HEAP_TYPE_UPLOAD ) {
    get()->Map(0, nullptr, reinterpret_cast<void**>(&pMappedData_));
}

void UploadBuffer::stage(const void* data, std::size_t byteWidth,
    std::size_t srcOffset, std::size_t destOffset
) {
    std::memcpy(pMappedData_ + destOffset,
        reinterpret_cast<const std::uint8_t*>(data) + srcOffset,
        byteWidth
    );
}

DefaultBuffer::DefaultBuffer( D3D12Device& device, std::size_t byteWidth,
    D3D12_RESOURCE_FLAGS flags
) : D3D12Resource( device, D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Width = static_cast<UINT64>(byteWidth),
        .Height = static_cast<UINT>(1),
        .DepthOrArraySize = static_cast<UINT16>(1),
        .MipLevels = static_cast<UINT16>(1),
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = DXGI_SAMPLE_DESC{
            .Count = 1,
            .Quality = 0
        },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = flags
    }, D3D12_HEAP_TYPE_DEFAULT ) {}

TextureResource::TextureResource( D3D12Device& device, const Desc& texResDesc,
    D3D12_HEAP_TYPE heapType
) : D3D12Resource(device, D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = texResDesc.width,
        .Height = texResDesc.height,
        .DepthOrArraySize = texResDesc.arraySize,
        .MipLevels = texResDesc.mipLevels,
        .Format = texResDesc.format,
        .SampleDesc = texResDesc.sampleDesc,
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = texResDesc.flags
    }, heapType), data_(), subresources_() {}

TextureResource::TextureResource( D3D12Device& device,
    D3D12GfxCmdList& cmdList, const std::filesystem::path& path,
    UploadBuffer& upBuf
) : D3D12Resource(), data_(), subresources_() {
    auto loadedData = loadDDS(device, cmdList, path, upBuf);
    init(std::move(loadedData.res));
    data_ = std::move(loadedData.ddsData);
    subresources_ = std::move(loadedData.subresources);
}

TextureResource::LoadDDSReturnType TextureResource::loadDDS(
    D3D12Device& device, D3D12GfxCmdList& cmdList,
    const std::filesystem::path& path, UploadBuffer& upBuf
) {
    auto ret = LoadDDSReturnType{};

    auto alphaMode = DirectX::DDS_ALPHA_MODE_UNKNOWN;
    auto blsCubemap = false;

    DirectX::LoadDDSTextureFromFileEx(
        device.get().Get(),
        path.wstring().c_str(),
        0,
        D3D12_RESOURCE_FLAG_NONE,
        DirectX::DDS_LOADER_DEFAULT,
        &ret.res,
        ret.ddsData,
        ret.subresources,
        &alphaMode,
        &blsCubemap
    );

    auto requiredBytes = GetRequiredIntermediateSize(ret.res.Get(), 0, static_cast<UINT>(ret.subresources.size()));

    UpdateSubresources(cmdList.get().Get(), ret.res.Get(), upBuf.get().Get(),
        0, 0, static_cast<UINT>(ret.subresources.size()), ret.subresources.data()
    );

    return ret;
}

Material::Material()
    : mapRefs_(etoi(MapType::Size), MapRef{ MapRef::invalid, MapRef::invalid, MapRef::invalid, 0 }),
    constants_(etoi(ConstantType::Size), RawMemory<16>{}) {}

void Material::addMapRef(MapType type, const MapRef& mapRef) {
    mapRefs_[etoi(type)] = mapRef;
}

void Material::addTexRes(MapType type, const Texture& tex) {
    addMapRef( type, MapRef{
        .type = etoi(ResourceType::Texture),
        .resourceIdx = static_cast<std::uint32_t>(tex.view(Texture::idxSrv).offset()),
        .arrayIdx = 0
    } );
}

void Material::addTexRes(MapType type, const TextureArray& tex) {
    addMapRef( type, MapRef{
        .type = etoi(ResourceType::TextureArray),
        .resourceIdx = static_cast<std::uint32_t>(tex.view(TextureArray::idxSrv).offset()),
        .arrayIdx = 0
    } );
}

void Material::addTexRes(MapType type, const TextureCube& tex) {
    addMapRef( type, MapRef{
        .type = etoi(ResourceType::TextureCube),
        .resourceIdx = static_cast<std::uint32_t>(tex.view(TextureCube::idxSrv).offset()),
        .arrayIdx = 0
    } );
}

void MU_CALLCONV Material::addConstant(ConstantType type, mu::Vec3 constant) {
    RawMemory<16> tmp{};
    *reinterpret_cast<dx::XMFLOAT3*>(&tmp) = constant.getXmf();
    constants_[etoi(type)] = tmp;
}

void Material::addConstant(ConstantType type, float constant) {
    RawMemory<16> tmp{};
    *reinterpret_cast<float*>(&tmp) = constant;
    constants_[etoi(type)] = tmp;
}

VertexBuffer::VertexBuffer( D3D12Device& device, D3D12GfxCmdList& cmdList,
    const void* pData, std::size_t byteWidth, std::size_t stride,
	std::bitset<etoi(Vertex::Properties::SIZE)> attribs
) : DefaultBuffer(device, byteWidth), upBuf_(device, pData, byteWidth),
    vbview_{ .BufferLocation = get()->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(byteWidth),
	    .StrideInBytes = static_cast<UINT>(stride)
    }, attribs_(attribs) {
    cmdList.copyResource(upBuf_, *this);
	commitState(cmdList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}

IndexBuffer::IndexBuffer(D3D12Device& device, D3D12GfxCmdList& cmdList,
    const void* pData, std::size_t indexCnt, std::size_t byteWidth
) : DefaultBuffer(device, byteWidth), upBuf_(device, pData, byteWidth),
    ibview_{ .BufferLocation = get()->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>(byteWidth),
		.Format = DXGI_FORMAT_R16_UINT
    }, size_(indexCnt) {
	cmdList.copyResource(upBuf_, *this);
	commitState(cmdList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
}

void RefMesh::bind(D3D12GfxCmdList& cmdList) const {
    VertexBuffer::bind(cmdList, 0u, vbs_);
    ib_.bind(cmdList);
    cmdList.get()->IASetPrimitiveTopology(topology_);
}

void RefMesh::draw(D3D12GfxCmdList& cmdList, std::size_t instanceCnt) const {
    bind(cmdList);
    DX_THROW_FAILED_VOID(
        cmdList.get()->DrawIndexedInstanced(
            static_cast<UINT>( ib_.size() ),
            static_cast<UINT>( instanceCnt ),
            0u, 0, 0u
        )
    );
}

RefModel::Node::Node(const Node& other)
    : coord_(other.coord_), meshes_(other.meshes_), children_(),
    pRefModel_(other.pRefModel_) {}

RefModel::Node::Node(Node&& other) noexcept
    : coord_(std::move(other.coord_)), meshes_(std::move(other.meshes_)),
    children_(std::move(other.children_)),
    pRefModel_(std::exchange(other.pRefModel_, nullptr)) {}

RefModel::Node& RefModel::Node::operator=(const Node& other) {
    if (this == &other) {
        return *this;
    }

    coord_ = other.coord_;
    meshes_ = other.meshes_;
    children_.clear();
    pRefModel_ = other.pRefModel_;

    return *this;
}

RefModel::Node& RefModel::Node::operator=(Node&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    coord_ = std::move(other.coord_);
    meshes_ = std::move(other.meshes_);
    children_ = std::move(other.children_);
    pRefModel_ = std::exchange(other.pRefModel_, nullptr);

    return *this;
}

void RefModel::Node::addMesh(RefMesh&& mesh) {
    for (auto& [key, material] : mesh.materialMap()) {
        for (auto& mapRef : material.mapRefs()) {
            if (mapRef.resourceIdx != Material::MapRef::invalid) {
                mapRef.resourceIdx = static_cast<std::uint32_t>(
                    pRefModel_->textureMap_.at(mapRef).offset()
                );
            }
        }
    }
    meshes_.push_back(std::move(mesh));
}

void RefModel::Node::addChild(Node* child) {
    child->pRefModel_ = pRefModel_;
    child->coord_.setParent(&coord_);
    children_.push_back(child);
}

RefModel::RefModel(const std::map<Material::MapRef, std::filesystem::path>& pathMap,
    const StaticTextureStorage& sts
) : nodeStorage_(), textureMap_(), pRoot_(nullptr) {
    for (auto& [mapRef, path] : pathMap) {
        assert(sts.contains(path));
        map(mapRef, sts.get(path));
    }
}

RefModel::RefModel(RefModel&& other) noexcept
    : nodeStorage_(other.nodeStorage_.size()),
    textureMap_(std::move(other.textureMap_)), pRoot_(nullptr) {
    auto pOtherFirstNode = other.nodeStorage_.data();

    pRoot_ = nodeStorage_.data() + (other.pRoot_ - pOtherFirstNode);

    for (std::size_t i = 0; i < other.nodeStorage_.size(); ++i) {
        auto& node = other.nodeStorage_[i];
        auto clone = Node(this);
        clone.meshes_ = std::move(node.meshes_);
        clone.coord_.setLocalXform(node.coord_.localXform());
        for (auto pChild : node.children_) {
            clone.addChild(nodeStorage_.data() + (pChild - pOtherFirstNode));
        }
        nodeStorage_[i] = std::move(clone);

        node.children_.clear();
        node.pRefModel_ = nullptr;
    }
}

RefModel& RefModel::operator=(RefModel&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    nodeStorage_.resize(other.nodeStorage_.size());
    textureMap_ = std::move(other.textureMap_);

    auto pOtherFirstNode = other.nodeStorage_.data();

    pRoot_ = nodeStorage_.data() + (other.pRoot_ - pOtherFirstNode);

    for (std::size_t i = 0; i < other.nodeStorage_.size(); ++i) {
        auto& node = other.nodeStorage_[i];
        auto clone = Node(this);
        clone.meshes_ = std::move(node.meshes_);
        clone.coord_.setLocalXform(node.coord_.localXform());
        for (auto pChild : node.children_) {
            clone.addChild(nodeStorage_.data() + (pChild - pOtherFirstNode));
        }
        nodeStorage_[i] = std::move(clone);

        node.children_.clear();
        node.pRefModel_ = nullptr;
    }

    return *this;
}

void Model::Node::addMesh(Mesh&& mesh) {
    meshes_.push_back(std::move(mesh));
}

void Model::Node::emplaceMesh(const RefMesh& refMesh, std::string&& initialState) {
    meshes_.emplace_back(refMesh, std::move(initialState));
}

void Model::Node::addChild(Node* child) {
    child->pModel_ = pModel_;
    child->coord_.setParent(&coord_);
    children_.push_back(child);
}

Model::Model(const RefModel& ref, std::string&& initialState)
    : nodeStorage_(ref.nodes().size()), state_(std::move(initialState)),
    pRoot_(nullptr) {
    auto pRefFirstNode = ref.nodes().data();

    pRoot_ = nodeStorage_.data() + (ref.root() - pRefFirstNode);

    for (std::size_t i = 0; i < ref.nodes().size(); ++i) {
        auto& node = ref.nodes()[i];
        auto clone = Node(this);
        for (const auto& mesh : node.meshes()) {
            clone.emplaceMesh(mesh, initialState);
        }
        clone.coord_.setLocalXform(node.coord().localXform());
        for (auto pChild : node.children()) {
            clone.addChild(nodeStorage_.data() + (pChild - pRefFirstNode));
        }
        nodeStorage_[i] = std::move(clone);
    }
}

Model::Model(const Model& other)
    : nodeStorage_(other.nodeStorage_.size()),
    state_(), pRoot_(nullptr) {
    auto pOtherFirstNode = other.nodeStorage_.data();

    pRoot_ = nodeStorage_.data() + (other.pRoot_ - pOtherFirstNode);

    for (std::size_t i = 0; i < other.nodeStorage_.size(); ++i) {
        auto& node = other.nodeStorage_[i];
        auto clone = Node(this);
        clone.meshes_ = node.meshes_;
        clone.coord_.setLocalXform(node.coord_.localXform());
        for (auto pChild : node.children_) {
            clone.addChild(nodeStorage_.data() + (pChild - pOtherFirstNode));
        }
        nodeStorage_[i] = std::move(clone);
    }
}

Model& Model::operator=(const Model& other) {
    if (this == &other) {
        return *this;
    }

    nodeStorage_.resize(other.nodeStorage_.size());
    state_ = other.state_;

    auto pOtherFirstNode = other.nodeStorage_.data();

    pRoot_ = nodeStorage_.data() + (other.pRoot_ - pOtherFirstNode);

    for (std::size_t i = 0; i < other.nodeStorage_.size(); ++i) {
        auto& node = other.nodeStorage_[i];
        auto clone = Node(this);
        clone.meshes_ = node.meshes_;
        clone.coord_.setLocalXform(node.coord_.localXform());
        for (auto pChild : node.children_) {
            clone.addChild(nodeStorage_.data() + (pChild - pOtherFirstNode));
        }
        nodeStorage_[i] = std::move(clone);
    }

    return *this;
}

Model::Model(Model&& other) noexcept
    : nodeStorage_(other.nodeStorage_.size()),
    state_(std::move(other.state_)), pRoot_(nullptr) {
    auto pOtherFirstNode = other.nodeStorage_.data();

    pRoot_ = nodeStorage_.data() + (other.pRoot_ - pOtherFirstNode);

    for (std::size_t i = 0; i < other.nodeStorage_.size(); ++i) {
        auto& node = other.nodeStorage_[i];
        auto clone = Node(this);
        clone.meshes_ = std::move(node.meshes_);
        clone.coord_.setLocalXform(node.coord_.localXform());
        for (auto pChild : node.children_) {
            clone.addChild(nodeStorage_.data() + (pChild - pOtherFirstNode));
        }
        nodeStorage_[i] = std::move(clone);

        node.children_.clear();
        node.pModel_ = nullptr;
    }
}

Model& Model::operator=(Model&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    nodeStorage_.resize(other.nodeStorage_.size());

    auto pOtherFirstNode = other.nodeStorage_.data();

    pRoot_ = nodeStorage_.data() + (other.pRoot_ - pOtherFirstNode);
    state_ = std::move(other.state_);

    for (std::size_t i = 0; i < other.nodeStorage_.size(); ++i) {
        auto& node = other.nodeStorage_[i];
        auto clone = Node(this);
        clone.meshes_ = std::move(node.meshes_);
        clone.coord_.setLocalXform(node.coord_.localXform());
        for (auto pChild : node.children_) {
            clone.addChild(nodeStorage_.data() + (pChild - pOtherFirstNode));
        }
        nodeStorage_[i] = std::move(clone);

        node.children_.clear();
        node.pModel_ = nullptr;
    }

    return *this;
}

}   // namespace gfx::d3d12

}   // namespace gfx