#include "gpuMem.hpp"

namespace gfx {

namespace d3d12 {

GpuMemPool::GpuMemPool(Core& core, D3D12RenderContext& ctx, std::size_t blockSize, std::size_t blockCount)
    : freeList_(), buffer_( createDefBuf( core,
        static_cast<UINT64>(blockSize * blockCount), D3D12_RESOURCE_STATE_COMMON
    ) ), blockSize_(blockSize), blockCount_(blockCount), head_(buffer_->GetGPUVirtualAddress()) {
    auto freePtr = head_;

    for (std::size_t i = 0; i < blockCount; ++i) {
        freeList_.push_back(freePtr);
        freePtr += blockSize;
    }
}

D3D12_GPU_VIRTUAL_ADDRESS GpuMemPool::allocate() {
    if (freeList_.empty()) {
        return 0;
    }

    auto ret = freeList_.front();
    freeList_.pop_front();
    return ret;
}

void GpuMemPool::deallocate(D3D12_GPU_VIRTUAL_ADDRESS addr) {
    checkAddressValidity(addr);
    freeList_.push_back(addr);
}

D3D12_GPU_VIRTUAL_ADDRESS GpuMemPool::construct_at( D3D12RenderContext &ctx,
    D3D12_GPU_VIRTUAL_ADDRESS addr, ID3D12Resource *pSrcBuf, UINT64 offset, UINT64 bytes
) {
    auto pCmdList = std::any_cast< wrl::ComPtr<ID3D12GraphicsCommandList> >(
        ctx.cast(RenderContextType::D3D12)
    );

    checkAddressValidity(addr);
    auto sz = pSrcBuf->GetDesc().Width;
    if (sz < offset + bytes) {
        throw std::runtime_error("Source buffer is too small");
    }
    copyAt(pCmdList.Get(), addr, pSrcBuf, offset, bytes);
    return addr;
}

void GpuMemPool::checkAddressValidity(D3D12_GPU_VIRTUAL_ADDRESS addr) {
    if (addr < head_ || addr >= head_ + blockSize_ * blockCount_) {
        throw std::runtime_error("Invalid address");
    }

    if ((addr - head_) % blockSize_ != 0) {
        throw std::runtime_error("Invalid address");
    }
}

void GpuMemPool::copyAt( ID3D12GraphicsCommandList *pCmdList, D3D12_GPU_VIRTUAL_ADDRESS addr,
    ID3D12Resource *pSrcBuf, UINT64 offset, UINT64 bytes
) {
    pCmdList->CopyBufferRegion(buffer_.Get(), addr - head_, pSrcBuf, offset, bytes);
}

}  // namespace gfx::d3d12

}   // namespace gfx