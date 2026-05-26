#ifndef __piercingMeshPipeline_HPP
#define __piercingMeshPipeline_HPP

#include "gfxUtil.hpp"

class RootSig;
struct Mesh;
struct SubMesh;

namespace PiercingMeshPipeline {

struct CameraData {
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
    mu::Vec3   pos;
};

struct FrameData {
    float time = 0.f;
};

struct DrawEvent {
    mu::Mat4x4 world;
    mu::Vec4   tint = { 1.f, 1.f, 1.f, 1.f };
    float      t = 0.f;
    mu::Vec2   custom1 = { 0.f, 0.f };
    mu::Vec2   custom2 = { 0.f, 0.f };
    bool       customDataEnabled = false;
    int        renderOrder = 0;

    const Mesh*    pMesh    = nullptr;
    const SubMesh* pSubMesh = nullptr;

    const Texture* pColorNoiseTex      = nullptr;
    const Texture* pPiercingTex        = nullptr;
    const Texture* pPiercingNoiseTex   = nullptr;
    const Texture* pDistortionNoiseTex = nullptr;
    const Texture* pDistortionMaskTex  = nullptr;
    const Texture* pEmissiveNoiseTex   = nullptr;
    const Texture* pEmissiveMaskTex    = nullptr;
    const Texture* pOpacityMaskTex     = nullptr;

    mu::Vec4 color1        = { 1.f, 1.f, 1.f, 1.f };
    mu::Vec4 color2        = { 1.f, 1.f, 1.f, 1.f };
    mu::Vec4 emissiveColor = { 1.f, 1.f, 1.f, 1.f };

    mu::Vec4 colorNoiseScaleSpeed       = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 piercingNoiseScaleSpeed    = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 distortionNoiseScaleSpeed  = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 emissiveDissolveScaleSpeed = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 distortionMaskST           = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 opacityMaskST              = { 1.f, 1.f, 0.f, 0.f };

    float colorBoost             = 1.f;
    float piercingNoiseIntensity = 0.f;
    float distortionIntensity    = 1.f;
    float emissiveIntensity      = 1.f;
    float opacityBoost           = 1.f;

    auto operator<=>(const DrawEvent& rhs) const noexcept {
        return renderOrder <=> rhs.renderOrder;
    }
};

struct Resources {
    StructuredBuffer    perInstanceData;
    ConstantBufferArray perDrawcallData;
    ConstantBuffer      perFrameData;
};

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

}  // namespace PiercingMeshPipeline

#endif  // __piercingMeshPipeline_HPP
