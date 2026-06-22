#ifndef __meshParticlePipeline_HPP
#define __meshParticlePipeline_HPP

#include "gfxUtil.hpp"

class RootSig;
struct Mesh;
struct SubMesh;

namespace MeshParticleShader {
    struct PerInstanceData;
}

namespace MeshParticlePipeline {

struct CameraData {
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
};

struct FrameData {
    mu::Mat4x4 vp;
};

struct DrawEvent {
    mu::Mat4x4     world;
    const Mesh*    pMesh    = nullptr;
    const SubMesh* pSubMesh = nullptr;
    const Texture* pTex     = nullptr;
    mu::Vec4       tint     = { 1.f, 1.f, 1.f, 1.f };
    // Sprite-sheet (flipbook) frame UV. Defaults select the full texture, so
    // effects without TextureSheetAnimation are unaffected.
    mu::Vec2       uvOffset = { 0.f, 0.f };
    mu::Vec2       uvScale  = { 1.f, 1.f };
    int            renderOrder = 0;
};

struct Resources {
    StructuredBuffer    perInstanceData;   // t0
    ConstantBufferArray perDrawcallData;   // b0
    ConstantBuffer      perFrameData;      // b1
};

// Draws mesh particles using an unlit alpha-blend pipeline.
// One draw call per DrawEvent (no cross-mesh batching).
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
        D3D12_CPU_DESCRIPTOR_HANDLE dsv,
        Fence* pFence,
        Resources* pResources,
        ThreadPool* threadPool,
        CommandListPool* commandListPool,
        std::vector<DrawEvent>&& drawEvents,
        const CameraData& cameraData,
        const FrameData& frameData,
        std::size_t roomIdx
    );

    void updateGPUDataSingleThreaded();
    void drawSingleThreaded();

    // Multithreaded variants — delegate to single-threaded for now.
    void updateGPUDataMultiThreaded() { updateGPUDataSingleThreaded(); }
    void drawMultiThreaded()          { drawSingleThreaded(); }

private:
    std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
    DescriptorPool* pTexPool_        = nullptr;
    DescriptorPool* pTexArrayPool_   = nullptr;
    DescriptorPool* pTexCubePool_    = nullptr;
    DescriptorPool* pSamPool_        = nullptr;
    DescriptorPool* pCmpSamPool_     = nullptr;
    std::shared_ptr<RootSig> rootSig_ = nullptr;
    ComPtr<ID3D12PipelineState> shader_{};
    RenderSubmitter* submitter_{};
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT     scissorRect_{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_{};
    Fence*          pFence_     = nullptr;
    Resources*      pResources_ = nullptr;
    ThreadPool*     threadPool_ = nullptr;
    CommandListPool* cmdListPool_ = nullptr;
    std::vector<DrawEvent> drawEvents_{};
    CameraData cameraData_{};
    FrameData  frameData_{};
    std::size_t roomIdx_{};

    UINT rootParamIdxPDD_{};
    UINT rootParamIdxPID_{};
    UINT rootParamIdxPFD_{};
    UINT rootParamIdxTexPool_{};
    UINT rootParamIdxTexArrayPool_{};
    UINT rootParamIdxTexCubePool_{};
    UINT rootParamIdxSamPool_{};
    UINT rootParamIdxCmpSamPool_{};
};

}  // namespace MeshParticlePipeline

#endif  // __meshParticlePipeline_HPP
