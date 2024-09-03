#include "d3d12res.hpp"

namespace gfx {

namespace d3d12 {

GpuMappedRes::GpuMappedRes( Core& core, UINT64 bytes, wrl::ComPtr<ID3D12Resource>* ppResources,
    std::size_t duplicateCnt
) : datas_() {
    for (std::size_t i = 0; i < duplicateCnt; ++i) {
        auto pair = MyPair{};
        ppResources[i] = createUpBuf(core, bytes);
        auto readRange = D3D12_RANGE{};
        ppResources[i]->Map(0, &readRange, &pair.pData);
        pair.gpuAddr = ppResources[i]->GetGPUVirtualAddress();
        datas_.push_back(std::move(pair));
    }
}

GpuMappedRes::GpuMappedRes( Core& core, const void* pData, UINT64 bytes,
    wrl::ComPtr<ID3D12Resource>* ppResources, std::size_t duplicateCnt
) : datas_() {
    for (std::size_t i = 0; i < duplicateCnt; ++i) {
        auto pair = MyPair{};
        ppResources[i] = createUpBuf(core, pData, bytes);
        auto readRange = D3D12_RANGE{};
        ppResources[i]->Map(0, &readRange, &pair.pData);
        pair.gpuAddr = ppResources[i]->GetGPUVirtualAddress();
        datas_.push_back(std::move(pair));
    }
}

GpuMappedRes::GpuMappedRes(UploadMemPool& pool, UINT64 bytes, std::size_t duplicateCnt) {
    for (std::size_t i = 0; i < duplicateCnt; ++i) {
        auto pair = MyPair{};
        pair.gpuAddr = pool.allocate(bytes);
        if (pair.gpuAddr == 0) {
            throw std::runtime_error("Failed to allocate memory");
        }
        pair.pData = pool.map(pair.gpuAddr);
        datas_.push_back(std::move(pair));
    }
}

GpuMappedRes::GpuMappedRes( UploadMemPool& pool, D3D12RenderContext& ctx, const void* pData,
    UINT64 bytes, std::size_t duplicateCnt
) {
    for (std::size_t i = 0; i < duplicateCnt; ++i) {
        auto pair = MyPair{};
        pair.gpuAddr = pool.allocate(bytes);
        if (pair.gpuAddr == 0) {
            throw std::runtime_error("Failed to allocate memory");
        }
        pool.construct_at(ctx, pair.gpuAddr, pData, bytes);
        pair.pData = pool.map(pair.gpuAddr);
        datas_.push_back(std::move(pair));
    }
}

}   // namespace d3d12

}   // namespace gfx