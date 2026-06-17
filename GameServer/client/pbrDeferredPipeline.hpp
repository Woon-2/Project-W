#ifndef __pbrDeferredPipeline_HPP
#define __pbrDeferredPipeline_HPP

#include "gfxUtil.hpp"
#include "sharedResources.hpp"

class RootSig;
class CmdSig;

struct Mesh;
struct SubMesh;
struct Material;

namespace ShadowMapCSMShader {
    struct PerInstanceData;
}

namespace PBRDeferredGBufferShader {
    struct PerInstanceData;
}

namespace PBRDeferredPipeline {

// Same light/camera/frame data structs as PBRPipeline
struct LightData {
    enum class Type {
        PointLight,
        Spotlight,
        DirectionalLight
    };
    mu::Vec3 pos;
    mu::Vec3 dir;
    mu::Vec3 color;
    float intensity;
    float cosTheta;
    float cosPhi;
    float falloff;
    mu::Vec3 atten;
    Type type;
    bool isMainDirectionalLight;
    std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeViews  = {};
    std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeProjs  = {};
    XMFLOAT4 cascadeSplitsFarV = {};
    u32t cascadeCount = MAX_CSM_CASCADES;
    std::array<float, MAX_CSM_CASCADES> cascadeNormalOffsets = {};
    mu::Vec3 cascadeCameraPos = {};  // camera eye for camera-relative cascade space (caster/receiver rebase)
};

struct CameraData {
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
    mu::Vec3 pos;
};

struct FrameData {
    mu::Vec3 globalAmbient;
};

struct DrawEvent {
    mu::Mat4x4   world;
    const Mesh*    mesh;
    const SubMesh* subMesh;
    const Material* material;
    bool viewFrustumCulled = false;
    bool shadowCulled      = false;
    // When set, this event is routed through the GPU Hi-Z occlusion-cull + indirect
    // draw path instead of the direct GBuffer draw. Scatter props with a BVH set this
    // (after passing per-instance VFC) when Hi-Z culling is enabled. Excluded from sort.
    bool occludeeCandidate = false;

    auto operator<=>(const DrawEvent& rhs) const noexcept {
        auto e = mesh <=> rhs.mesh;
        if (e == std::strong_ordering::equal) {
            auto e2 = subMesh <=> rhs.subMesh;
            if (e2 == std::strong_ordering::equal)
                return material <=> rhs.material;
            return e2;
        }
        return e;
    }
};

// A near-camera static prop instance drawn (depth-only, position-only) into the shared
// Hi-Z source depth so it occludes other Hi-Z candidates. Reuses HiZOccluderShader.
struct OccluderInfo {
    const Mesh*    mesh    = nullptr;
    const SubMesh* subMesh = nullptr;
    mu::Mat4x4     world   = {};
};

struct Resources {
    // Hi-Z occluder pass: near-camera collidable props rendered depth-only into the
    // shared Hi-Z source depth (alongside terrain occluders).
    struct OccluderPass {
        StructuredBuffer    perInstanceData;   // t0 (HiZOccluderShader::PerInstanceData)
        ConstantBufferArray perDrawcallData;   // b0
    } occluderPass;

    // Hi-Z occlusion-cull + indirect draw for occludee-candidate events (BVH props).
    // Reuses the same compute shaders / command signature as the skinned pipeline.
    // No CPU visibility feedback ring: static props need no anim/physics skip, so the
    // cull shader's per-instance feedback write lands in a throwaway scratch buffer.
    struct HiZPass {
        static constexpr u32t MAX_HIZ_INSTANCES = 65'536u;

        StructuredBuffer perInstanceDataCull;      // t0
        StructuredBuffer perInstanceDataCompact;   // t0
        ConstantBuffer   perFrameDataClear;        // b1
        ConstantBuffer   perFrameDataCull;         // b1
        ConstantBuffer   perFrameDataCompact;      // b1
        ConstantBuffer   perFrameDataCommand;      // b1
        RWStructuredBuffer perGroupCnt;            // t3, u1
        RWStructuredBuffer groupOffsets;           // t3, u1
        RWStructuredBuffer visibleFlags;           // t4, u2
        RWStructuredBuffer perGroupData;           // t5, u3
        RWStructuredBuffer visibleIndices;         // u1 (compact) / t3 (indirect draw SRV)
        RWStructuredBuffer indirectCmd;            // u0, space1
        RWStructuredBuffer cullScratch;            // u3 (cull feedback sink, no readback)

