#ifndef __GpuMem_HPP
#define __GpuMem_HPP

#include "d3d12res.hpp"

#include <list>

namespace gfx {

namespace d3d12 {

class GpuMemPool {
public:
    GpuMemPool(Core& core, D3D12RenderContext& ctx, std::size_t blockSize, std::size_t blockCount);

    D3D12_GPU_VIRTUAL_ADDRESS allocate();
    void deallocate(D3D12_GPU_VIRTUAL_ADDRESS addr);
    D3D12_GPU_VIRTUAL_ADDRESS construct_at( D3D12RenderContext& ctx, D3D12_GPU_VIRTUAL_ADDRESS addr,
        ID3D12Resource* pSrcBuf
    ) {
        return construct_at(ctx, addr, pSrcBuf, 0, pSrcBuf->GetDesc().Width);
    }
    D3D12_GPU_VIRTUAL_ADDRESS construct_at( D3D12RenderContext& ctx, D3D12_GPU_VIRTUAL_ADDRESS addr,
        ID3D12Resource* pSrcBuf, UINT64 offset, UINT64 bytes
    );
    D3D12_GPU_VIRTUAL_ADDRESS construct_at( Core& core, D3D12RenderContext& ctx, D3D12_GPU_VIRTUAL_ADDRESS addr,
        const void* pData, std::size_t bytes
    ) {
        auto pUploadBuf = createUpBuf(core, pData, bytes);
        return construct_at(ctx, addr, pUploadBuf.Get(), 0, bytes);
    }
    template <std::ranges::contiguous_range R>
        requires std::ranges::sized_range<R>
    D3D12_GPU_VIRTUAL_ADDRESS construct_at( D3D12RenderContext& ctx, D3D12_GPU_VIRTUAL_ADDRESS addr,
        const R& data
    ) {
        return construct_at(addr, std::data(data), std::size(data) * sizeof(std::ranges::range_value_t<R>));
    }

private:
    void checkAddressValidity(D3D12_GPU_VIRTUAL_ADDRESS addr);
    void copyAt( ID3D12GraphicsCommandList* pCmdList, D3D12_GPU_VIRTUAL_ADDRESS addr,
        ID3D12Resource* pSrcBuf, UINT64 offset, UINT64 bytes
    );

    std::list<D3D12_GPU_VIRTUAL_ADDRESS> freeList_;
    wrl::ComPtr<ID3D12Resource> buffer_;
    std::size_t blockSize_;
    std::size_t blockCount_;
    D3D12_GPU_VIRTUAL_ADDRESS head_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __GpuMem_HPP