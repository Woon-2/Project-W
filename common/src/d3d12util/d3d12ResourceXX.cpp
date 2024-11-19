#include "d3d12util/d3d12ResourceXX.hpp"

#include <cstdio>

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
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
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
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
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
    D3D12GfxCmdList& cmdList, const std::filesystem::path& path
) : D3D12Resource(), data_(), subresources_() {
    auto loadedData = loadDDS(device, cmdList, path);
    init(std::move(loadedData.res));
    data_ = std::move(loadedData.ddsData);
    subresources_ = std::move(loadedData.subresources);
}

TextureResource::LoadDDSReturnType TextureResource::loadDDS(
    D3D12Device& device, D3D12GfxCmdList& cmdList,
    const std::filesystem::path& path
) {
    auto ret = LoadDDSReturnType{};

    auto alphaMode = DirectX::DDS_ALPHA_MODE_UNKNOWN;
    auto blsCubemap = false;

    DX_THROW_FAILED( DirectX::LoadDDSTextureFromFileEx(
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
    ) );

    auto requiredBytes = GetRequiredIntermediateSize(ret.res.Get(), 0, static_cast<UINT>(ret.subresources.size()));

    auto upBufIdx = cmdList.emplaceXResource<UploadBuffer>(device, requiredBytes);
    auto& upBuf = cmdList.getXResource<UploadBuffer>(upBufIdx);

    UpdateSubresources(cmdList.get().Get(), ret.res.Get(), upBuf.get().Get(),
        0, 0, static_cast<UINT>(ret.subresources.size()), ret.subresources.data()
    );

    return ret;
}

void StaticTextureStorage::load( const std::filesystem::path& path,
    TextureResource::Type type, D3D12Device& device, D3D12GfxCmdList& cmdList,
    DescriptorRange<DescriptorHeapGPU>& range
) {
    switch(type) {
    case TextureResource::Type::Texture:
        storedTexs_.emplace_back(device, cmdList, range, path);
        map_[path] = storedTexs_.back().view(Texture::idxSrv);
        break;

    case TextureResource::Type::TextureArray:
        storedTexArrs_.emplace_back(device, cmdList, range, path);
        map_[path] = storedTexArrs_.back().view(TextureArray::idxSrv);
        break;

    case TextureResource::Type::TextureCube:
        storedTexCubes_.emplace_back(device, cmdList, range, path);
        map_[path] = storedTexCubes_.back().view(TextureCube::idxSrv);
        break;
    }
}

const DescriptorGPU& StaticTextureStorage::get(const std::filesystem::path& path) const {
    return map_.at(path);
}

DescriptorGPU& StaticTextureStorage::get(const std::filesystem::path& path) {
    return map_.at(path);
}

const DescriptorGPU& StaticTextureStorage::operator[](const std::filesystem::path& path) const {
    return map_.at(path);
}

DescriptorGPU& StaticTextureStorage::operator[](const std::filesystem::path& path) {
    return map_[path];
}

