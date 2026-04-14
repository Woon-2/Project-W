#ifndef __smokeBlendCGPipeline_HPP
#define __smokeBlendCGPipeline_HPP

#include "gfxUtil.hpp"

class RootSig;

namespace SmokeBlendCGPipeline {

struct CameraData {
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
    mu::Vec3   pos;
};

struct FrameData {
    float time       = 0.f;
    float cameraNear = 0.1f;
    float cameraFar  = 500.f;
};

struct DrawEvent {
    mu::Mat4x4 world;
    mu::Vec4   tint = { 1.f, 1.f, 1.f, 1.f };
    float      rotation = 0.f;
    mu::Vec4   stretchAxisAndMode = { 0.f, 0.f, 0.f, 0.f }; // xyz=world axis, w=1 when stretched
    int        renderOrder = 0;

    const Texture* pMainTex = nullptr;
    const Texture* pNoiseTex = nullptr;
    const Texture* pFlowTex = nullptr;
    const Texture* pMaskTex = nullptr;
    const Texture* pCameraDepthTex = nullptr;

    mu::Vec2 uvOffset = { 0.f, 0.f };
    mu::Vec2 uvScale = { 1.f, 1.f };

    mu::Vec4 mainTexST = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 noiseTexST = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 flowTexST = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 maskTexST = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 speedMainTexUVNoiseZW = { 0.f, 0.f, 0.f, 0.f };
    mu::Vec4 distortionSpeedXYPowerZ = { 0.f, 0.f, 0.f, 0.f };
    mu::Vec4 color = { 1.f, 1.f, 1.f, 1.f };

    float emission = 2.f;
    float opacity = 1.f;
    float textureOpacity = 0.f;
    float multiplyTexture = 1.f;
    float useOnlyColor = 0.f;
    float useFresnel = 0.f;
    float fresnelPower = 3.f;
    float fresnelScale = 3.f;
    float useCenterGlow = 0.f;
    float useDepth = 1.f;
    float depthPower = 0.5f;

    auto operator<=>(const DrawEvent& rhs) const noexcept {
        return renderOrder <=> rhs.renderOrder;
    }
};

struct Resources {
    StructuredBuffer    perInstanceData;  // t0
    ConstantBufferArray perDrawcallData;  // b0
    ConstantBuffer      perFrameData;     // b1
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
    void drawMultiThreaded() { drawSingleThreaded(); }

private:
    std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
    DescriptorPool* pTexPool_ = nullptr;
    DescriptorPool* pTexArrayPool_ = nullptr;
    DescriptorPool* pTexCubePool_ = nullptr;
    DescriptorPool* pSamPool_ = nullptr;
    DescriptorPool* pCmpSamPool_ = nullptr;
    std::shared_ptr<RootSig> rootSig_ = nullptr;
    ComPtr<ID3D12PipelineState> shader_{};
    ComPtr<ID3D12CommandQueue> cmdQ_{};
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_{};
    Fence* pFence_ = nullptr;
    Resources* pResources_ = nullptr;
    ThreadPool* threadPool_ = nullptr;
    CommandListPool* cmdListPool_ = nullptr;
    std::vector<DrawEvent> drawEvents_{};
    CameraData cameraData_{};
    FrameData frameData_{};
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

}  // namespace SmokeBlendCGPipeline

#endif  // __smokeBlendCGPipeline_HPP
