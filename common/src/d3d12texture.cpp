#include "d3d12texture.hpp"

#include "d3d12resLow.hpp"
#include "dxexcept.hpp"

#include <memory>
#include <vector>
#include <cstdint>

namespace gfx {

namespace d3d12 {

void Texture::makeSrv(Core& core, const Descriptor& desc) {
    srv_ = desc;
    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );
    srv_.makeSrv(pDevice, res_.Get());
}

void Texture::makeSrv(Core& core, const Descriptor& desc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc) {
    srv_ = desc;
    auto pDevice = static_cast<ID3D12Device*>(DeviceFetcher::device(core));
    srv_.makeSrv(pDevice, res_.Get(), srvDesc);
}

void Texture::makeRtv(Core& core, const Descriptor& desc) {
    rtv_ = desc;
    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );
    rtv_.makeRtv(pDevice, res_.Get());
}

void Texture::makeDsv(Core& core, const Descriptor& desc) {
    dsv_ = desc;
    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );
    dsv_.makeDsv(pDevice, res_.Get());
}

void Texture::cvt( D3D12RenderContext& ctx, D3D12_RESOURCE_STATES state ) {
    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        ctx.cast(RenderContextType::D3D12)
    );

    auto bar = D3D12_RESOURCE_BARRIER{
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Transition = {
            .pResource = res_.Get(),
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = state_,
            .StateAfter = state
        }
    };

    pCmdList->ResourceBarrier(1, &bar);

    state_ = state;
}

void Texture::load( Core& core, D3D12RenderContext& ctx,
    const std::filesystem::path& path, D3D12_RESOURCE_STATES initialState
) {
    if (path.extension() == ".dds") {
        loadDDS(core, ctx, path, initialState);
    } else {
        loadWIC(core, ctx, path, initialState);
    }

    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        ctx.cast(RenderContextType::D3D12)
    );

    auto bar = D3D12_RESOURCE_BARRIER{
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Transition = {
            .pResource = res_.Get(),
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
            .StateAfter = initialState
        }
    };

    state_ = initialState;
    desc_ = res_->GetDesc();

    pCmdList->ResourceBarrier(1, &bar);

    // dangerous:
    // It does not wait for the gpu to finish the command list.
    // So, If the created SRV is used before the command list is executed,
    // it will cause an error.
}

void Texture::loadDDS( Core& core, D3D12RenderContext& ctx,
    const std::filesystem::path& path, D3D12_RESOURCE_STATES initialState
) {
    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        ctx.cast(RenderContextType::D3D12)
    );
    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );

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

    auto requiredBytes = GetRequiredIntermediateSize(res_.Get(), 0, static_cast<UINT>( subresources.size() ));

    upRes_ = createUpBuf(core, requiredBytes);

    UpdateSubresources( pCmdList.Get(), res_.Get(), upRes_.Get(),
        0, 0, static_cast<UINT>( subresources.size() ), subresources.data()
    );
}

void Texture::loadWIC( Core& core, D3D12RenderContext& ctx,
    const std::filesystem::path& path, D3D12_RESOURCE_STATES initialState
) {
    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        ctx.cast(RenderContextType::D3D12)
    );

    auto wicData = std::unique_ptr<std::uint8_t[]>();
    auto subresource = D3D12_SUBRESOURCE_DATA();

    DX_THROW_FAILED( DirectX::LoadWICTextureFromFileEx(
        static_cast<ID3D12Device*>( DeviceFetcher::device(core) ),
        path.wstring().c_str(),
        0,
        D3D12_RESOURCE_FLAG_NONE,
        dx::WIC_LOADER_DEFAULT,
        &res_,
        wicData,
        subresource
    ) );

    auto requiredBytes = GetRequiredIntermediateSize(res_.Get(), 0, 1u);

    upRes_ = createUpBuf(core, requiredBytes);

    UpdateSubresources( pCmdList.Get(), res_.Get(), upRes_.Get(),
        0, 0, 1u, &subresource
    );
}

}   // namespace gfx::d3d12

}   // namespace gfx