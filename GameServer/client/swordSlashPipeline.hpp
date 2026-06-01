#ifndef __swordSlashPipeline_HPP
#define __swordSlashPipeline_HPP

#include "gfxUtil.hpp"

class RootSig;
struct Mesh;
struct SubMesh;

namespace SwordSlashPipeline {

struct CameraData {
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
};

struct FrameData {
    float time = 0.f;
};

struct DrawEvent {
    // Per-instance
    mu::Mat4x4     world;
    mu::Vec4       tint         = { 1.f, 1.f, 1.f, 1.f };
    float          t            = 0.f;  // normalized particle age [0, 1]
    mu::Vec2       custom1      = { 0.f, 0.f };
    mu::Vec2       custom2      = { 0.f, 0.f };
    bool           customDataEnabled = false;
    int            renderOrder  = 0;

    // Mesh
    const Mesh*    pMesh    = nullptr;
    const SubMesh* pSubMesh = nullptr;

    // Textures (pFlowTex may be nullptr when flowPower == 0)
    const Texture* pMainTex     = nullptr;
    const Texture* pEmissionTex = nullptr;
    const Texture* pDissolveTex = nullptr;
    const Texture* pFlowTex     = nullptr;

    // FX parameters
    mu::Vec2 speedMainTexUV    = { 0.f, 0.f };
    mu::Vec2 speedDissolveUV   = { 0.f, 0.f };
    mu::Vec2 speedFlow         = { 0.f, 0.f };
    mu::Vec4 mainTexST         = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 emissionTexST     = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 dissolveTexST     = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 flowTexST         = { 1.f, 1.f, 0.f, 0.f };
    float    flowPower         = 0.f;
    float    emission          = 1.f;
    float    desaturation      = 0.f;
    mu::Vec2 remap             = { -2.f, 1.f };
    mu::Vec4 addColor          = { 0.f, 0.f, 0.f, 0.f };
    float    opacity           = 1.f;
    bool     useSmoothDissolve = false;
};

struct Resources {
    StructuredBuffer    perInstanceData;   // t0
    ConstantBufferArray perDrawcallData;   // b0
    ConstantBuffer      perFrameData;      // b1
};

// Draws sword-slash mesh particles using a flow+dissolve+emission alpha-blend pipeline.
// One draw call per DrawEvent (no cross-material batching).
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
        const ComPtr<ID3D12CommandQueue>& cmdQ,
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
    ComPtr<ID3D12CommandQueue>  cmdQ_{};
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

}  // namespace SwordSlashPipeline

#endif  // __swordSlashPipeline_HPP
