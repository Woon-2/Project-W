#ifndef __minimapTerrainPipeline_HPP
#define __minimapTerrainPipeline_HPP

#include "gfxUtil.hpp"
#include "terrain.hpp"

class RootSig;
struct Mesh;

namespace MinimapTerrainPipeline {

// Upper bound on terrain chunks drawn into the minimap cache in one re-bake
// (maxHop streaming radius keeps the loaded set well under this).
constexpr u32t kMaxDrawEvents = 96u;

struct CameraData {
    mu::Mat4x4 view;   // North-up lookAt centered on the player
    mu::Mat4x4 proj;   // Orthographic, half-extent = GFX::kMinimapWorldRadius
};

struct DrawEvent {
    const TerrainData* terrain = nullptr;
    mu::Mat4x4          world  = {};   // world transform; default is identity
};

struct Resources {
    ConstantBufferArray perDrawcallData;   // b0
};

// Ensures the 2-slot (Position, UV) VBV cache exists in mesh.vbViewsByPipeline.
void layoutMeshIfNeeded(const Mesh& mesh);

// Dispatcher for the lightweight minimap terrain cache pass. Single-threaded,
// single render pass (no shadow/occluder sub-passes) — re-baked only when the
// terrain chunk set changes, never every frame.
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
        const CameraData& cameraData,
        std::size_t roomIdx
    );

    // Stages per-drawcall constant buffers and records every draw call. No-op if
    // drawEvents is empty. Caller is responsible for the RTV clear/transition.
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
    CameraData        cameraData_{};
    std::size_t       roomIdx_{};

    UINT rootParamIdxPDD_{};
    UINT rootParamIdxTexPool_{};
    UINT rootParamIdxTexArrayPool_{};
    UINT rootParamIdxTexCubePool_{};
    UINT rootParamIdxSamPool_{};
    UINT rootParamIdxCmpSamPool_{};
};

}   // namespace MinimapTerrainPipeline

#endif  // __minimapTerrainPipeline_HPP