        // GBuffer instance/drawcall data for the indirect draw (occludee events only;
        // disjoint from gBufferPass which serves the direct, non-occludee draws).
        StructuredBuffer    gBufferPerInstanceData;   // t0
        ConstantBufferArray gBufferPerDrawcallData;   // b0
    } hiZPass;

    struct ShadowPass {
        StructuredBuffer    perInstanceData;   // t0
        ConstantBufferArray perDrawcallData;   // b0 (opaque casters)
        // b0 for alpha-tested (foliage) casters: carries bindless albedo + cutoff.
        // Indexed by the same per-group slot as perDrawcallData (sparse; a group uses
        // exactly one of the two arrays depending on its material's cAlphaCutoff).
        ConstantBufferArray perDrawcallDataMasked;
        ConstantBufferArray perFrameData;      // b1: cascade-per-slot
    } shadowPass;

    struct GBufferPass {
        StructuredBuffer    perInstanceData;   // t0
        StructuredBuffer    lightData;         // t1 (bound but unused in PSMain)
        ConstantBufferArray perDrawcallData;   // b0
        ConstantBuffer      perFrameData;      // b1
    } gBufferPass;
};

// Prepare the mesh's VBV layout for the deferred GBuffer pipeline if not already done.
// Shadow slot: "PBRDeferredPipeline_Shadow" (position only)
// GBuffer slot: "PBRDeferredPipeline_GBuffer" (position, normal, tangent, bitangent, uv)
void layoutMeshIfNeeded(const Mesh& mesh);

// Dispatcher for PBRDeferredPipeline.
// 2-pass: shadow pass + gBuffer pass.
// Lighting pass is handled separately in GFX (fullscreen triangle).
class Dispatcher {
public:
    Dispatcher() = default;
    Dispatcher(
        const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
        DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
        DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
        DescriptorPool* pCmpSamPool, DescriptorPool* pDsvPool,
        const std::shared_ptr<RootSig>& rootSig,
        const std::shared_ptr<CmdSig>& cmdSig,
        const ComPtr<ID3D12PipelineState>& hiZClearShader,
        const ComPtr<ID3D12PipelineState>& hiZCullShader,
        const ComPtr<ID3D12PipelineState>& prefixSumShader,
        const ComPtr<ID3D12PipelineState>& hiZCompactShader,
        const ComPtr<ID3D12PipelineState>& hiZCommandShader,
        const ComPtr<ID3D12PipelineState>& occluderShader,
        const ComPtr<ID3D12PipelineState>& gBufferShader,
        const ComPtr<ID3D12PipelineState>& indirectGBufferShader,
        const ComPtr<ID3D12PipelineState>& shadowShader,
        const ComPtr<ID3D12PipelineState>& shadowMaskedShader,
        const ComPtr<ID3D12CommandQueue>& cmdQ,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissorRect,
        Fence* pFence,
        Resources* pResources, ThreadPool* threadPool,
        CommandListPool* commandListPool,
        std::vector<DrawEvent>&& drawEvents,
        std::vector<OccluderInfo>&& occluderInfos,
        std::vector<LightData>&& lightData,
        const LightData& mainDirectionalLightData,
        const CameraData& cameraData, const FrameData& frameData,
        std::size_t roomIdx
    );

    void sortDrawEvents();
    void occluderPass();
    void hiZPass();
    void shadowPass();
    void shadowPassMT();
    void gBufferPass();
    void gBufferPassMT();
    void gBufferIndirectPass();
    void gBufferIndirectPassMT();

private:
    void occluderUpdate();
    void occluderDraw();

    void hiZPassUpdate();
    void hiZPassCompute();

    void shadowUpdate();
    void shadowUpdateMT();
    void shadowDraw();
    void shadowDrawMT();

    void gBufferUpdate();
    void gBufferUpdateMT();
    void gBufferDraw();
    void gBufferDrawMT();

    void gBufferIndirectUpdate();
    void gBufferIndirectDraw();

    // Stage gBuffer perFrameData + lightData (shared by the direct and indirect draws,
    // so it is staged even when one of the two event sets is empty).
    void stageGBufferFrameAndLights();

