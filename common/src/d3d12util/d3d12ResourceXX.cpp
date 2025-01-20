#include "d3d12util/d3d12ResourceXX.hpp"

#include "d3d12util/d3d12RenderPass.hpp"

#include "resourcePath.hpp"

#include <cstdio>
#include <array>
#include <ranges>
#include <algorithm>

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
    D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState
) : D3D12Resource( device, D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = texResDesc.width,
        .Height = texResDesc.height,
        .DepthOrArraySize = texResDesc.arraySize,
        .MipLevels = texResDesc.mipLevels,
        .Format = texResDesc.format,
        .SampleDesc = texResDesc.sampleDesc,
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = texResDesc.flags
    }, heapType, initialState, nullptr ),
    data_(), subresources_() {}

TextureResource::TextureResource( D3D12Device& device, const Desc& texResDesc,
    D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
    const D3D12_CLEAR_VALUE& optimizedClearValue
) : D3D12Resource( device, D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = texResDesc.width,
        .Height = texResDesc.height,
        .DepthOrArraySize = texResDesc.arraySize,
        .MipLevels = texResDesc.mipLevels,
        .Format = texResDesc.format,
        .SampleDesc = texResDesc.sampleDesc,
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = texResDesc.flags
    }, heapType, initialState, &optimizedClearValue ),
    data_(), subresources_() {}

TextureResource::TextureResource( D3D12Device& device, const Desc& texResDesc,
    D3D12_HEAP_TYPE heapType
) : TextureResource( device, texResDesc, heapType,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
    ) {}

TextureResource::TextureResource( D3D12Device& device, const Desc& texResDesc,
    D3D12_HEAP_TYPE heapType, const D3D12_CLEAR_VALUE& optimizedClearValue
) : TextureResource( device, texResDesc, heapType,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, optimizedClearValue
    ) {}

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
        .arrayIdx = 0,
        .colorSpace = etoi(ColorSpace::SRGB)
    } );
}

void Material::addTexRes(MapType type, const TextureArray& tex) {
    addMapRef( type, MapRef{
        .type = etoi(ResourceType::TextureArray),
        .resourceIdx = static_cast<std::uint32_t>(tex.view(TextureArray::idxSrv).offset()),
        .arrayIdx = 0,
        .colorSpace = etoi(ColorSpace::SRGB)
    } );
}

void Material::addTexRes(MapType type, const TextureCube& tex) {
    addMapRef( type, MapRef{
        .type = etoi(ResourceType::TextureCube),
        .resourceIdx = static_cast<std::uint32_t>(tex.view(TextureCube::idxSrv).offset()),
        .arrayIdx = 0,
        .colorSpace = etoi(ColorSpace::SRGB)
    } );
}

