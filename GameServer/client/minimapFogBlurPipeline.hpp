#ifndef __minimapFogBlurPipeline_HPP
#define __minimapFogBlurPipeline_HPP

#include "gfxUtil.hpp"

class RootSig;

namespace MinimapFogBlurPipeline {

// 2 passes (horizontal, vertical+composite) per re-bake.
constexpr u32t kPassCount = 2u;

// GPU-side resources for the blur pass. One constant buffer element per pass (b0).
struct Resources {
    ConstantBufferArray perDrawcallData;   // kPassCount elements (x roomCnt)
};

// Dispatcher for the minimap fog-of-war 2-pass separable box blur. Reads the raw
// minimap terrain cache (SharedResources::Minimap texA), writes the intermediate
// horizontal-blur result to texB, then blurs+composites texB back into texA — the
// same fullscreen-triangle ping-pong idiom as BloomPipeline. Only invoked when the
// minimap cache is re-baked (chunk load/unload), never every frame.
class Dispatcher {
public:
    Dispatcher() = default;
    Dispatcher(
        const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
        DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
        DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
        DescriptorPool* pCmpSamPool,
        const std::shared_ptr<RootSig>& rootSig,
        const ComPtr<ID3D12PipelineState>& blurShader,
        const ComPtr<ID3D12CommandQueue>& cmdQ,
        Fence* pFence,
        Resources* pResources,
        CommandListPool* commandListPool,
        std::size_t roomIdx,
        float blurRadiusTexels
    );

    // Records both blur passes and submits. Leaves the final composited result in
    // SharedResources::Minimap::minimapData[roomIdx].texA, in PIXEL_SHADER_RESOURCE state.
    void render();

private:
    std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
    DescriptorPool* pTexPool_      = nullptr;
    DescriptorPool* pTexArrayPool_ = nullptr;
    DescriptorPool* pTexCubePool_  = nullptr;
    DescriptorPool* pSamPool_      = nullptr;
    DescriptorPool* pCmpSamPool_   = nullptr;
    std::shared_ptr<RootSig> rootSig_ = nullptr;
    ComPtr<ID3D12PipelineState> blurShader_ = nullptr;
    ComPtr<ID3D12CommandQueue>  cmdQ_ = nullptr;
    Fence* pFence_ = nullptr;
    Resources* pResources_ = nullptr;
    CommandListPool* cmdListPool_ = nullptr;
    std::size_t roomIdx_{};
    float blurRadiusTexels_ = 8.f;

    UINT rootParamIdxPDD_{};
    UINT rootParamIdxTexPool_{};
    UINT rootParamIdxTexArrayPool_{};
    UINT rootParamIdxTexCubePool_{};
    UINT rootParamIdxSamPool_{};
    UINT rootParamIdxCmpSamPool_{};
};

}   // namespace MinimapFogBlurPipeline

#endif  // __minimapFogBlurPipeline_HPP
