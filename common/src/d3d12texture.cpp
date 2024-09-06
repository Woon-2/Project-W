#include "d3d12texture.hpp"

#include "dxtarget.hpp"
#include "dxexcept.hpp"

#include <memory>
#include <vector>
#include <cstdint>

namespace gfx {

namespace d3d12 {

void Texture::load(Core& core, const std::filesystem::path& path) {
    if (path.extension() == ".dds") {
        loadDDS(core, path);
    } else {
        loadWIC(core, path);
    }
}

void Texture::loadDDS(Core& core, const std::filesystem::path& path) {
    auto ddsData = std::unique_ptr<std::uint8_t[]>();
    auto subresources = std::vector<D3D12_SUBRESOURCE_DATA>();
    auto alphaMode = dx::DDS_ALPHA_MODE_UNKNOWN;
    auto blsCubemap = false;

    DX_THROW_FAILED( DirectX::LoadDDSTextureFromFileEx(
        static_cast<ID3D12Device*>( DeviceFetcher::device(core) ),
        path.wstring().c_str(),
        0,
        D3D12_RESOURCE_FLAG_NONE,
        dx::DDS_LOADER_DEFAULT,
        &res_,
        ddsData,
        subresources,
        &alphaMode,
        &blsCubemap
    ) );

    auto heapPropDesc = D3D12_HEAP_PROPERTIES{
        .Type = D3D12_HEAP_TYPE_UPLOAD,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0
    };

    auto requiredBytes = GetRequiredIntermediateSize(res_.Get(), 0, static_cast<UINT>( subresources.size() ));
}

void Texture::loadWIC(Core& core, const std::filesystem::path& path) {

}

}   // namespace gfx::d3d12

}   // namespace gfx