void MU_CALLCONV Material::addConstant(ConstantType type, mu::Vec2 constant) {
    RawMemory<16> tmp{};
    *reinterpret_cast<dx::XMFLOAT2*>(&tmp) = constant.getXmf();
    constants_[etoi(type)] = tmp;
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
    std::vector<std::uint8_t>&& pData, std::size_t byteWidth, std::size_t stride,
	std::bitset<etoi(Vertex::Properties::SIZE)> attribs
) : DefaultBuffer(device, byteWidth), cpuMem_(std::move(pData)), attribs_(attribs) {
    auto upBufIdx = cmdList.emplaceXResource<UploadBuffer>(device, cpuMem_.data(), byteWidth);
    auto& upBuf = cmdList.getXResource<UploadBuffer>(upBufIdx);
    cmdList.copyResource(upBuf, *this);

    setStride(stride);
    makeDefVbv(device);

	commitState(cmdList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}

IndexBuffer::IndexBuffer(D3D12Device& device, D3D12GfxCmdList& cmdList,
    std::vector<std::uint8_t>&& pData, DXGI_FORMAT indexFormat, std::size_t indexCnt
) : DefaultBuffer(device, indexCnt * indexByteWidth(indexFormat)), size_(indexCnt),
    indexFormat_(indexFormat), cpuMem_(std::move(pData)) {

    auto upBufIdx = cmdList.emplaceXResource<UploadBuffer>(device, cpuMem_.data(), cpuMem_.size());
    auto& upBuf = cmdList.getXResource<UploadBuffer>(upBufIdx);
    cmdList.copyResource(upBuf, *this);

    makeIbv(device, indexFormat, indexCnt);

    commitState(cmdList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
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
    auto strFileName = path.string();
    auto cstrFileName = strFileName.c_str();

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
    RGBQUAD ret{};
    if ( !FreeImage_GetPixelColor( pBitmap_, static_cast<unsigned int>(x), 
        static_cast<unsigned int>(y), &ret 
    ) ) {
        std::cerr << "Failed to get pixel\n";
    }
    return ret.rgbRed;
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

void RefSubmesh::draw( D3D12GfxCmdList& cmdList, std::size_t instanceCnt,
    std::size_t vbLayoutIdx
) const {
    VertexBuffer::bind(cmdList, 0u, parent_->vbs(vbLayoutIdx));
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

RefSubmesh::RefSubmesh(RefSubmesh&& other) noexcept
    : parent_(std::exchange(other.parent_, nullptr)),
    ib_(std::move(other.ib_)),
    material_(std::move(other.material_)),
    topology_(std::exchange(other.topology_, D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)) {}

RefSubmesh& RefSubmesh::operator=(RefSubmesh&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    parent_ = std::exchange(other.parent_, nullptr);
    ib_ = std::move(other.ib_);
    material_ = std::move(other.material_);
    topology_ = std::exchange(other.topology_, D3D_PRIMITIVE_TOPOLOGY_UNDEFINED);

    return *this;
}

RefMesh::RefMesh(RefMesh&& other) noexcept
    : name_(std::move(other.name_)),
    vbLayouts_(std::move(other.vbLayouts_)),
    submeshes_(std::move(other.submeshes_)),
    parent_(std::exchange(other.parent_, nullptr)) {
    for (auto& submesh : submeshes_) {
        submesh.parent_ = this;
    }
}

RefMesh& RefMesh::operator=(RefMesh&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    name_ = std::move(other.name_);
    vbLayouts_ = std::move(other.vbLayouts_);
    submeshes_ = std::move(other.submeshes_);
    parent_ = std::exchange(other.parent_, nullptr);

    for (auto& submesh : submeshes_) {
        submesh.parent_ = this;
    }

    return *this;
}

void RefMesh::arrangeVBs( D3D12Device& device, D3D12GfxCmdList& cmdList,
    std::size_t layoutIdx, const std::vector<std::vector<Vertex::Properties>>& vbProps
) {
    // make newly arranged Vbs based on primaryVBs at layout index 0
    // to match the specified vbProps(layout)

    if (vbLayouts_.size() <= layoutIdx) {
        vbLayouts_.resize(layoutIdx + 1);
    }

    auto& primaryVBs = vbLayouts_[0];
    assert(!primaryVBs.empty());

    auto newVBs = std::vector<VertexBuffer>{};

    for (auto& props : vbProps) {
        // configure memory layout of the new vertex buffer
        auto vb = gfx::VertexBuffer();
        std::size_t stride = 0u;
        for (auto prop : props) {
            stride += Vertex::propByteWidth(prop);
        }
        vb.configStride(stride);

        // calculate the number of vertices
        auto vertexCnt = primaryVBs[0].vbview().SizeInBytes / primaryVBs[0].vbview().StrideInBytes;

        // construct the new vertex buffer
        std::size_t accOffset = 0u;
        for (auto prop : props) {
            vb.configProperty(prop, accOffset);
            accOffset += Vertex::propByteWidth(prop);

            auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
            tmp.set(etoi(prop));

            auto it = std::ranges::find_if(primaryVBs, [tmp](const VertexBuffer& vb) {
                return vb.attributes() == tmp;
            });

            if (it != primaryVBs.end()) {
                assert(Vertex::propByteWidth(prop) == it->vbview().StrideInBytes);
                vb.constructProperty( prop, it->cpuMem(),
                    vertexCnt, Vertex::propByteWidth(prop)
                );
            }
            else {
                vb.constructNullProperty(prop, vertexCnt);
            }
        }

        newVBs.emplace_back( device, cmdList, std::move(vb) );
    }

    vbLayouts_[layoutIdx] = std::move(newVBs);
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

    ret.vbLayouts_.emplace_back();
    auto& primaryVBs = ret.vbLayouts_.back();
    auto vbMem = std::vector<std::uint8_t>();

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
				vbMem.resize(sizeof(dx::XMFLOAT3) * nPositions);
				nReads = (UINT)::fread(vbMem.data(), sizeof(dx::XMFLOAT3), nPositions, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::Position3D));
                primaryVBs.emplace_back( device, cmdList, std::move(vbMem),
                    sizeof(dx::XMFLOAT3) * nPositions, sizeof(dx::XMFLOAT3), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<Colors:>"))
		{
			nReads = (UINT)::fread(&nColors, sizeof(int), 1, pInFile);
			if (nColors > 0)
			{
                vbMem.resize(sizeof(dx::XMFLOAT4) * nColors);
                nReads = (UINT)::fread(vbMem.data(), sizeof(dx::XMFLOAT4), nColors, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::Color4D));
                primaryVBs.emplace_back( device, cmdList, std::move(vbMem),
                    sizeof(dx::XMFLOAT4) * nColors, sizeof(dx::XMFLOAT4), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<TextureCoords0:>"))
		{
			nReads = (UINT)::fread(&nTextureCoords, sizeof(int), 1, pInFile);
			if (nTextureCoords > 0)
			{
                vbMem.resize(sizeof(dx::XMFLOAT2) * nTextureCoords);
                nReads = (UINT)::fread(vbMem.data(), sizeof(dx::XMFLOAT2), nTextureCoords, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::TexCoord2D0));
                primaryVBs.emplace_back( device, cmdList, std::move(vbMem),
                    sizeof(dx::XMFLOAT2) * nTextureCoords, sizeof(dx::XMFLOAT2), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<TextureCoords1:>"))
		{
			nReads = (UINT)::fread(&nTextureCoords, sizeof(int), 1, pInFile);
			if (nTextureCoords > 0)
			{
                vbMem.resize(sizeof(dx::XMFLOAT2) * nTextureCoords);
                nReads = (UINT)::fread(vbMem.data(), sizeof(dx::XMFLOAT2), nTextureCoords, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::TexCoord2D1));
                primaryVBs.emplace_back( device, cmdList, std::move(vbMem),
                    sizeof(dx::XMFLOAT2) * nTextureCoords, sizeof(dx::XMFLOAT2), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<Normals:>"))
		{
			nReads = (UINT)::fread(&nNormals, sizeof(int), 1, pInFile);
			if (nNormals > 0)
			{
                vbMem.resize(sizeof(dx::XMFLOAT3) * nNormals);
                nReads = (UINT)::fread(vbMem.data(), sizeof(dx::XMFLOAT3), nNormals, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::Normal3D));
                primaryVBs.emplace_back( device, cmdList, std::move(vbMem),
                    sizeof(dx::XMFLOAT3) * nNormals, sizeof(dx::XMFLOAT3), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<Tangents:>"))
		{
			nReads = (UINT)::fread(&nTangents, sizeof(int), 1, pInFile);
			if (nTangents > 0)
			{
                vbMem.resize(sizeof(dx::XMFLOAT3) * nTangents);
                nReads = (UINT)::fread(vbMem.data(), sizeof(dx::XMFLOAT3), nTangents, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::Tangent3D));
                primaryVBs.emplace_back( device, cmdList, std::move(vbMem),
                    sizeof(dx::XMFLOAT3) * nTangents, sizeof(dx::XMFLOAT3), tmp
                );
			}
		}
		else if (!strcmp(pstrToken, "<BiTangents:>"))
		{
			nReads = (UINT)::fread(&nBiTangents, sizeof(int), 1, pInFile);
			if (nBiTangents > 0)
			{
                vbMem.resize(sizeof(dx::XMFLOAT3) * nBiTangents);
                nReads = (UINT)::fread(vbMem.data(), sizeof(dx::XMFLOAT3), nBiTangents, pInFile);
                auto tmp = std::bitset<etoi(Vertex::Properties::SIZE)>{};
                tmp.set(etoi(Vertex::Properties::Bitangent3D));
                primaryVBs.emplace_back( device, cmdList, std::move(vbMem),
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
    auto& material = submesh.material();

    for (;;) {
        nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
        nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
        pstrToken[nStrLength] = '\0';

        if (!strcmp(pstrToken, "</Material>")) {
            break;
        }
        else if (!strcmp(pstrToken, "<AlbedoColor:>"))
        {
            nReads = (UINT)::fread(&float4, sizeof(dx::XMFLOAT4), 1, pInFile);
            material.addConstant( Material::ConstantType::Albedo, mu::Vec4(float4.x, float4.y, float4.z, float4.w) );
        }
        else if (!strcmp(pstrToken, "<EmissiveColor:>"))
        {
            nReads = (UINT)::fread(&float4, sizeof(dx::XMFLOAT4), 1, pInFile);
            material.addConstant( Material::ConstantType::Emmisive, mu::Vec3(float4.x, float4.y, float4.z) );
        }
        else if (!strcmp(pstrToken, "<AmbientOcclusion:>"))
        {
            nReads = (UINT)::fread(&floatVal, sizeof(float), 1, pInFile);
            material.addConstant( Material::ConstantType::AmbientOcclusion, floatVal );
        }
        else if (!strcmp(pstrToken, "<Smoothness:>"))
        {
            nReads = (UINT)::fread(&floatVal, sizeof(float), 1, pInFile);
            material.addConstant( Material::ConstantType::Roughness, 1.f - floatVal );
        }
        else if (!strcmp(pstrToken, "<Metallic:>"))
        {
            nReads = (UINT)::fread(&floatVal, sizeof(float), 1, pInFile);
            material.addConstant( Material::ConstantType::Metallic, floatVal );
        }
        else if (!strcmp(pstrToken, "<AmbientOcclusion:>"))
        {
            nReads = (UINT)::fread(&floatVal, sizeof(float), 1, pInFile);
            material.addConstant( Material::ConstantType::AmbientOcclusion, floatVal );
        }
        else if (!strcmp(pstrToken, "<AlbedoMap:>"))
        {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            material.addMapRef( Material::MapType::Albedo, mapRef );
        }
        else if (!strcmp(pstrToken, "<NormalMap:>"))
        {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            material.addMapRef( Material::MapType::Normal, mapRef );
        }
        else if (!strcmp(pstrToken, "<MetallicMap:>"))
        {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            material.addMapRef( Material::MapType::Metallic, mapRef );
        }
        else if (!strcmp(pstrToken, "<MetallicSmoothnessMap:>"))
        {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            material.addMapRef( Material::MapType::MetallicSmoothness, mapRef );
        }
        else if (!strcmp(pstrToken, "<EmissionMap:>"))
        {
            nReads = (UINT)::fread(&mapRef, sizeof(Material::MapRef), 1, pInFile);
            material.addMapRef( Material::MapType::Emmisive, mapRef );
        }
        else {
            fclose(pInFile);
            throw std::runtime_error("expected material token or material end token but got " + std::string(pstrToken));
        }

    }

    const auto albedoRatio = material.contains(Material::MapType::Albedo) ? 0.f : 1.f;
    const auto roughnessRatio = ( material.contains(Material::MapType::MetallicSmoothness)
            || material.contains(Material::MapType::Roughness)
        ) ? 0.f : 1.f;
    const auto metallicRatio = ( material.contains(Material::MapType::MetallicSmoothness)
            || material.contains(Material::MapType::Metallic)
        ) ? 0.f : 1.f;
    const auto emmisiveRatio = material.contains(Material::MapType::Emmisive) ? 0.f : 1.f;
    const auto ambientOcllusionRatio = material.contains(Material::MapType::AmbientOcclusion) ? 0.f : 1.f;

    material.addConstant( Material::ConstantType::AlbedoConstantMapRatio, albedoRatio );
    material.addConstant( Material::ConstantType::RoughnessConstantMapRatio, roughnessRatio );
    material.addConstant( Material::ConstantType::MetallicConstantMapRatio, metallicRatio );
    material.addConstant( Material::ConstantType::EmmisiveConstantMapRatio, emmisiveRatio );
    material.addConstant( Material::ConstantType::AmbientOcclusionConstantMapRatio, ambientOcllusionRatio );
}

RefModel::Node::Node(Node&& other) noexcept
    : coord_(std::move(other.coord_)), name_(std::move(other.name_)),
    meshes_(std::move(other.meshes_)), children_(std::move(other.children_)),
    pRefModel_(std::exchange(other.pRefModel_, nullptr)) {
    for (auto& mesh : meshes_) {
        mesh.parent_ = this;
    }

    for (auto& child : children_) {
        child->pRefModel_ = this->pRefModel_;
    }
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

    for (auto& mesh : meshes_) {
        mesh.parent_ = this;
    }

    for (auto& child : children_) {
        child->pRefModel_ = this->pRefModel_;
    }

    return *this;
}

void RefModel::Node::addMesh(RefMesh&& mesh) {
    for (auto& submesh : mesh.submeshes_) {
        for (auto& mapRef : submesh.material().mapRefs()) {
            if (mapRef.resourceIdx != Material::MapRef::invalid) {
                if (pRefModel_->textureMap_.contains(mapRef)) {
                    mapRef.resourceIdx = static_cast<std::uint32_t>(
                        pRefModel_->textureMap_.at(mapRef).offset()
                    );
                }
            }
        }
    }
    meshes_.push_back(std::move(mesh));
    meshes_.back().parent_ = this;
}

void RefModel::Node::addChild(Node* child) {
    child->pRefModel_ = pRefModel_;
    child->coord_.setParent(&coord_);
    children_.push_back(child);
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

    auto posVBMem = std::vector<std::uint8_t>(width * length * sizeof(dx::XMFLOAT3));
    auto normalVBMem = std::vector<std::uint8_t>(width * length * sizeof(dx::XMFLOAT3));
    auto uvVBMem = std::vector<std::uint8_t>(width * length * sizeof(dx::XMFLOAT2));
    const auto posVBMemSize = posVBMem.size();
    const auto normalVBMemSize = normalVBMem.size();
    const auto uvVBMemSize = uvVBMem.size();

    auto scaleXmf = scale.getXmf();

    for (int i = 0, z = zStart; z < (zStart + length); ++z) {
        for (int x = xStart; x < (xStart + width); ++x, ++i) {
            // Calculate position
            auto height = heightmap.getGreyscalePixel(x, z) / 255.f * scaleXmf.y;

            auto position = dx::XMFLOAT3(x * scaleXmf.x,
                height, z * scaleXmf.z
            );

            auto posOffset = i * sizeof(dx::XMFLOAT3);
            std::memcpy(posVBMem.data() + posOffset, &position, sizeof(dx::XMFLOAT3));

            // Calculate normal
            auto iWidth = static_cast<int>(heightmap.width());
            auto iLength = static_cast<int>(heightmap.height());

            auto nHeightMapIndex = x + (z * iWidth);
            auto xHeightMapAdd = x < iWidth - 1 ? 1 : -1;
            auto zHeightMapAdd = z < iLength - 1 ? 1 : -1;

            float y1 = heightmap.getGreyscalePixel(x, z) / 255.f * scaleXmf.y;
            float y2 = heightmap.getGreyscalePixel(x + xHeightMapAdd, z) / 255.f * scaleXmf.y;
            float y3 = heightmap.getGreyscalePixel(x, z + zHeightMapAdd) / 255.f * scaleXmf.y;

            auto edge1 = mu::Vec3(0.f, y3 - y1, scaleXmf.z);
            auto edge2 = mu::Vec3(scaleXmf.x, y2 - y1, 0.f);
            auto normal = mu::cross(edge1, edge2);

            auto normalOffset = i * sizeof(dx::XMFLOAT3);
            std::memcpy(normalVBMem.data() + normalOffset, &normal, sizeof(dx::XMFLOAT3));

            // Calculate UV
            auto uv = dx::XMFLOAT2(static_cast<float>(x) / iWidth, static_cast<float>(z) / iLength);

            auto uvOffset = i * sizeof(dx::XMFLOAT2);
            std::memcpy(uvVBMem.data() + uvOffset, &uv, sizeof(dx::XMFLOAT2));
        }
    }

    auto mesh = RefMesh();
    mesh.vbLayouts_.emplace_back();
    auto& primaryVBs = mesh.vbLayouts_.back();

    primaryVBs.emplace_back(device, cmdList, std::move(posVBMem),
        posVBMemSize, sizeof(dx::XMFLOAT3),
        std::bitset<etoi(Vertex::Properties::SIZE)>{ 1ull << etoi(Vertex::Properties::Position3D) }
    );

    primaryVBs.emplace_back(device, cmdList, std::move(normalVBMem),
        normalVBMemSize, sizeof(dx::XMFLOAT3),
        std::bitset<etoi(Vertex::Properties::SIZE)>{ 1ull << etoi(Vertex::Properties::Normal3D) }
    );

    primaryVBs.emplace_back(device, cmdList, std::move(uvVBMem),
        uvVBMemSize, sizeof(dx::XMFLOAT2),
        std::bitset<etoi(Vertex::Properties::SIZE)>{ 1ull << etoi(Vertex::Properties::TexCoord2D0) }
    );

    // without tesselation
    // mesh.submeshes_.emplace_back(&mesh, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // with tesselation (quad patch, 16 control points)
    mesh.submeshes_.emplace_back(&mesh, D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST);

    auto& submesh = mesh.submeshes_.back();


    // without tesselation

    // auto makeIb = [&](auto&& indices) {
    //     using IndexType = std::ranges::range_value_t<decltype(indices)>;
    //     indices.reserve(((width * 2) * (length - 1)) + ((length - 1) - 1));

    //     auto out = std::back_inserter(indices);

    //     for (int i = 0, z = 0; z < length - 1; ++z) {
    //         if (!(z % 2)) {
    //             for (int x = 0; x < width; ++x) {
    //                 if (x == 0 && z > 0) {
    //                     out = static_cast<IndexType>(x + (z * width));
    //                 }
    //                 out = static_cast<IndexType>(x + (z * width));
    //                 out = static_cast<IndexType>(x + ((z + 1) * width));
    //             }
    //         }
    //         else {
    //             for (int x = width - 1; x >= 0; --x) {
    //                 if (x == (width - 1)) {
    //                     out = static_cast<IndexType>(x + (z * width));
    //                 }
    //                 out = static_cast<IndexType>(x + (z * width));
    //                 out = static_cast<IndexType>(x + ((z + 1) * width));
    //             }
    //         }
    //     }

    //     const auto format = sizeof(IndexType) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    //     submesh.ib_ = IndexBuffer(device, cmdList, indices.data(), format, indices.size());
    // };

    // with tesselation (quad patch, 16 control points)

    const auto indexCnt = (width / 16 - 3) * (length / 16 - 3) * 16;
    const auto indexByteWidth = indexCnt < 65536 ? sizeof(std::uint16_t) : sizeof(std::uint32_t);

    std::vector<std::uint8_t> ibMem(indexCnt * indexByteWidth);
    auto k = 0;

    for (int z = 0; z < length - 3 * 16; z += 16) {
        for (int x = 0; x < width - 3 * 16; x += 16) {
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    auto idx = x + j * 16 + ((z + i * 16) * width);
                    std::memcpy(ibMem.data() + k++ * indexByteWidth, &idx, indexByteWidth);
                }
            }
        }
    }

    const auto format = indexCnt < 65536 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    assert(indexCnt == k);
    submesh.ib_ = IndexBuffer(device, cmdList, std::move(ibMem), format, k);
    

    auto& material = submesh.material();

    material.addMapRef( Material::MapType::Albedo, albedoMapRef );

    // temporal constants
    material.addConstant( Material::ConstantType::AmbientOcclusion, 1.f );
    material.addConstant( Material::ConstantType::Roughness, 0.83f );
    material.addConstant( Material::ConstantType::Metallic, 0.07f );
    material.addConstant( Material::ConstantType::Emmisive, mu::Vec3(0.f) );
    material.addConstant( Material::ConstantType::AlbedoConstantMapRatio, 0.f );
    material.addConstant( Material::ConstantType::RoughnessConstantMapRatio, 1.f );
    material.addConstant( Material::ConstantType::MetallicConstantMapRatio, 1.f );
    material.addConstant( Material::ConstantType::EmmisiveConstantMapRatio, 1.f );
    material.addConstant( Material::ConstantType::AmbientOcclusionConstantMapRatio, 1.f );

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
        else if (!strcmp(pstrToken, "</NodesInfo>")) {
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

void RefModel::arrangeVBs( D3D12Device& device, D3D12GfxCmdList& cmdList,
    std::size_t layoutIdx, const std::vector<std::vector<Vertex::Properties>>& vbProps
) {
    for (auto& node : nodeStorage_) {
        for (auto& mesh : node.meshes_) {
            mesh.arrangeVBs(device, cmdList, layoutIdx, vbProps);
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

Submesh::Submesh(Submesh&& other) noexcept
    : parent_(std::exchange(other.parent_, nullptr)),
    pRefSubmesh_(std::exchange(other.pRefSubmesh_, nullptr)),
    material_(std::move(other.material_)) {}

Submesh& Submesh::operator=(Submesh&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    parent_ = std::exchange(other.parent_, nullptr);
    pRefSubmesh_ = std::exchange(other.pRefSubmesh_, nullptr);
    material_ = std::move(other.material_);

    return *this;
}

Mesh::Mesh(const RefMesh& refMesh, Model::Node* parent)
    : parent_(parent), pRefMesh_(&refMesh), submeshes_() {
    submeshes_.reserve(refMesh.submeshes().size());
    for (const auto& refSubmesh : refMesh.submeshes()) {
        submeshes_.emplace_back(this, &refSubmesh);
    }
}

Mesh::Mesh(const Mesh& other)
    : parent_(other.parent_), pRefMesh_(other.pRefMesh_),
    submeshes_(other.submeshes_) {
    for (auto& submesh : submeshes_) {
        submesh.parent_ = this;
    }
}

Mesh::Mesh(Mesh&& other) noexcept
    : parent_(std::exchange(other.parent_, nullptr)),
    pRefMesh_(std::exchange(other.pRefMesh_, nullptr)),
    submeshes_(std::move(other.submeshes_)) {
    for (auto& submesh : submeshes_) {
        submesh.parent_ = this;
    }
}

Mesh& Mesh::operator=(const Mesh& other) {
    parent_ = other.parent_;
    pRefMesh_ = other.pRefMesh_;
    submeshes_ = other.submeshes_;

    for (auto& submesh : submeshes_) {
        submesh.parent_ = this;
    }

    return *this;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    parent_ = std::exchange(other.parent_, nullptr);
    pRefMesh_ = std::exchange(other.pRefMesh_, nullptr);
    submeshes_ = std::move(other.submeshes_);

    for (auto& submesh : submeshes_) {
        submesh.parent_ = this;
    }

    return *this;
}

Model::Node::Node(const Node& other)
    : coord_(other.coord_), meshes_(other.meshes_),
    children_(), pModel_(other.pModel_) {
    for (auto& mesh : meshes_) {
        mesh.parent_ = this;
    }
}

Model::Node::Node(Node&& other) noexcept
    : coord_(std::move(other.coord_)), meshes_(std::move(other.meshes_)),
    children_(std::move(other.children_)),
    pModel_(std::exchange(other.pModel_, nullptr)) {
    for (auto& mesh : meshes_) {
        mesh.parent_ = this;
    }

    for (auto& child : children_) {
        child->pModel_ = this->pModel_;
    }
}

Model::Node& Model::Node::operator=(const Node& other) {
    coord_ = other.coord_;
    meshes_ = other.meshes_;
    children_.clear();
    children_.shrink_to_fit();
    pModel_ = other.pModel_;

    for (auto& mesh : meshes_) {
        mesh.parent_ = this;
    }

    return *this;
}

Model::Node& Model::Node::operator=(Node&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    coord_ = std::move(other.coord_);
    meshes_ = std::move(other.meshes_);
    children_ = std::move(other.children_);
    pModel_ = std::exchange(other.pModel_, nullptr);

    for (auto& mesh : meshes_) {
        mesh.parent_ = this;
    }

    for (auto& child : children_) {
        child->pModel_ = this->pModel_;
    }

    return *this;
}

void Model::Node::addMesh(Mesh&& mesh) {
    meshes_.push_back(std::move(mesh));
    meshes_.back().parent_ = this;
}

void Model::Node::emplaceMesh(const RefMesh& refMesh) {
    meshes_.emplace_back(refMesh, this);
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

void ScreenQuad::draw(D3D12GfxCmdList& cmdList) const {
    cmdList.get()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    DX_THROW_FAILED_VOID( cmdList.get()->DrawInstanced(4u, 1u, 0u, 0u) );
}

LevelRegionModel::LevelRegionModel(const StaticTextureStorage& sts, std::istream&& is)
    : chunks_() {
    char pstrToken[64] = { '\0' };

	BYTE nStrLength = 0;
	UINT nReads = 0;

    Material::MapRef mapRef{};
    float floatVal{};

    std::map<Material::MapRef, DescriptorGPU> textureMap;


    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<Dictionary:>")) {
        throw std::runtime_error("<Dictionary:> tag expected but has not been received, in LevelChunkModel Loading.");    
    }

    // map textures
    for (;;) {
        is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
        is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
        pstrToken[nStrLength] = '\0';

        if (!strcmp(pstrToken, "<Item:>")) {
            is.read(reinterpret_cast<char*>(&mapRef), sizeof(Material::MapRef));

            is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
            auto texRelativePath = std::string(nStrLength, '\0');
            is.read(texRelativePath.data(), nStrLength);

            auto texPath = resourcePath / std::move(texRelativePath);

            if (!sts.contains(texPath)) {
                throw std::runtime_error("Texture not found: " + texPath.string());
            }
            textureMap[mapRef] = sts.get(texPath);
        }
        else if (!strcmp(pstrToken, "</Dictionary>")) {
            break;
        }
        else {
            throw std::runtime_error("expected Item or Dictionary end token but got: " + std::string(pstrToken));
        }
    }

    
    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<Chunks:>")) {
        throw std::runtime_error("Chunks token expected but got: " + std::string(pstrToken));
    }

    int nChunks = 0;
    is.read(reinterpret_cast<char*>(&nChunks), sizeof(int));
    chunks_.resize(nChunks);

    for (auto& chunk : chunks_) {
        chunk.load(sts, textureMap, is);
    }

    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "</Chunks>")) {
        throw std::runtime_error("Chunks end token expected but got: " + std::string(pstrToken));
    }
}

LevelChunkModel& LevelRegionModel::get(const dx::XMUINT2& idx) {
    auto it = std::ranges::find_if(chunks_, [&](const auto& chunk) {
        return chunk.idx().x == idx.x && chunk.idx().y == idx.y;
    });

    if (it == chunks_.end()) {
        throw std::runtime_error("Chunk not found: " + std::to_string(idx.x) + ", " + std::to_string(idx.y));
    }

    return *it;
}

const LevelChunkModel& LevelRegionModel::get(const dx::XMUINT2& idx) const {
    auto it = std::ranges::find_if(chunks_, [&](const auto& chunk) {
        return chunk.idx().x == idx.x && chunk.idx().y == idx.y;
    });

    if (it == chunks_.end()) {
        throw std::runtime_error("Chunk not found: " + std::to_string(idx.x) + ", " + std::to_string(idx.y));
    }

    return *it;
}

void LevelChunkModel::load( const StaticTextureStorage& sts,
    std::map<Material::MapRef, DescriptorGPU>& textureMap, std::istream& is
) {
    char pstrToken[64] = { '\0' };

	BYTE nStrLength = 0;
	UINT nReads = 0;

    Material::MapRef mapRef{};
    float floatVal{};
    dx::XMFLOAT2 float2Val{};

    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<Chunk:>")) {
        throw std::runtime_error("Chunk token expected but got: " + std::string(pstrToken));
    }

    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    auto chunkName = std::string(nStrLength, '\0');
    is.read(chunkName.data(), nStrLength);

    // TerrainName_M_N => M(x), N(z) extract
    auto pos = chunkName.find_last_of('_');
    if (pos == std::string::npos) {
        throw std::runtime_error("Invalid chunk name: " + chunkName);
    }

    const auto zStrPos = pos + 1;
    pos = chunkName.find_last_of('_', pos - 1);
    if (pos == std::string::npos) {
        throw std::runtime_error("Invalid chunk name: " + chunkName);
    }

    const auto xStrPos = pos + 1;

    idx_.x = std::stoi(chunkName.substr(xStrPos, zStrPos - xStrPos - 1));
    idx_.y = std::stoi(chunkName.substr(zStrPos));

    // HeightMap
    const auto heightMapPath = resourcePath/"terrains/HeightMaps"/(chunkName + "_HeightMap.dds");
    if ( !sts.contains(heightMapPath) ) {
        throw std::runtime_error(std::string("HeightMap not found: ") + heightMapPath.string());
    }
    mapRef = Material::MapRef{
        .resourceIdx = static_cast<std::uint32_t>(sts.get(heightMapPath).offset())
    };
    material_.addMapRef(Material::MapType::Height, mapRef);        

    // Layer
    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<Layer:>")) {
        throw std::runtime_error("Layer token expected but got: " + std::string(pstrToken));
    }

    int layer{};    // temporarily no use
    is.read(reinterpret_cast<char*>(&layer), sizeof(int));
    
    // AlbedoMap
    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<AlbedoMap:>")) {
        throw std::runtime_error("AlbedoMap token expected but got: " + std::string(pstrToken));
    }

    is.read(reinterpret_cast<char*>(&mapRef), sizeof(Material::MapRef));
    mapRef.resourceIdx = static_cast<std::uint32_t>(textureMap.at(mapRef).offset());
    material_.addMapRef(Material::MapType::Albedo, mapRef);
    material_.addConstant(Material::ConstantType::AlbedoConstantMapRatio, 0.f);

    // NormalMap
    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<NormalMap:>")) {
        throw std::runtime_error("NormalMap token expected but got: " + std::string(pstrToken));
    }

    is.read(reinterpret_cast<char*>(&mapRef), sizeof(Material::MapRef));
    mapRef.resourceIdx = static_cast<std::uint32_t>(textureMap.at(mapRef).offset());
    material_.addMapRef(Material::MapType::Normal, mapRef);

    // Smoothness
    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<Smoothness:>")) {
        throw std::runtime_error("Smoothness token expected but got: " + std::string(pstrToken));
    }

    is.read(reinterpret_cast<char*>(&floatVal), sizeof(float));
    material_.addConstant(Material::ConstantType::Roughness, 1.f - floatVal);
    material_.addConstant(Material::ConstantType::RoughnessConstantMapRatio, 1.f);

    // Metallic
    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<Metallic:>")) {
        throw std::runtime_error("Metallic token expected but got: " + std::string(pstrToken));
    }

    is.read(reinterpret_cast<char*>(&floatVal), sizeof(float));
    material_.addConstant(Material::ConstantType::Metallic, floatVal);
    material_.addConstant(Material::ConstantType::MetallicConstantMapRatio, 1.f);

    // Tile Size
    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<TileSize:>")) {
        throw std::runtime_error("TileSize token expected but got: " + std::string(pstrToken));
    }

    is.read(reinterpret_cast<char*>(&float2Val), sizeof(dx::XMFLOAT2));
    material_.addConstant(Material::ConstantType::TileSize, mu::Vec2(float2Val.x, float2Val.y));

    // Tile Offset
    is.read(reinterpret_cast<char*>(&nStrLength), sizeof(BYTE));
    is.read(reinterpret_cast<char*>(pstrToken), nStrLength);
    pstrToken[nStrLength] = '\0';

    if (strcmp(pstrToken, "<TileOffset:>")) {
        throw std::runtime_error("TileOffset token expected but got: " + std::string(pstrToken));
    }

    is.read(reinterpret_cast<char*>(&float2Val), sizeof(dx::XMFLOAT2));
    material_.addConstant(Material::ConstantType::TileOffset, mu::Vec2(float2Val.x, float2Val.y));
}

void LevelChunkModel::draw(D3D12GfxCmdList& cmdList) const {
    VertexBuffer::bind(cmdList, 0u, std::views::single(sChunkVb));
    cmdList.get()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
    sChunkIb.bind(cmdList);

    DX_THROW_FAILED_VOID(
        cmdList.get()->DrawIndexedInstanced(
            static_cast<UINT>( sChunkIb.size() ),
            1u, 0u, 0, 0u
        )
    );
}

mu::Mat4x4 MU_CALLCONV LevelChunkModel::idxToWorld() const {
    // temporary
    return mu::translate(
        0.f + 0.f * static_cast<float>(idx_.x),
        -50.f + 0.f,
        0.f + 0.f * static_cast<float>(idx_.y)
    );
}

void LevelChunkModel::initChunkMesh(D3D12Device& device, D3D12GfxCmdList& cmdList) {
    // construct 256x256 size, 100m x 100m area patch
    static constexpr auto patchWidth = 256;
    static constexpr auto patchLength = 256;

    // construct vertex buffer
    std::vector<std::uint8_t> vbMem(patchWidth * patchLength * sizeof(PatchVertex));

    for (int z = 0; z < patchLength; ++z) {
        for (int x = 0; x < patchWidth; ++x) {
            const auto vertex = PatchVertex{
                .pos = dx::XMFLOAT3(
                    static_cast<float>(x) / patchWidth * 100.f, 0.f, static_cast<float>(z) / patchLength * 100.f
                ),
                .texCoord = dx::XMFLOAT2(
                    static_cast<float>(x) / patchWidth, static_cast<float>(z) / patchLength
                )
            };

            std::memcpy( vbMem.data() + (z * patchWidth + x) * sizeof(PatchVertex),
                &vertex, sizeof(PatchVertex)
            );
        }
    }

    const auto vbMemByteWidth = vbMem.size();
    sChunkVb = VertexBuffer(device, cmdList, std::move(vbMem), vbMemByteWidth, sizeof(PatchVertex),
        std::bitset<etoi(Vertex::Properties::SIZE)>(
            (1ull << etoi(Vertex::Properties::Position3D))
            | (1ull << etoi(Vertex::Properties::TexCoord2D0))
        )
    );

    // construct index buffer
    std::vector<std::uint8_t> ibMem((patchWidth - 1) * (patchLength - 1) * 4 * sizeof(std::uint16_t));

    auto k = 0;

    for (int z = 0; z < patchLength - 1; ++z) {
        for (int x = 0; x < patchWidth - 1; ++x) {
            for (int i = 0; i < 2; ++i) {
                for (int j = 0; j < 2; ++j) {
                    auto idx = static_cast<std::uint16_t>( x + j + ((z + i) * patchWidth) );
                    std::memcpy(ibMem.data() + k++ * sizeof(std::uint16_t), &idx, sizeof(std::uint16_t));
                }
            }
        }
    }

    const auto format = DXGI_FORMAT_R16_UINT;
    const auto indexCnt = k;

    sChunkIb = IndexBuffer(device, cmdList, std::move(ibMem), format, indexCnt);
}

VertexBuffer LevelChunkModel::sChunkVb;
IndexBuffer LevelChunkModel::sChunkIb;

}   // namespace gfx::d3d12

}   // namespace gfx