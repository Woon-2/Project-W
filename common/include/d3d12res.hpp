#ifndef __D3D12RES_HPP
#define __D3D12RES_HPP

#include "d3d12core.hpp"

#include <vector>
#include <cstdlib>
#include <ranges>
#include <algorithm>
#include <concepts>

namespace gfx {

namespace d3d12 {

/**
 * @brief Creates a upload buffer which can upload data from the cpu to the gpu in D3D12, with initial data.    
 * @tparam R The type of the range of the data to upload.
 * @param core The D3D12 core object.
 * @param data The range of the initial data.
 * @details It creates the buffer with `CreateCommittedResource` function and the state of the buffer is `D3D12_RESOURCE_STATE_GENERIC_READ`.    
 * The initial data's source memory region is acquired by `std::data`    
 * and the byte width is acquired by `std::size` of the data multiplied by the size of the range value type.    
 * The initial data is copied to the buffer with `ID3D12Resource::Map` and `ID3D12Resource::Unmap`.
 * @see createDefBuf
 */
template <std::ranges::contiguous_range R>
    requires std::ranges::sized_range<R>
wrl::ComPtr<ID3D12Resource> createUpBuf(Core& core, const R& data) {
    return createUpBuf( core, std::data(data), std::size(data)
        * sizeof(std::ranges::range_value_t<R>)
    );
}
/**
 * @brief Creates a default buffer which can be read and written by the gpu in D3D12.
 * @tparam R The type of the range of the initial data.
 * @param core The D3D12 core object.
 * @param ctx The D3D12 render context object.
 * @param data The range of the initial data.
 * @param state The state of the buffer after the initial upload is done.
 * @param pUploadBuf The reference of upload buffer pointer,    
 * this function creates an auxiliary upload buffer for the initial data upload.    
 * The pointer to the upload buffer is stored in this reference.
 * @details It creates the buffer with `CreateCommittedResource` function and the state of the buffer is `D3D12_RESOURCE_STATE_COMMON`.    
 * @note It uploads the initial data from the cpu to the gpu with a auxiliary upload buffer created by createUpBuf.     
 * As the upload buffer's data is copied to the default buffer by `ID3D12GraphicsCommandList::CopyData`,     
 * the default buffer doesn't reach the specified state and have valid data until the command list is executed.
 * @see createUpBuf
 */
template <std::ranges::contiguous_range R>
    requires std::ranges::sized_range<R>
wrl::ComPtr<ID3D12Resource> createDefBuf( Core& core, D3D12RenderContext& ctx, const R& data,
    D3D12_RESOURCE_STATES state, wrl::ComPtr<ID3D12Resource>& pUploadBuf
) {
    return createDefBuf( core, ctx, std::data(data), static_cast<UINT64>( std::size(data)
        * sizeof(std::ranges::range_value_t<R>) ), state, pUploadBuf
    );
}
/**
 * @brief Creates a upload buffer which can upload data from the cpu to the gpu in D3D12.
 * @param core The D3D12 core object.
 * @param bytes The byte width of the buffer.
 * @details It creates the buffer with `CreateCommittedResource` function. and the state of the buffer is `D3D12_RESOURCE_STATE_GENERIC_READ`.
 * @see createDefBuf
 */
wrl::ComPtr<ID3D12Resource> createUpBuf(Core& core, UINT64 bytes);
/**
 * @brief Creates a upload buffer which can upload data from the cpu to the gpu in D3D12, with initial data.
 * @param core The D3D12 core object.
 * @param pData The pointer to the initial data.
 * @param bytes The byte width of the initial data.
 * @details It creates the buffer with `CreateCommittedResource` function. and the state of the buffer is `D3D12_RESOURCE_STATE_GENERIC_READ`.    
 * The initial data is copied to the buffer with `ID3D12Resource::Map` and `ID3D12Resource::Unmap`.
 * @see createUpBuf
 */
wrl::ComPtr<ID3D12Resource> createUpBuf(Core& core, const void* pData, UINT64 bytes);
wrl::ComPtr<ID3D12Resource> createDefBuf(Core& core, UINT64 bytes, D3D12_RESOURCE_STATES state);
/**
 * @brief Creates a default buffer which can be read and written by the gpu in D3D12, copying the initial data from the source buffer.
 * @param core The D3D12 core object.
 * @param ctx The D3D12 render context object.
 * @param pSrcBuf The pointer to the source buffer.
 * @param state The state of the buffer after the copy is done.
 * @details It creates the buffer with `CreateCommittedResource` function. and the state of the buffer is `D3D12_RESOURCE_STATE_COMMON`.
 * @note As the source buffer's data is copied to the default buffer by `ID3D12GraphicsCommandList::CopyData`,    
 * the default buffer doesn't reach the specified state and have valid data until the command list is executed.
 * @see createUpBuf
 */
wrl::ComPtr<ID3D12Resource> createDefBuf(Core& core, D3D12RenderContext& ctx, ID3D12Resource* pSrcBuf, D3D12_RESOURCE_STATES state);
wrl::ComPtr<ID3D12Resource> createDefBuf(Core& core, D3D12RenderContext& ctx, ID3D12Resource* pSrcBuf, UINT64 offset, UINT64 bytes, D3D12_RESOURCE_STATES state);
/**
 * @brief Creates a default buffer which can be read and written by the gpu in D3D12.
 * @param core The D3D12 core object.
 * @param ctx The D3D12 render context object.
 * @param pData The pointer to the initial data.
 * @param bytes The byte width of the initial data.
 * @param state The state of the buffer after the initial upload is done.
 * @param pUploadBuf The reference of upload buffer pointer,    
 * this function creates an auxiliary upload buffer for the initial data upload.    
 * The pointer to the upload buffer is stored in this reference.
 * @details It creates the buffer with `CreateCommittedResource` function. and the state of the buffer is `D3D12_RESOURCE_STATE_COMMON`.     
 * @note It uploads the initial data from the cpu to the gpu with a auxiliary upload buffer created by createUpBuf.     
 * As the upload buffer's data is copied to the default buffer by `ID3D12GraphicsCommandList::CopyData`,     
 * the default buffer doesn't reach the specified state and have valid data until the command list is executed.
 * @see createUpBuf
 */
wrl::ComPtr<ID3D12Resource> createDefBuf(Core& core, D3D12RenderContext& ctx, const void* pData, UINT64 bytes, D3D12_RESOURCE_STATES state, wrl::ComPtr<ID3D12Resource>& pUploadBuf);

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

    const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress(std::size_t idx = 0) const {
        return datas_[idx].gpuAddr;
    }

    void upload(Core& core, const void* pData, UINT bytes, std::size_t idx = 0) {
        std::memcpy(datas_[idx].pData, pData, bytes);
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

#endif // __D3D12RES_HPP