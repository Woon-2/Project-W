#include "d3d12util/d3d12ResourceXX.hpp"

#include "d3d12util/d3d12RenderPass.hpp"

#include "resourcePath.hpp"

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
    const void* pData, DXGI_FORMAT indexFormat, std::size_t indexCnt
) : DefaultBuffer(device, indexCnt * indexByteWidth(indexFormat)), size_(indexCnt),
    indexFormat_(indexFormat) {

    auto upBufIdx = cmdList.emplaceXResource<UploadBuffer>(
        device, pData, indexCnt * indexByteWidth(indexFormat)
    );
    auto& upBuf = cmdList.getXResource<UploadBuffer>(upBufIdx);
	cmdList.copyResource(upBuf, *this);

    makeIbv(device, indexFormat, indexCnt);

	commitState(cmdList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
}

std::size_t IndexBuffer::indexByteWidth(DXGI_FORMAT indexFormat) {
    switch (indexFormat) {
    case DXGI_FORMAT_R16_UINT:
        return 2u;

    case DXGI_FORMAT_R32_UINT:
        return 4u;

    default:
        throw std::runtime_error("Invalid index format");
    }
}

Bitmap::Bitmap(Bitmap&& other) noexcept
    : pBitmap_(std::exchange(other.pBitmap_, nullptr)),
    width_(std::exchange(other.width_, 0)),
    height_(std::exchange(other.height_, 0)),
    bits_(std::exchange(other.bits_, nullptr)) {}

Bitmap& Bitmap::operator=(Bitmap&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    pBitmap_ = std::exchange(other.pBitmap_, nullptr);
    width_ = std::exchange(other.width_, 0);
    height_ = std::exchange(other.height_, 0);
    bits_ = std::exchange(other.bits_, nullptr);

    return *this;
}

void Bitmap::load( const std::filesystem::path& path ) {
    auto cstrFileName = path.string().c_str();

    FREE_IMAGE_FORMAT format = FreeImage_GetFileType( cstrFileName );

    if ( format == -1 )
    {
        std::cerr << "Could not find image: \"" << path << "\"\n";
        return;
    }
    else if ( format == FIF_UNKNOWN )
    {
        std::cerr << "Couldn't determine file format - attempting to get from file extension...\n";
        format = FreeImage_GetFIFFromFilename( cstrFileName );

        if ( !FreeImage_FIFSupportsReading( format ) )
        {
            std::cerr << "Detected image format cannot be read!\n";
            return;
        }
    }

    FIBITMAP* bitmap = FreeImage_Load( format, cstrFileName );
    int bits_per_pixel = FreeImage_GetBPP( bitmap );

    if ( bits_per_pixel == 32 )
    {
        pBitmap_ = bitmap;
    }
    else
    {
        pBitmap_ = FreeImage_ConvertTo32Bits( bitmap );
    }

    width_ = FreeImage_GetWidth( pBitmap_ );
    height_ = FreeImage_GetHeight( pBitmap_ );
    bits_ = FreeImage_GetBits( pBitmap_ );
}

BYTE Bitmap::getGreyscalePixel( size_t x, size_t y ) const {
    BYTE ret{};
    if ( !FreeImage_GetPixelIndex( pBitmap_, static_cast<unsigned int>(x), 
        static_cast<unsigned int>(y), &ret 
    ) ) {
        std::cerr << "Failed to get pixel index\n";
    }
    return ret;
}

void Bitmap::unload() {
    if ( pBitmap_ )
    {
        FreeImage_Unload( pBitmap_ );
        pBitmap_ = nullptr;
        width_ = 0;
        height_ = 0;
        bits_ = nullptr;
    }
}

void RefSubmesh::draw(D3D12GfxCmdList& cmdList, std::size_t instanceCnt) const {
    VertexBuffer::bind(cmdList, 0u, parent_->vbs());
    ib_.bind(cmdList);
    cmdList.get()->IASetPrimitiveTopology(topology_);
    DX_THROW_FAILED_VOID(
        cmdList.get()->DrawIndexedInstanced(
            static_cast<UINT>( ib_.size() ),
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
	int nPositions = 0, nColors = 0, nNormals = 0, nTangents = 0, nBiTangents = 0, nTextureCoords = 0, nSubMeshes = 0, nIndices = 0, nSubmeshIndex = 0, nSubIndices = 0;

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

		if (!strcmp(pstrToken, "<Bounds:>"))
		{
            dx::XMFLOAT3 aabbCenter, aabbExtents;
			nReads = (UINT)::fread(&aabbCenter, sizeof(dx::XMFLOAT3), 1, pInFile);
			nReads = (UINT)::fread(&aabbExtents, sizeof(dx::XMFLOAT3), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<Positions:>"))
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
		else if (!strcmp(pstrToken, "<Colors:>"))
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
		else if (!strcmp(pstrToken, "<TextureCoords0:>"))
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
		else if (!strcmp(pstrToken, "<TextureCoords1:>"))
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
		else if (!strcmp(pstrToken, "<Normals:>"))
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
		else if (!strcmp(pstrToken, "<Tangents:>"))
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
		else if (!strcmp(pstrToken, "<BiTangents:>"))
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
		else if (!strcmp(pstrToken, "<Submeshes:>")) {
            nReads = (UINT)::fread(&nSubMeshes, sizeof(int), 1, pInFile);
            nReads = (UINT)::fread(&nIndices, sizeof(int), 1, pInFile);
            ret.submeshes_.reserve(nSubMeshes);

            for (int i = 0; i < nSubMeshes; ++i) {

                nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
                nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
                pstrToken[nStrLength] = '\0';
                
                if (strcmp(pstrToken, "<Submesh:>")) {
                    fclose(pInFile);
                    throw std::runtime_error("Submesh token expected but got " + std::string(pstrToken));
                }

                nReads = (UINT)::fread(&nSubmeshIndex, sizeof(int), 1, pInFile);
                
                ret.submeshes_.emplace_back(&ret);
                auto& submesh = ret.submeshes_.back();
                
                // enable more topology types later
                // nReads = (UINT)::fread(&submesh.topology_, sizeof(int), 1, pInFile);
                submesh.topology_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                nReads = (UINT)::fread(&nSubIndices, sizeof(int), 1, pInFile);
                if (nIndices < 65536) {
                    auto indices = std::vector<std::uint16_t>(nSubIndices);
                    nReads = (UINT)::fread(indices.data(), sizeof(std::uint16_t), nSubIndices, pInFile);
                    submesh.ib_ = IndexBuffer(device, cmdList, indices.data(), DXGI_FORMAT_R16_UINT, nSubIndices);
                }
                else {
                    auto indices = std::vector<std::uint32_t>(nSubIndices);
                    nReads = (UINT)::fread(indices.data(), sizeof(std::uint32_t), nSubIndices, pInFile);
                    submesh.ib_ = IndexBuffer(device, cmdList, indices.data(), DXGI_FORMAT_R32_UINT, nSubIndices);
                }
            }
        }
		else if (!strcmp(pstrToken, "</Mesh>")) {
			break;
		}
	}

    return ret;
}

void RefMesh::loadMaterialsFromFile( D3D12Device& device, D3D12GfxCmdList& cmdList,
    FILE* pInFile, RefMesh& mesh
) {
    char pstrToken[64] = { '\0' };
	BYTE nStrLength = 0;

	UINT nReads{};

    int materialCnt = 0;
    nReads = (UINT)::fread(&materialCnt, sizeof(int), 1, pInFile);

    if (materialCnt != mesh.submeshes_.size()) {
        fclose(pInFile);
        throw std::runtime_error("Material count mismatch");
    }

	for ( ; ; )
	{
        nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
        nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
		pstrToken[nStrLength] = '\0';
        if (!strcmp(pstrToken, "</Materials>")) {
            break;
        }
        else if (!strcmp(pstrToken, "<Material:>")) {
            int materialIdx = 0;
            nReads = (UINT)::fread(&materialIdx, sizeof(int), 1, pInFile);
            loadMaterialFromFile(device, cmdList, pInFile, mesh, materialIdx);
        }
        else {
            fclose(pInFile);
            throw std::runtime_error("expected material token or materials end token but got " + std::string(pstrToken));
        }
	}
}

void RefMesh::loadMaterialFromFile( D3D12Device& device, D3D12GfxCmdList& cmdList,
    FILE* pInFile, RefMesh& mesh, std::size_t materialIdx
) {
    char pstrToken[64] = { '\0' };

	BYTE nStrLength = 0;

	UINT nReads{};

    auto floatVal = float{};
    auto float4 = dx::XMFLOAT4{};
    Material::MapRef mapRef{};

    auto& submesh = mesh.submeshes_[materialIdx];

    for (;;) {

        if (!strcmp(pstrToken, "</Material>")) {
            break;
        }
        else if (!strcmp(pstrToken, "<AlbedoColor:>"))
        {
            nReads = (UINT)::fread(&float4, sizeof(dx::XMFLOAT4), 1, pInFile);
            submesh.material().addConstant( Material::ConstantType::Albedo, mu::Vec4(float4.x, float4.y, float4.z, float4.w) );
        }
        else if (!strcmp(pstrToken, "<EmissiveColor:>"))
        {
            nReads = (UINT)::fread(&float4, sizeof(dx::XMFLOAT4), 1, pInFile);
            submesh.material().addConstant( Material::ConstantType::Emmisive, mu::Vec3(float4.x, float4.y, float4.z) );
        }
        else if (!strcmp(pstrToken, "<AmbientOcllusion:>"))
        {
            nReads = (UINT)::fread(&floatVal, sizeof(float), 1, pInFile);
            submesh.material().addConstant( Material::ConstantType::AmbientOcllusion, floatVal );
        }
        else if (!strcmp(pstrToken, "<Smoothness:>"))
        {
            nReads = (UINT)::fread(&floatVal, sizeof(float), 1, pInFile);
            submesh.material().addConstant( Material::ConstantType::Roughness, 1.f - floatVal );
        }
        else if (!strcmp(pstrToken, "<Metallic:>"))
        {
            nReads = (UINT)::fread(&floatVal, sizeof(float), 1, pInFile);
            submesh.material().addConstant( Material::ConstantType::Metallic, floatVal );
        }
        else if (!strcmp(pstrToken, "<AlbedoMap:>"))
        {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            submesh.material().addMapRef( Material::MapType::Albedo, mapRef );
        }
        else if (!strcmp(pstrToken, "<NormalMap:>"))
        {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            submesh.material().addMapRef( Material::MapType::Normal, mapRef );
        }
        else if (!strcmp(pstrToken, "<MetallicMap:>"))
        {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            submesh.material().addMapRef( Material::MapType::Metallic, mapRef );
        }
        else if (!strcmp(pstrToken, "<MetallicSmoothnessMap:>"))
        {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            submesh.material().addMapRef( Material::MapType::MetallicSmoothness, mapRef );
        }
        else if (!strcmp(pstrToken, "<EmissionMap:>"))
        {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            submesh.material().addMapRef( Material::MapType::Emmisive, mapRef );
        }
        else {
            fclose(pInFile);
            throw std::runtime_error("expected material token or material end token but got " + std::string(pstrToken));
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
    for (auto& submesh : mesh.submeshes_) {
        for (auto& mapRef : submesh.material().mapRefs()) {
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

RefModel RefModel::loadTerrainSubsetFromHeightmap( const Bitmap& heightmap,
    D3D12Device& device, D3D12GfxCmdList& cmdList,
    int xStart, int zStart, int width, int length, mu::Vec3 scale,
    const Material::MapRef& albedoMapRef
) {
    auto model = RefModel();

    model.nodeStorage_.emplace_back(&model);
    auto& node = model.nodeStorage_.back();
    model.pRoot_ = &node;

    auto positions = std::vector<dx::XMFLOAT3>(width * length);
    auto normals = std::vector<dx::XMFLOAT3>(width * length);
    auto uvs = std::vector<dx::XMFLOAT2>(width * length);

    auto scaleXmf = scale.getXmf();

    for (int i = 0, z = zStart; z < (zStart + length); ++z) {
        for (int x = xStart; x < (xStart + width); ++x, ++i) {
            // Calculate position
            auto height = heightmap.getGreyscalePixel(x, z) / 255.f * scaleXmf.y;

            auto position = dx::XMFLOAT3(x * scaleXmf.x,
                height, z * scaleXmf.z
            );

            positions.push_back(position);

            // Calculate normal
            auto iWidth = static_cast<int>(heightmap.width());
            auto iLength = static_cast<int>(heightmap.height());

            auto nHeightMapIndex = x + (z * iWidth);
            auto xHeightMapAdd = x < iWidth - 1 ? 1 : -1;
            auto zHeightMapAdd = z < iLength - 1 ? iWidth : -iWidth;

            float y1 = heightmap.getGreyscalePixel(x, z) / 255.f * scaleXmf.y;
            float y2 = heightmap.getGreyscalePixel(x + xHeightMapAdd, z) / 255.f * scaleXmf.y;
            float y3 = heightmap.getGreyscalePixel(x, z + zHeightMapAdd) / 255.f * scaleXmf.y;

            auto edge1 = mu::Vec3(0.f, y3 - y1, scaleXmf.z);
            auto edge2 = mu::Vec3(scaleXmf.x, y2 - y1, 0.f);
            auto normal = mu::cross(edge1, edge2);

            normals.push_back(normal.getXmf());

            // Calculate UV
            auto uv = dx::XMFLOAT2(static_cast<float>(x) / iWidth, static_cast<float>(z) / iLength);
            uvs.push_back(uv);
        }
    }

    auto mesh = RefMesh();

    mesh.vbs_.emplace_back(device, cmdList, positions.data(),
        sizeof(dx::XMFLOAT3) * positions.size(), sizeof(dx::XMFLOAT3),
        std::bitset<etoi(Vertex::Properties::SIZE)>{ 1ull << etoi(Vertex::Properties::Position3D) }
    );

    mesh.vbs_.emplace_back(device, cmdList, normals.data(),
        sizeof(dx::XMFLOAT3) * normals.size(), sizeof(dx::XMFLOAT3),
        std::bitset<etoi(Vertex::Properties::SIZE)>{ 1ull << etoi(Vertex::Properties::Normal3D) }
    );

    mesh.vbs_.emplace_back(device, cmdList, uvs.data(),
        sizeof(dx::XMFLOAT2) * uvs.size(), sizeof(dx::XMFLOAT2),
        std::bitset<etoi(Vertex::Properties::SIZE)>{ 1ull << etoi(Vertex::Properties::TexCoord2D0) }
    );

    mesh.submeshes_.emplace_back(&mesh, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    auto& submesh = mesh.submeshes_.back();


    auto makeIb = [&](auto&& indices) {
        using IndexType = std::ranges::range_value_t<decltype(indices)>;
        indices.reserve(((width * 2) * (length - 1)) + ((length - 1) - 1));

        auto out = std::back_inserter(indices);

        for (int i = 0, z = 0; z < length - 1; ++z) {
            if (!(z % 2)) {
                for (int x = 0; x < width; ++x) {
                    if (x == 0 && z > 0) {
                        out = static_cast<IndexType>(x + (z * width));
                    }
                    out = static_cast<IndexType>(x + (z * width));
                    out = static_cast<IndexType>(x + ((z + 1) * width));
                }
            }
            else {
                for (int x = width - 1; x >= 0; --x) {
                    if (x == (width - 1)) {
                        out = static_cast<IndexType>(x + (z * width));
                    }
                    out = static_cast<IndexType>(x + (z * width));
                    out = static_cast<IndexType>(x + ((z + 1) * width));
                }
            }
        }

        const auto format = sizeof(IndexType) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        submesh.ib_ = IndexBuffer(device, cmdList, indices.data(), format, indices.size());
    };

    const auto indexCnt = ((width * 2) * (length - 1)) + ((length - 1) - 1);

    if (indexCnt < 65536) {
        makeIb(std::vector<std::uint16_t>{});
    }
    else {
        makeIb(std::vector<int>{});
    }

    submesh.material().addMapRef( Material::MapType::Albedo, albedoMapRef );

    node.addMesh(std::move(mesh));

    return model;
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

    if (strcmp(pstrToken, "<Dictionary:>")) {
        std::fclose(pInFile);
        throw std::runtime_error("Invalid file format: " + path.string());    
    }

    for ( ; ; ) {
        nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
        nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
        pstrToken[nStrLength] = '\0';

        if (!strcmp(pstrToken, "<Item:>")) {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);

            nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
            auto texRelativePath = std::string(nStrLength, '\0');
            nReads = (UINT)::fread(texRelativePath.data(), sizeof(char), nStrLength, pInFile);

            auto texPath = resourcePath / std::move(texRelativePath);

            if (!sts.contains(texPath)) {
                std::fclose(pInFile);
                throw std::runtime_error("Texture not found: " + texPath.string());
            }
            model.textureMap_[mapRef] = sts.get(texPath);
        }
        else if (!strcmp(pstrToken, "</Dictionary>")) {
            break;
        }
        else {
            std::fclose(pInFile);
            throw std::runtime_error("expected Item or Dictionary end token but got: " + std::string(pstrToken));
        }
    }

    nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
    nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<NodesInfo:>")) {
        std::fclose(pInFile);
        throw std::runtime_error("expected NodesInfo token but got: " + std::string(pstrToken));
    }

    nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
    nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<NodeCnt:>")) {
        std::fclose(pInFile);
        throw std::runtime_error("expected NodeCnt token but got: " + std::string(pstrToken));
    }

    int nNodes = 0;
    nReads = (UINT)::fread(&nNodes, sizeof(int), 1, pInFile);
    model.nodeStorage_.reserve(nNodes);

    for (;;) {
        nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
        nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
        pstrToken[nStrLength] = '\0';

        if (!strcmp(pstrToken, "<Node:>")) {
            model.nodeStorage_.emplace_back(&model);
            auto& node = model.nodeStorage_.back();
            loadNodesFromFile(device, cmdList, pInFile, node, model);
            model.pRoot_ = &node;
        }
        else if (strcmp(pstrToken, "</NodesInfo>")) {
            break;
        }
        else {
            std::fclose(pInFile);
            throw std::runtime_error("expected Node or NodesInfo end token but got: " + std::string(pstrToken));
        }

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

    nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
    auto nodeName = std::string(nStrLength, '\0');
	nReads = (UINT)::fread(nodeName.data(), sizeof(char), nStrLength, pInFile);

    for ( ; ; )
	{
		nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
		nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
		pstrToken[nStrLength] = '\0';

		if (!strcmp(pstrToken, "<Xform:>"))
		{
            nReads = (UINT)::fread(&xform, sizeof(float), 16, pInFile);
            node.coord_.setLocalXform(DirectX::XMLoadFloat4x4(&xform));
        }
        else if (!strcmp(pstrToken, "<Mesh:>"))
		{
			node.addMesh( RefMesh::loadGeometryFromFile(device, cmdList, pInFile) );
            
            nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
            nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
            pstrToken[nStrLength] = '\0';

            if (!strcmp(pstrToken, "<Materials:>")) {
                node.meshes_.back().loadMaterialsFromFile(device, cmdList, pInFile, node.meshes_.back());
            }
		}
		else if (!strcmp(pstrToken, "<Children:>"))
		{
			int nChilds = 0;
			nReads = (UINT)::fread(&nChilds, sizeof(int), 1, pInFile);
			if (nChilds > 0)
			{
				for (int i = 0; i < nChilds; ++i)
				{
                    model.nodeStorage_.emplace_back(&model);
                    auto& child = model.nodeStorage_.back();

                    nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
                    nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
                    pstrToken[nStrLength] = '\0';

                    if (strcmp(pstrToken, "<Node:>")) {
                        fclose(pInFile);
                        throw std::runtime_error("Node token expected but got: " + std::string(pstrToken));
                    }

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

void RefModelStorage::loadModel( const std::filesystem::path& path,
    const ID& key, const StaticTextureStorage& sts, D3D12Device& device,
    D3D12GfxCmdList& cmdList
) {
    map_[key] = RefModel::loadHierarchyFromFile(path, device, cmdList, sts);
}

Submesh::Submesh(Mesh* parent, const RefSubmesh* pRefSubmesh)
    : parent_(parent), pRefSubmesh_(pRefSubmesh), material_() {
    if (pRefSubmesh_) {
        material_ = pRefSubmesh_->material();
    }
}

Mesh::Mesh(const RefMesh& refMesh)
    : pRefMesh_(&refMesh), submeshes_() {
    submeshes_.reserve(refMesh.submeshes().size());
    for (const auto& refSubmesh : refMesh.submeshes()) {
        submeshes_.emplace_back(this, &refSubmesh);
    }
}

void Model::Node::addMesh(Mesh&& mesh) {
    meshes_.push_back(std::move(mesh));
}

void Model::Node::emplaceMesh(const RefMesh& refMesh) {
    meshes_.emplace_back(refMesh);
}

void Model::Node::addChild(Node* child) {
    child->pModel_ = pModel_;
    child->coord_.setParent(&coord_);
    children_.push_back(child);
}

Model::Model(const RefModel& ref)
    : nodeStorage_(ref.nodes().size()), pRoot_(nullptr) {
    auto pRefFirstNode = ref.nodes().data();

    pRoot_ = nodeStorage_.data() + (ref.root() - pRefFirstNode);

    for (std::size_t i = 0; i < ref.nodes().size(); ++i) {
        auto& node = ref.nodes()[i];
        auto clone = Node(this);
        for (const auto& mesh : node.meshes()) {
            clone.emplaceMesh(mesh);
        }
        clone.coord_.setLocalXform(node.coord().localXform());
        for (auto pChild : node.children()) {
            clone.addChild(nodeStorage_.data() + (pChild - pRefFirstNode));
        }
        nodeStorage_[i] = std::move(clone);
    }
}

Model::Model(const Model& other)
    : nodeStorage_(other.nodeStorage_.size()), pRoot_(nullptr) {
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
    : nodeStorage_(other.nodeStorage_.size()), pRoot_(nullptr) {
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