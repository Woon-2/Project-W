#ifndef __minimapPropPipeline_HPP
#define __minimapPropPipeline_HPP

#include "gfxUtil.hpp"

class RootSig;
struct Mesh;
struct SubMesh;
struct Material;

namespace MinimapPropPipeline {

// Upper bound on prop part-draws baked into the minimap in one re-bake. Only BVH props
// (trees/rocks/mesh landmarks) are submitted (foliage/grass is skipped), so this stays
// bounded; instances beyond the cap are dropped (acceptable: map edge).
constexpr u32t kMaxDrawEvents = 4096u;

struct DrawEvent {
    const Mesh*     mesh     = nullptr;
    const SubMesh*  subMesh  = nullptr;
    const Material* material = nullptr;
    mu::Mat4x4      world    = {};   // part.meshXform * instance.world
};

struct Resources {
    ConstantBufferArray perDrawcallData;   // b0
};

// Ensures the 2-slot (Position, UV) VBV cache exists in mesh.vbViewsByPipeline.
void layoutMeshIfNeeded(const Mesh& mesh);

// Dispatcher for the minimap scatter-prop bake. Renders each part top-down (albedo +
// alpha-cutout, no lighting) into the same RT/camera the terrain pass used. Single pass,
// single-threaded; invoked only on a minimap re-bake (chunk-set change), never per frame.
class Dispatcher {
public:
    Dispatcher() = default;
    Dispatcher(
        const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
        DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
        DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
        DescriptorPool* pCmpSamPool,
        const std::shared_ptr<RootSig>& rootSig,
        const ComPtr<ID3D12PipelineState>& shader,
        RenderSubmitter* submitter,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE rtv,
        Fence* pFence,
        Resources* pResources,
        CommandListPool* commandListPool,
        std::vector<DrawEvent>&& drawEvents,
        const mu::Mat4x4& viewProj,
        std::size_t roomIdx
    );

    // Stages per-drawcall constant buffers and records every draw call. No-op if empty.
    // Caller leaves the RT in RENDER_TARGET (the terrain pass already cleared/transitioned).
    void render();

private:
    std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
    DescriptorPool* pTexPool_      = nullptr;
    DescriptorPool* pTexArrayPool_ = nullptr;
    DescriptorPool* pTexCubePool_  = nullptr;
    DescriptorPool* pSamPool_      = nullptr;
    DescriptorPool* pCmpSamPool_   = nullptr;
    std::shared_ptr<RootSig>    rootSig_ = nullptr;
    ComPtr<ID3D12PipelineState> shader_  = nullptr;
    RenderSubmitter* submitter_    = nullptr;
    D3D12_VIEWPORT              viewport_{};
    D3D12_RECT                  scissorRect_{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_{};
    Fence*            pFence_      = nullptr;
    Resources*        pResources_  = nullptr;
    CommandListPool*  cmdListPool_ = nullptr;
    std::vector<DrawEvent> drawEvents_{};
    mu::Mat4x4        viewProj_{};
    std::size_t       roomIdx_{};

    UINT rootParamIdxPDD_{};
    UINT rootParamIdxTexPool_{};
    UINT rootParamIdxTexArrayPool_{};
    UINT rootParamIdxTexCubePool_{};
    UINT rootParamIdxSamPool_{};
    UINT rootParamIdxCmpSamPool_{};
};

}   // namespace MinimapPropPipeline

#endif  // __minimapPropPipeline_HPP
