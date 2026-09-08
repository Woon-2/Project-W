#ifndef __OutlinePipeline_HPP
#define __OutlinePipeline_HPP

#include "gfxUtil.hpp"

class RootSig;
struct Mesh;
struct SubMesh;

namespace OutlineShader {
struct PerInstanceData;
}

// Inverted-hull silhouette pass for interaction highlighting.
// Draw counts are tiny (at most the one aimed object per frame), so this
// pipeline is deliberately single-threaded and is excluded from Hi-Z culling.
// It renders into SceneColorHDR before bloom, so an HDR color makes the rim glow.
// See client/docs/graphicsArchitecture.md.
namespace OutlinePipeline {

struct CameraData {
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
};

struct FrameData {
    float screenWidth  = 1.f;
    float screenHeight = 1.f;
};

struct DrawEvent {
    mu::Mat4x4     world;
    const Mesh*    pMesh    = nullptr;
    const SubMesh* pSubMesh = nullptr;
    mu::Vec4       color{ 1.f, 0.85f, 0.35f, 1.f };   // HDR: scale > 1 to feed bloom
    float          thicknessPx = 3.f;
};

struct Resources {
    StructuredBuffer    perInstanceData;   // t0
    ConstantBufferArray perDrawcallData;   // b0
    ConstantBuffer      perFrameData;      // b1
};

class Dispatcher {
public:
    Dispatcher() = default;
    Dispatcher(
        const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
        const std::shared_ptr<RootSig>& rootSig,
        const ComPtr<ID3D12PipelineState>& shader,
        RenderSubmitter* submitter,
        const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv,
        Fence* pFence, Resources* pResources,
        CommandListPool* commandListPool,
        std::vector<DrawEvent>&& drawEvents,
        const CameraData& cameraData, const FrameData& frameData,
        std::size_t roomIdx
    );

    void updateGPUDataSingleThreaded();
    void drawSingleThreaded();

private:
    std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
    std::shared_ptr<RootSig> rootSig_ = nullptr;
    ComPtr<ID3D12PipelineState> shader_ = nullptr;
    RenderSubmitter* submitter_ = nullptr;
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_{};
    Fence* pFence_{};
    Resources* pResources_ = nullptr;
    CommandListPool* cmdListPool_ = nullptr;
    std::vector<DrawEvent> drawEvents_{};
    CameraData cameraData_{};
    FrameData frameData_{};
    std::size_t roomIdx_{};

    UINT rootParamIdxPDD_{};
    UINT rootParamIdxPID_{};
    UINT rootParamIdxPFD_{};
};

}   // namespace OutlinePipeline

#endif   // __OutlinePipeline_HPP
