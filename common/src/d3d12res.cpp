#include "d3d12res.hpp"

#include <cstdlib>

namespace gfx {

namespace d3d12 {

wrl::ComPtr<ID3D12Resource> createUpBuf(Core& core, UINT bytes) {
    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );

    auto ret = wrl::ComPtr<ID3D12Resource>();

    auto desc = D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = bytes,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    auto heapProps = D3D12_HEAP_PROPERTIES{
        .Type = D3D12_HEAP_TYPE_UPLOAD,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0
    };

    DX_THROW_FAILED( pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        __uuidof(ID3D12Resource), &ret
    ) );

    return ret;
}

wrl::ComPtr<ID3D12Resource> createUpBuf(Core& core, const void* pData, UINT bytes) {
    auto ret = createUpBuf(core, bytes);

    UINT8* pMappedData = nullptr;
    ret->Map(0, nullptr, reinterpret_cast<void**>(&pMappedData));
    std::memcpy(pMappedData, pData, bytes);
    ret->Unmap(0, nullptr);

    return ret;
}

wrl::ComPtr<ID3D12Resource> createDefBuf( Core& core, D3D12RenderContext& ctx,
    ID3D12Resource* pSrcBuf, D3D12_RESOURCE_STATES state
) {
    auto pCmdList = std::any_cast< wrl::ComPtr<ID3D12GraphicsCommandList> >(
        ctx.cast(RenderContextType::D3D12)
    );
    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );

    auto ret = wrl::ComPtr<ID3D12Resource>();

    auto desc = D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = pSrcBuf->GetDesc().Width,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = DXGI_SAMPLE_DESC{
            .Count = 1,
            .Quality = 0
        },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    auto heapProps = D3D12_HEAP_PROPERTIES{
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0
    };

    DX_THROW_FAILED( pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        __uuidof(ID3D12Resource), &ret
    ) );

    pCmdList->CopyResource(ret.Get(), pSrcBuf);

    auto bar = D3D12_RESOURCE_BARRIER{
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource = ret.Get(),
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
            .StateAfter = state
        }
    };

    pCmdList->ResourceBarrier(1, &bar);

    return ret;
}


wrl::ComPtr<ID3D12Resource> createDefBuf( Core& core, D3D12RenderContext& ctx, const void* pData,
    UINT bytes, D3D12_RESOURCE_STATES state, wrl::ComPtr<ID3D12Resource>& pUploadBuf
) {
    pUploadBuf = createUpBuf(core, pData, bytes);
    return createDefBuf(core, ctx, pUploadBuf.Get(), state);
}

}   // namespace d3d12

}   // namespace gfx