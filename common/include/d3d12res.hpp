#ifndef __D3D12Res_HPP
#define __D3D12Res_HPP

#include "d3d12resLow.hpp"
#include "gpuMem.hpp"

namespace gfx {

namespace d3d12 {

class GpuMappedRes {
private:
    struct MyPair {
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddr;
        void* pData; 
    };

public:
    template <std::ranges::range R>
        requires std::same_as< std::ranges::range_value_t<R>, wrl::ComPtr<ID3D12Resource> >
    GpuMappedRes( Core& core, UINT64 bytes, R& ppResources,
        std::size_t duplicateCnt = 1
    );

    template <std::ranges::range R>
        requires std::same_as< std::ranges::range_value_t<R>, wrl::ComPtr<ID3D12Resource> >
    GpuMappedRes( Core& core, const void* pData, UINT64 bytes,
        R& resources, std::size_t duplicateCnt = 1
    );

    template <std::ranges::contiguous_range R1, std::ranges::range R2>
        requires std::ranges::sized_range<R1>
            && std::same_as< std::ranges::range_value_t<R2>, wrl::ComPtr<ID3D12Resource> >
    GpuMappedRes( Core& core, const R1& data, R2& resources, std::size_t duplicateCnt = 1 )
        : GpuMappedRes( core, std::data(data), std::size(data)
            * sizeof(std::ranges::range_value_t<R1>), resources, duplicateCnt
        ) {}
    

    GpuMappedRes( Core& core, UINT64 bytes, wrl::ComPtr<ID3D12Resource>* ppResources,
        std::size_t duplicateCnt = 1
    );
    GpuMappedRes( Core& core, const void* pData, UINT64 bytes,
        wrl::ComPtr<ID3D12Resource>* ppResources, std::size_t duplicateCnt = 1
    );

    template <std::ranges::contiguous_range R>
        requires std::ranges::sized_range<R>
    GpuMappedRes(Core& core, const R& data, std::size_t duplicateCnt = 1)
        : GpuMappedRes(core, std::data(data), std::size(data)
            * sizeof(std::ranges::range_value_t<R>), duplicateCnt
        ) {}

    GpuMappedRes(UploadMemPool& pool, UINT64 bytes, std::size_t duplicateCnt = 1);
    GpuMappedRes(UploadMemPool& pool, D3D12RenderContext& ctx, const void* pData, UINT64 bytes, std::size_t duplicateCnt = 1);

    template <std::ranges::contiguous_range R>
        requires std::ranges::sized_range<R>
    GpuMappedRes(UploadMemPool& pool, const R& data, std::size_t duplicateCnt = 1)
        : GpuMappedRes(pool, std::data(data), std::size(data)
            * sizeof(std::ranges::range_value_t<R>), duplicateCnt
        ) {}

    const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress(std::size_t idx = 0) const {
        return datas_[idx].gpuAddr;
    }

    void upload(const void* pData, std::size_t bytes, std::size_t idx = 0) {
        std::memcpy(datas_[idx].pData, pData, bytes);
    }

    void uploadRegion(const void* pData, std::size_t bytes, std::size_t dstOffset, std::size_t idx = 0) {
        std::memcpy( static_cast<std::uint8_t*>( datas_[idx].pData ) + dstOffset, pData, bytes );
    }

private:
    std::vector<MyPair> datas_;
};

template <std::ranges::range R>
    requires std::same_as< std::ranges::range_value_t<R>, wrl::ComPtr<ID3D12Resource> >
GpuMappedRes::GpuMappedRes( Core& core, UINT64 bytes, R& resources,
    std::size_t duplicateCnt
) {
    auto it = std::begin(resources);
    
    for (std::size_t i = 0; i < duplicateCnt; ++i) {
        MyPair pair;
        *it = createUpBuf(core, bytes);
        auto readRange = D3D12_RANGE{};
        (*it)->Map(0, &readRange, &pair.pData);
        pair.gpuAddr = (*it)->GetGPUVirtualAddress();
        datas_.push_back(std::move(pair));
        ++it;
    }
}

template <std::ranges::range R>
    requires std::same_as< std::ranges::range_value_t<R>, wrl::ComPtr<ID3D12Resource> >
GpuMappedRes::GpuMappedRes( Core& core, const void* pData, UINT64 bytes,
    R& resources, std::size_t duplicateCnt
) {
    auto it = std::begin(resources);
    
    for (std::size_t i = 0; i < duplicateCnt; ++i) {
        MyPair pair;
        *it = createUpBuf(core, pData, bytes);
        auto readRange = D3D12_RANGE{};
        (*it)->Map(0, &readRange, &pair.pData);
        pair.gpuAddr = (*it)->GetGPUVirtualAddress();
        datas_.push_back(std::move(pair));
        ++it;
    }
}

}   // namespace d3d12

} // namespace gfx

#endif // __D3D12Res_HPP