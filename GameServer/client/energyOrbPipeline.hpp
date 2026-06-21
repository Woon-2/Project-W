#ifndef __energyOrbPipeline_HPP
#define __energyOrbPipeline_HPP

#include "gfxUtil.hpp"
#include <span>

class RootSig;
struct Mesh;
struct SubMesh;

namespace EnergyOrbShader {
    struct PerInstanceData;
}

// Renders energy orbs as additive, self-lit point sprites into SceneColorHDR
// (before the bloom pass). One draw call per orb (one submesh's vertices).
//
// M1: static point cloud from the bound Position VB, GS-expanded to camera-facing
// quads with an HDR color. M2 adds death-pose skinning + vertex->sphere morph.
namespace EnergyOrbPipeline {

// GPU resource capacity: max orbs (= submesh drawcalls) rendered in one frame. Each
// orb consumes one per-instance entry + one per-drawcall CB slot, so the fixed-size
// perInstanceData / perDrawcallData buffers (sized to this in gfx.cpp) bound how many
// orbs can be drawn. Excess orbs in a frame are dropped (logged) rather than overrun
// the buffers. Keep large enough to cover bursts of simultaneous deaths.
inline constexpr u32t kMaxOrbDrawcalls = 512u;

struct CameraData {
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
    mu::Vec3   pos;   // world-space camera position (for GS billboarding)
};

struct FrameData {
    mu::Mat4x4 vp;
    mu::Vec3   cameraPosW;
};

struct DrawEvent {
    mu::Mat4x4  world;                       // death-time object world
    const Mesh*    pMesh    = nullptr;       // source mesh (Position/BoneIndices/BoneWeights/UV VBs)
    const SubMesh* pSubMesh = nullptr;       // submesh whose indices select this orb's vertices
    const Texture* pAlbedo  = nullptr;       // submesh albedo (morph start color); null = grey
    std::span<const mu::Mat4x4> boneXforms;  // death-pose bone snapshot (owned by EnergyOrbSystem)
    mu::Vec3    sphereCenter = { 0.f, 0.f, 0.f };
    float       sphereRadius = 0.5f;
    mu::Vec3    colorHDR     = { 1.f, 1.f, 1.f };
    float       morphT       = 0.f;          // 0 = mesh pose, 1 = collapsed sphere
    float       pointSize    = 0.05f;        // world-unit half-size of each particle quad
    u32t        vertexCount  = 0u;           // submesh vertex count (DrawInstanced count basis)
    int         renderOrder  = 0;
};

struct Resources {
    StructuredBuffer    perInstanceData;   // t0
    StructuredBuffer    boneData;          // t2 (death-pose bone palette)
    ConstantBufferArray perDrawcallData;   // b0
    ConstantBuffer      perFrameData;      // b1
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
    DescriptorPool* pTexPool_      = nullptr;
    DescriptorPool* pTexArrayPool_ = nullptr;
    DescriptorPool* pTexCubePool_  = nullptr;
    DescriptorPool* pSamPool_      = nullptr;
    DescriptorPool* pCmpSamPool_   = nullptr;
    std::shared_ptr<RootSig> rootSig_ = nullptr;
    ComPtr<ID3D12PipelineState> shader_{};
    ComPtr<ID3D12CommandQueue>  cmdQ_{};
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT     scissorRect_{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_{};
    Fence*           pFence_      = nullptr;
    Resources*       pResources_  = nullptr;
    ThreadPool*      threadPool_  = nullptr;
    CommandListPool* cmdListPool_ = nullptr;
    std::vector<DrawEvent> drawEvents_{};
    CameraData cameraData_{};
    FrameData  frameData_{};
    std::size_t roomIdx_{};

    UINT rootParamIdxPDD_{};
    UINT rootParamIdxPID_{};
    UINT rootParamIdxPFD_{};
    UINT rootParamIdxBoneData_{};
    UINT rootParamIdxTexPool_{};
    UINT rootParamIdxTexArrayPool_{};
    UINT rootParamIdxTexCubePool_{};
    UINT rootParamIdxSamPool_{};
    UINT rootParamIdxCmpSamPool_{};
};

}  // namespace EnergyOrbPipeline

#endif  // __energyOrbPipeline_HPP