bool StaticTextureStorage::contains(const std::filesystem::path& path) const {
    return map_.contains(path);
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

void MU_CALLCONV Material::addConstant(ConstantType type, mu::Vec4 constant) {
    RawMemory<16> tmp{};
    *reinterpret_cast<dx::XMFLOAT4*>(&tmp) = constant.getXmf();
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
) : DefaultBuffer(device, byteWidth), attribs_(attribs) {
    auto upBufIdx = cmdList.emplaceXResource<UploadBuffer>(device, pData, byteWidth);
    auto& upBuf = cmdList.getXResource<UploadBuffer>(upBufIdx);
    cmdList.copyResource(upBuf, *this);

    setStride(stride);
    makeDefVbv(device);

	commitState(cmdList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}

IndexBuffer::IndexBuffer(D3D12Device& device, D3D12GfxCmdList& cmdList,
    const void* pData, std::size_t indexCnt
) : DefaultBuffer(device, indexCnt * sizeof(std::uint16_t)), size_(indexCnt) {
    auto upBufIdx = cmdList.emplaceXResource<UploadBuffer>(
        device, pData, indexCnt * sizeof(std::uint16_t)
    );
    auto& upBuf = cmdList.getXResource<UploadBuffer>(upBufIdx);
	cmdList.copyResource(upBuf, *this);

    makeDefIbv(device);

	commitState(cmdList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
}

void RefMesh::draw(D3D12GfxCmdList& cmdList, std::size_t instanceCnt, std::size_t ibIdx) const {
    VertexBuffer::bind(cmdList, 0u, vbs_);
    ibs_[ibIdx].bind(cmdList);
    cmdList.get()->IASetPrimitiveTopology(topology_);
    DX_THROW_FAILED_VOID(
        cmdList.get()->DrawIndexedInstanced(
            static_cast<UINT>( ibs_[ibIdx].size() ),
            static_cast<UINT>( instanceCnt ),
            0u, 0, 0u
        )
    );
}

RefMesh RefMesh::loadGeometryFromFile(D3D12Device& device, D3D12GfxCmdList& cmdList, FILE* pInFile) {
    auto ret = RefMesh{};
    
    char pstrToken[64] = { '\0' };
	BYTE nStrLength = 0;

    int nVertices = 0;
	int nPositions = 0, nColors = 0, nNormals = 0, nTangents = 0, nBiTangents = 0, nTextureCoords = 0, nIndices = 0, nSubMeshes = 0, nSubIndices = 0;

	UINT nReads = (UINT)::fread(&nVertices, sizeof(int), 1, pInFile);

	nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
    auto meshName = std::string(nStrLength, '\0');
	nReads = (UINT)::fread(meshName.data(), sizeof(char), nStrLength, pInFile);
    ret.name_ = std::move(meshName);

	for ( ; ; )
	{
		nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
		nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
		pstrToken[nStrLength] = '\0';

		if (!strcmp(pstrToken, "<Bounds>:"))
		{
            dx::XMFLOAT3 aabbCenter, aabbExtents;
			nReads = (UINT)::fread(&aabbCenter, sizeof(dx::XMFLOAT3), 1, pInFile);
			nReads = (UINT)::fread(&aabbExtents, sizeof(dx::XMFLOAT3), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<Positions>:"))
		{
			nReads = (UINT)::fread(&nPositions, sizeof(int), 1, pInFile);
			if (nPositions > 0)
			{
				auto positions = std::vector<dx::XMFLOAT3>(nPositions);
				nReads = (UINT)::fread(positions.data(), sizeof(dx::XMFLOAT3), nPositions, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::Position3D));
                ret.vbs_.emplace_back( device, cmdList, positions.data(),
                    sizeof(dx::XMFLOAT3) * nPositions, sizeof(dx::XMFLOAT3), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<Colors>:"))
		{
			nReads = (UINT)::fread(&nColors, sizeof(int), 1, pInFile);
			if (nColors > 0)
			{
                auto colors = std::vector<dx::XMFLOAT4>(nColors);
                nReads = (UINT)::fread(colors.data(), sizeof(dx::XMFLOAT4), nColors, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::Color4D));
                ret.vbs_.emplace_back( device, cmdList, colors.data(),
                    sizeof(dx::XMFLOAT4) * nColors, sizeof(dx::XMFLOAT4), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<TextureCoords0>:"))
		{
			nReads = (UINT)::fread(&nTextureCoords, sizeof(int), 1, pInFile);
			if (nTextureCoords > 0)
			{
                auto texCoords = std::vector<dx::XMFLOAT2>(nTextureCoords);
                nReads = (UINT)::fread(texCoords.data(), sizeof(dx::XMFLOAT2), nTextureCoords, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::TexCoord2D0));
                ret.vbs_.emplace_back( device, cmdList, texCoords.data(),
                    sizeof(dx::XMFLOAT2) * nTextureCoords, sizeof(dx::XMFLOAT2), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<TextureCoords1>:"))
		{
			nReads = (UINT)::fread(&nTextureCoords, sizeof(int), 1, pInFile);
			if (nTextureCoords > 0)
			{
                auto texCoords = std::vector<dx::XMFLOAT2>(nTextureCoords);
                nReads = (UINT)::fread(texCoords.data(), sizeof(dx::XMFLOAT2), nTextureCoords, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::TexCoord2D1));
                ret.vbs_.emplace_back( device, cmdList, texCoords.data(),
                    sizeof(dx::XMFLOAT2) * nTextureCoords, sizeof(dx::XMFLOAT2), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<Normals>:"))
		{
			nReads = (UINT)::fread(&nNormals, sizeof(int), 1, pInFile);
			if (nNormals > 0)
			{
                auto normals = std::vector<dx::XMFLOAT3>(nNormals);
                nReads = (UINT)::fread(normals.data(), sizeof(dx::XMFLOAT3), nNormals, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::Normal3D));
                ret.vbs_.emplace_back( device, cmdList, normals.data(),
                    sizeof(dx::XMFLOAT3) * nNormals, sizeof(dx::XMFLOAT3), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<Tangents>:"))
		{
			nReads = (UINT)::fread(&nTangents, sizeof(int), 1, pInFile);
			if (nTangents > 0)
			{
                auto tangents = std::vector<dx::XMFLOAT3>(nTangents);
                nReads = (UINT)::fread(tangents.data(), sizeof(dx::XMFLOAT3), nTangents, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::Tangent3D));
                ret.vbs_.emplace_back( device, cmdList, tangents.data(),
                    sizeof(dx::XMFLOAT3) * nTangents, sizeof(dx::XMFLOAT3), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<BiTangents>:"))
		{
			nReads = (UINT)::fread(&nBiTangents, sizeof(int), 1, pInFile);
			if (nBiTangents > 0)
			{
                auto biTangents = std::vector<dx::XMFLOAT3>(nBiTangents);
                nReads = (UINT)::fread(biTangents.data(), sizeof(dx::XMFLOAT3), nBiTangents, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::Bitangent3D));
                ret.vbs_.emplace_back( device, cmdList, biTangents.data(),
                    sizeof(dx::XMFLOAT3) * nBiTangents, sizeof(dx::XMFLOAT3), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<Indices>:")) {
            nReads = (UINT)::fread(&nIndices, sizeof(int), 1, pInFile);
            auto indices = std::vector<std::uint16_t>(nIndices);
            nReads = (UINT)::fread(indices.data(), sizeof(std::uint16_t), nIndices, pInFile);
            ret.ibs_.emplace_back( device, cmdList, indices.data(), nIndices );
        }
		else if (!strcmp(pstrToken, "</Mesh>"))
		{
			break;
		}
	}

    return ret;
}

void RefMesh::loadMaterialsFromFile( D3D12Device& device, D3D12GfxCmdList& cmdList,
    FILE* pInFile, RefMesh& mesh
) {
    char pstrToken[64] = { '\0' };

	int nMaterial = 0;
	BYTE nStrLength = 0;

	UINT nReads{};

    auto floatVal = float{};
    auto float4 = dx::XMFLOAT4{};
    auto float3 = dx::XMFLOAT3{};
    Material::MapRef mapRef{};

	for ( ; ; )
	{
        nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
        nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
		pstrToken[nStrLength] = '\0';
        if (!strcmp(pstrToken, "</Materials>")) {
            break;
        }

        nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
        auto stateName = std::string(nStrLength, '\0');
	    nReads = (UINT)::fread(stateName.data(), sizeof(char), nStrLength, pInFile);

        nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
        auto renderPassName = std::string(nStrLength, '\0');
	    nReads = (UINT)::fread(renderPassName.data(), sizeof(char), nStrLength, pInFile);


		nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
		nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile); 
		pstrToken[nStrLength] = '\0';

		if (!strcmp(pstrToken, "<AlbedoColor>:"))
		{
			nReads = (UINT)::fread(&float4, sizeof(dx::XMFLOAT4), 1, pInFile);
            mesh.map( MaterialMapKey{ stateName, renderPassName }, 
                Material::ConstantType::Albedo, mu::Vec4(float4.x, float4.y, float4.z, float4.w)
            );
		}
		else if (!strcmp(pstrToken, "<EmissiveColor>:"))
		{
			nReads = (UINT)::fread(&float3, sizeof(dx::XMFLOAT3), 1, pInFile);
            mesh.map( MaterialMapKey{ stateName, renderPassName }, 
                Material::ConstantType::Emmisive, mu::Vec3(float3.x, float3.y, float3.z)
            );
		}
		else if (!strcmp(pstrToken, "<AmbientOcllusion>:"))
		{
			nReads = (UINT)::fread(&floatVal, sizeof(float), 1, pInFile);
            mesh.map( MaterialMapKey{ stateName, renderPassName }, 
                Material::ConstantType::AmbientOcllusion, floatVal
            );
		}
		else if (!strcmp(pstrToken, "<Smoothness>:"))
		{
			nReads = (UINT)::fread(&floatVal, sizeof(float), 1, pInFile);
            mesh.map( MaterialMapKey{ stateName, renderPassName }, 
                Material::ConstantType::Roughness, 1.f - floatVal
            );
		}
		else if (!strcmp(pstrToken, "<Metallic>:"))
		{
			nReads = (UINT)::fread(&float3, sizeof(dx::XMFLOAT3), 1, pInFile);
            mesh.map( MaterialMapKey{ stateName, renderPassName }, 
                Material::ConstantType::Emmisive, mu::Vec3(float3.x, float3.y, float3.z)
            );
		}
		else if (!strcmp(pstrToken, "<AlbedoMap>:"))
		{
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            mesh.map( MaterialMapKey{ stateName, renderPassName }, 
                Material::MapType::Albedo, mapRef
            );
		}
		else if (!strcmp(pstrToken, "<NormalMap>:"))
		{
			nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            mesh.map( MaterialMapKey{ stateName, renderPassName }, 
                Material::MapType::Normal, mapRef
            );
		}
		else if (!strcmp(pstrToken, "<MetallicMap>:"))
		{
			nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            mesh.map( MaterialMapKey{ stateName, renderPassName }, 
                Material::MapType::Metallic, mapRef
            );
		}
        else if (!strcmp(pstrToken, "<MetallicSmoothnessMap>:"))
		{
			nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            mesh.map( MaterialMapKey{ stateName, renderPassName }, 
                Material::MapType::MetallicSmoothness, mapRef
            );
		}
		else if (!strcmp(pstrToken, "<EmissionMap>:"))
		{
			nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            mesh.map( MaterialMapKey{ stateName, renderPassName }, 
                Material::MapType::Emmisive, mapRef
            );
		}
	}
}

RefModel::Node::Node(const Node& other)
    : coord_(other.coord_), name_(other.name_), meshes_(other.meshes_),
    children_(), pRefModel_(other.pRefModel_) {}

RefModel::Node::Node(Node&& other) noexcept
    : coord_(std::move(other.coord_)), name_(std::move(other.name_)),
    meshes_(std::move(other.meshes_)), children_(std::move(other.children_)),
    pRefModel_(std::exchange(other.pRefModel_, nullptr)) {}

RefModel::Node& RefModel::Node::operator=(const Node& other) {
    if (this == &other) {
        return *this;
    }

    coord_ = other.coord_;
    name_ = other.name_;
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
    name_ = std::move(other.name_);
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

RefModel RefModel::loadHierarchyFromFile( const std::filesystem::path& path,
    D3D12Device& device, D3D12GfxCmdList& cmdList, const StaticTextureStorage& sts
) {
    auto model = RefModel();

    auto pInFile = std::fopen(path.string().c_str(), "rb");
    if (!pInFile) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    char pstrToken[64] = { '\0' };

	BYTE nStrLength = 0;
	UINT nReads = 0;

    Material::MapRef mapRef{};


    nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
    nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<Dictionary>:")) {
        std::fclose(pInFile);
        throw std::runtime_error("Invalid file format: " + path.string());    
    }

    for ( ; ; ) {
        nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
        nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
        pstrToken[nStrLength] = '\0';

        if (!strcmp(pstrToken, "<Item>")) {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);

            nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
            auto texPath = std::string(nStrLength, '\0');
            nReads = (UINT)::fread(texPath.data(), sizeof(char), nStrLength, pInFile);

            if (!sts.contains(std::filesystem::path(texPath))) {
                std::fclose(pInFile);
                throw std::runtime_error("Texture not found: " + texPath);
            }
            model.textureMap_[mapRef] = sts.get(std::filesystem::path(texPath));

            nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
            nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
            pstrToken[nStrLength] = '\0';

            if (strcmp(pstrToken, "</Item>")) {
                std::fclose(pInFile);
                throw std::runtime_error("Invalid file format: " + path.string());
            }
        }
        else if (!strcmp(pstrToken, "</Dictionary>")) {
            break;
        }
    }

    nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
    nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<NodesInfo>:")) {
        std::fclose(pInFile);
        throw std::runtime_error("Invalid file format: " + path.string());
    }

    nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
    nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<NodeCnt>:")) {
        std::fclose(pInFile);
        throw std::runtime_error("Invalid file format: " + path.string());
    }

    int nNodes = 0;
    nReads = (UINT)::fread(&nNodes, sizeof(int), 1, pInFile);
    model.nodeStorage_.reserve(nNodes);

    nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
    nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
    pstrToken[nStrLength] = '\0';

    if (!strcmp(pstrToken, "<Node>:")) {
        model.nodeStorage_.emplace_back(&model);
        auto& node = model.nodeStorage_.back();
        loadNodesFromFile(device, cmdList, pInFile, node, model);
        model.pRoot_ = &node;
    }

    std::fclose(pInFile);
    return model;
}

void RefModel::loadNodesFromFile( D3D12Device& device, D3D12GfxCmdList& cmdList,
    FILE* pInFile, Node& node, RefModel& model
) {
    char pstrToken[64] = { '\0' };

	BYTE nStrLength = 0;
	UINT nReads = 0;

    dx::XMFLOAT4X4 xform{};

	int nFrame = 0, nTextures = 0;

    auto nodeName = std::string(nStrLength, '\0');
	nReads = (UINT)::fread(nodeName.data(), sizeof(char), nStrLength, pInFile);

    for ( ; ; )
	{
		nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
		nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
		pstrToken[nStrLength] = '\0';

		if (!strcmp(pstrToken, "<Xform>:"))
		{
            nReads = (UINT)::fread(&xform, sizeof(float), 16, pInFile);
            node.coord_.setLocalXform(DirectX::XMLoadFloat4x4(&xform));
        }
        else if (!strcmp(pstrToken, "<Mesh>:"))
		{
			node.addMesh( RefMesh::loadGeometryFromFile(device, cmdList, pInFile) );
            
            nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
            nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
            pstrToken[nStrLength] = '\0';

            if (!strcmp(pstrToken, "<Materials>:")) {
                node.meshes_.back().loadMaterialsFromFile(device, cmdList, pInFile, node.meshes_.back());
            }
		}
		else if (!strcmp(pstrToken, "<Children>:"))
		{
			int nChilds = 0;
			nReads = (UINT)::fread(&nChilds, sizeof(int), 1, pInFile);
			if (nChilds > 0)
			{
				for (int i = 0; i < nChilds; ++i)
				{
                    model.nodeStorage_.emplace_back(&model);
                    auto& child = model.nodeStorage_.back();
                    loadNodesFromFile(device, cmdList, pInFile, child, model);
                    node.addChild(&child);
				}
			}
		}
		else if (!strcmp(pstrToken, "</Node>"))
		{
			break;
		}
    }
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

void Model::setState(std::string&& state) {
    state_ = std::move(state);

    for (auto& node : nodeStorage_) {
        for (auto& mesh : node.meshes_) {
            mesh.setState(state_);
        }
    }
}

}   // namespace gfx::d3d12

}   // namespace gfx