#include "gpuMem.hpp"

#include "dxexcept.hpp"

#include <cstdint>

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
    // is this necessary?
    auto bar = D3D12_RESOURCE_BARRIER{
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource = buffer_.Get(),
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_COMMON,
            .StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE
        }
    };

    DX_THROW_FAILED_VOID( pCmdList->CopyBufferRegion(buffer_.Get(), addr - head_, pSrcBuf, offset, bytes) );

    // is this necessary?
    bar = D3D12_RESOURCE_BARRIER{
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource = buffer_.Get(),
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
            .StateAfter = D3D12_RESOURCE_STATE_COMMON
        }
    };

    DX_THROW_FAILED_VOID( pCmdList->ResourceBarrier(1, &bar) );
}

UploadMemPool::UploadMemPool( Core &core, D3D12RenderContext &ctx,
    std::size_t blockSize, std::size_t blockCount
) : freeList_(), buffer_( createUpBuf( core, static_cast<UINT64>(blockSize * blockCount) ) ),
    blockSize_(blockSize), blockCount_(blockCount), head_(buffer_->GetGPUVirtualAddress()) {
    auto freePtr = head_;

    for (std::size_t i = 0; i < blockCount; ++i) {
        freeList_.push_back(freePtr);
        freePtr += blockSize;
    }
}

D3D12_GPU_VIRTUAL_ADDRESS UploadMemPool::allocate() {
    if (freeList_.empty()) {
        return 0;
    }

    auto ret = freeList_.front();
    freeList_.pop_front();
    return ret;
}

void UploadMemPool::deallocate(D3D12_GPU_VIRTUAL_ADDRESS addr) {
    checkAddressValidity(addr);
    freeList_.push_back(addr);
}

D3D12_GPU_VIRTUAL_ADDRESS UploadMemPool::construct_at( Core& core, D3D12RenderContext& ctx,
    D3D12_GPU_VIRTUAL_ADDRESS addr, const void* pData, std::size_t bytes
) {
    auto pCmdList = std::any_cast< wrl::ComPtr<ID3D12GraphicsCommandList> >(
        ctx.cast(RenderContextType::D3D12)
    );

    checkAddressValidity(addr);

    std::uint8_t* pMappedData = nullptr;

    auto readRange = D3D12_RANGE{};
    DX_THROW_FAILED( buffer_->Map(0, &readRange, reinterpret_cast<void**>(&pMappedData)) );

    std::memcpy(pMappedData + (addr - head_), pData, bytes);

    auto writeRange = D3D12_RANGE{ addr - head_, addr - head_ + bytes };
    DX_THROW_FAILED_VOID( buffer_->Unmap(0, &writeRange) );

    return addr;
}

void UploadMemPool::checkAddressValidity(D3D12_GPU_VIRTUAL_ADDRESS addr) {
    if (addr < head_ || addr >= head_ + blockSize_ * blockCount_) {
        throw std::runtime_error("Invalid address");
    }

    if ((addr - head_) % blockSize_ != 0) {
        throw std::runtime_error("Invalid address");
    }
}

} // namespace gfx::d3d12

}   // namespace gfx