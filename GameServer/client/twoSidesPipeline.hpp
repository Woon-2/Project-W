#ifndef __twoSidesPipeline_HPP
#define __twoSidesPipeline_HPP

#include "gfxUtil.hpp"

class RootSig;
struct Mesh;
struct SubMesh;

namespace TwoSidesPipeline {

struct CameraData {
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
    mu::Vec3   pos;
};

struct FrameData {
    float time = 0.f;
};

struct DrawEvent {
    // Per-instance
    mu::Mat4x4     world;
    mu::Vec4       tint         = { 1.f, 1.f, 1.f, 1.f };
    float          t            = 0.f;
    mu::Vec2       custom1      = { 0.f, 0.f };
    mu::Vec2       custom2      = { 0.f, 0.f };
    bool           customDataEnabled = false;
    int            renderOrder  = 0;

    // Mesh
    const Mesh*    pMesh    = nullptr;
    const SubMesh* pSubMesh = nullptr;

    // Textures (pNoiseTex may be nullptr when noise distortion is inactive)
    const Texture* pMainTex  = nullptr;
    const Texture* pMaskTex  = nullptr;
    const Texture* pNoiseTex = nullptr;

    // FX parameters
    mu::Vec4 mainTexST   = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 maskTexST   = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 noiseTexST  = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 texSpeed    = { 0.f, 0.f, 0.f, 0.f };  // xy=main UV, zw=noise UV
    float    emission    = 1.f;
    float    opacity     = 1.f;
    float    useFresnel        = 0.f;
    float    fresnelPower      = 2.f;
    mu::Vec4 frontFacesColor   = { 1.f, 1.f, 1.f, 1.f };
    mu::Vec4 backFacesColor    = { 1.f, 1.f, 1.f, 1.f };
    mu::Vec4 fresnelColor      = { 1.f, 1.f, 1.f, 1.f };
    float    fresnelEmission   = 1.f;
    float    useBackFresnel    = 0.f;
    float    backFresnel       = -4.f;
    float    backFresnelEmission = 1.f;
    mu::Vec4 backFresnelColor  = { 1.f, 1.f, 1.f, 1.f };
};

struct Resources {
    StructuredBuffer    perInstanceData;   // t0
    ConstantBufferArray perDrawcallData;   // b0
    ConstantBuffer      perFrameData;      // b1
};

// Draws two-sided mesh particles using a mask+noise alpha-blend pipeline.
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
    Fence*           pFence_     = nullptr;
    Resources*       pResources_ = nullptr;
    ThreadPool*      threadPool_ = nullptr;
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

}  // namespace TwoSidesPipeline

#endif  // __twoSidesPipeline_HPP