    void MU_CALLCONV addJobGBufferUpdate( mu::Mat4x4 view, const mu::Mat4x4& viewProj,
        const DrawEvent* pFirst, const DrawEvent* pLast,
        PBRDeferredGBufferShader::PerInstanceData* pOut,
        std::latch& latch
    );
    void addJobGBufferDraw( ID3D12GraphicsCommandList* threadCmdList,
        const std::vector<DrawEvent>::const_iterator* pItFirst,
        const std::vector<DrawEvent>::const_iterator* pItLast,
        std::size_t firstDrawcallIdx, std::latch& latch
    );
    void addJobShadowUpdate( const DrawEvent* pFirst,
        const DrawEvent* pLast, ShadowMapCSMShader::PerInstanceData* pOut,
        std::latch& latch
    );
    void addJobShadowDraw( ID3D12GraphicsCommandList* threadCmdList,
        const std::vector<DrawEvent>::const_iterator* pItFirst,
        const std::vector<DrawEvent>::const_iterator* pItLast,
        std::size_t firstDrawcallIdx,
        const CSMShadowMapData::CascadeSlice& cascadeSlice, u32t cascadeIdx,
        std::latch& latch
    );

    std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
    DescriptorPool* pTexPool_     = nullptr;
    DescriptorPool* pTexArrayPool_ = nullptr;
    DescriptorPool* pTexCubePool_ = nullptr;
    DescriptorPool* pSamPool_     = nullptr;
    DescriptorPool* pCmpSamPool_  = nullptr;
    DescriptorPool* pDsvPool_     = nullptr;
    std::shared_ptr<RootSig> rootSig_ = nullptr;
    std::shared_ptr<CmdSig>  cmdSig_  = nullptr;
    ComPtr<ID3D12PipelineState> hiZClearShader_   = nullptr;
    ComPtr<ID3D12PipelineState> hiZCullShader_    = nullptr;
    ComPtr<ID3D12PipelineState> prefixSumShader_  = nullptr;
    ComPtr<ID3D12PipelineState> hiZCompactShader_ = nullptr;
    ComPtr<ID3D12PipelineState> hiZCommandShader_ = nullptr;
    ComPtr<ID3D12PipelineState> occluderShader_   = nullptr;
    ComPtr<ID3D12PipelineState> gBufferShader_ = nullptr;
    ComPtr<ID3D12PipelineState> indirectGBufferShader_ = nullptr;
    ComPtr<ID3D12PipelineState> shadowShader_  = nullptr;
    ComPtr<ID3D12PipelineState> shadowMaskedShader_ = nullptr;  // foliage alpha-test shadow PSO
    ComPtr<ID3D12CommandQueue>  cmdQ_          = nullptr;
    D3D12_VIEWPORT    viewport_{};
    D3D12_RECT        scissorRect_{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvGB_[5]{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvGB_{};
    Fence* pFence_ = nullptr;
    ThreadPool*       threadPool_   = nullptr;
    CommandListPool*  cmdListPool_  = nullptr;
    Resources*        pResources_   = nullptr;
    std::vector<DrawEvent> drawEvents_{};
    std::vector<DrawEvent> gBufferEvents_{};   // direct (non-occludee) draws
    std::vector<DrawEvent> hiZEvents_{};       // occludee candidates → indirect Hi-Z draw
    std::vector<DrawEvent> shadowEvents_{};
    std::vector<u32t>      instanceGroups_{};  // group boundaries within hiZEvents_
    std::vector<OccluderInfo> occluderInfos_{};
    std::vector<LightData> lightData_{};
    LightData  mainDirectionalLightData_{};
    CameraData cameraData_{};
    FrameData  frameData_{};
    std::size_t roomIdx_{};

    UINT rootParamIdxPID_{};
    UINT rootParamIdxPDD_{};
    UINT rootParamIdxPFD_{};
    UINT rootParamIdxLightData_{};
    UINT rootParamIdxTexPool_{};
    UINT rootParamIdxTexArrayPool_{};
    UINT rootParamIdxTexCubePool_{};
    UINT rootParamIdxSamPool_{};
    UINT rootParamIdxCmpSamPool_{};
    UINT rootParamIdxFirstInstOffset_{};
    UINT rootParamIdxSrcCnts0_{};
    UINT rootParamIdxSrcCnts1_{};
    UINT rootParamIdxPerGroupData_{};
    UINT rootParamIdxDestCnts0_{};
    UINT rootParamIdxDestCnts1_{};
    UINT rootParamIdxOutPerGroupData_{};
    UINT rootParamIdxIndirectCmd_{};
    UINT rootParamIdxHiZMap_{};

    std::size_t jobSizeUpdate_ = 120u;
    std::size_t jobSizeDraw_   = 200u;
};

}  // namespace PBRDeferredPipeline

#endif  // __pbrDeferredPipeline_HPP
