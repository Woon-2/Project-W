#ifndef __pbrDeferredSkinnedPipeline_HPP
#define __pbrDeferredSkinnedPipeline_HPP

#include "gfxUtil.hpp"
#include "sharedResources.hpp"

class RootSig;
class CmdSig;

struct Mesh;
struct SubMesh;
struct Material;

namespace ShadowMapSkinnedCSMShader {
    struct PerInstanceData;
}

namespace PBRDeferredSkinnedGBufferShader {
    struct PerInstanceData;
}

namespace PBRDeferredSkinnedPipeline {

// Same light/camera/frame data structs as PBRSkinnedPipeline
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
    mu::Mat4x4              world;
    std::span<mu::Mat4x4>   boneXforms;
    const Mesh*             mesh;
    const SubMesh*          subMesh;
    const Material*         material;

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

struct Resources {
    struct HiZPass {
        StructuredBuffer perInstanceDataCull;   // t0
        StructuredBuffer perInstanceDataCompact;   // t0
        ConstantBuffer perFrameDataClear;      // b1
        ConstantBuffer perFrameDataCull;    // b1
        ConstantBuffer perFrameDataCompact;    // b1
        ConstantBuffer perFrameDataCommand;    // b1
        RWStructuredBuffer perGroupCnt;   // t3, u1
        RWStructuredBuffer groupOffsets;   // t3, u1
        RWStructuredBuffer visibleFlags;   // t4, u2
        RWStructuredBuffer perGroupData;   // t5, u3
        RWStructuredBuffer visibleIndices;   // u1
        RWStructuredBuffer indirectCmd;   // u0, space1

        // GPU readback: perGroupCnt 복사 대상 (D3D12_HEAP_TYPE_READBACK)
        ComPtr<ID3D12Resource> visibleCountReadback;
        u32t* visibleCountMapped = nullptr;
        u32t  lastGroupCnt      = 0u;
        u32t  lastObjCnt        = 0u;
        u32t  lastVisibleCount  = 0u;
        u32t  lastTotalCount    = 0u;
    } hiZPass;

    struct ShadowPass {
        StructuredBuffer    perInstanceData;   // t0
        StructuredBuffer    boneData;          // t2
        ConstantBufferArray perDrawcallData;   // b0
        ConstantBufferArray perFrameData;      // b1: cascade-per-slot
    } shadowPass;

    struct GBufferPass {
        StructuredBuffer    perInstanceData;   // t0
        StructuredBuffer    lightData;         // t1
        StructuredBuffer    boneData;          // t2
        ConstantBufferArray perDrawcallData;   // b0
        ConstantBuffer      perFrameData;      // b1
    } gBufferPass;
};

// Prepare the mesh's VBV layout for the deferred skinned GBuffer pipeline if not already done.
// Shadow slot: "PBRDeferredSkinnedPipeline_Shadow" (position, boneIndices, boneWeights)
// GBuffer slot: "PBRDeferredSkinnedPipeline_GBuffer"
//               (position, normal, tangent, bitangent, uv, boneIndices, boneWeights)
void layoutMeshIfNeeded(const Mesh& mesh);

// Dispatcher for PBRDeferredSkinnedPipeline.
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
        const ComPtr<ID3D12PipelineState>& gBufferShader,
        const ComPtr<ID3D12PipelineState>& shadowShader,
        const ComPtr<ID3D12CommandQueue>& cmdQ,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissorRect,
        Fence* pFence,
        Resources* pResources, ThreadPool* threadPool,
        CommandListPool* commandListPool,
        std::vector<DrawEvent>&& drawEvents,
        std::vector<LightData>&& lightData,
        const LightData& mainDirectionalLightData,
        const CameraData& cameraData, const FrameData& frameData,
        std::size_t roomIdx
    );

    void sortDrawEvents();
    void hiZPass();
    void shadowPass();
    void shadowPassMT();
    void gBufferIndirectPass();
    void gBufferIndirectPassMT();
    void gBufferPass();
    void gBufferPassMT();

private:
    void hiZPassUpdate();
    void hiZPassCompute();

    void shadowUpdate();
    void shadowUpdateMT();
    void shadowDraw();
    void shadowDrawMT();

    void gBufferIndirectUpdate();
    void gBufferIndirectUpdateMT();
    void gBufferIndirectDraw();
    void gBufferIndirectDrawMT();

    void gBufferUpdate();
    void gBufferUpdateMT();
    void gBufferDraw();
    void gBufferDrawMT();

    void MU_CALLCONV addJobGBufferUpdate( mu::Mat4x4 view, const mu::Mat4x4& viewProj,
        const DrawEvent* pFirst, const DrawEvent* pLast,
        PBRDeferredSkinnedGBufferShader::PerInstanceData* pOut,
        std::latch& latch
    );
    void addJobGBufferIndirectDraw( ID3D12GraphicsCommandList* threadCmdList,
        const std::vector<DrawEvent>::const_iterator* pItFirst,
        const std::vector<DrawEvent>::const_iterator* pItLast,
        std::size_t firstDrawcallIdx, std::latch& latch
    );
    void addJobGBufferDraw( ID3D12GraphicsCommandList* threadCmdList,
        const std::vector<DrawEvent>::const_iterator* pItFirst,
        const std::vector<DrawEvent>::const_iterator* pItLast,
        std::size_t firstDrawcallIdx, std::latch& latch
    );
    void addJobShadowUpdate( const DrawEvent* pFirst,
        const DrawEvent* pLast, ShadowMapSkinnedCSMShader::PerInstanceData* pOut,
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
    DescriptorPool* pTexPool_      = nullptr;
    DescriptorPool* pTexArrayPool_ = nullptr;
    DescriptorPool* pTexCubePool_  = nullptr;
    DescriptorPool* pSamPool_      = nullptr;
    DescriptorPool* pCmpSamPool_   = nullptr;
    DescriptorPool* pDsvPool_      = nullptr;
    std::shared_ptr<RootSig> rootSig_ = nullptr;
    std::shared_ptr<CmdSig> cmdSig_ = nullptr;
    ComPtr<ID3D12PipelineState> hiZClearShader_   = nullptr;
    ComPtr<ID3D12PipelineState> hiZCullShader_ = nullptr;
    ComPtr<ID3D12PipelineState> prefixSumShader_ = nullptr;
    ComPtr<ID3D12PipelineState> hiZCompactShader_ = nullptr;
    ComPtr<ID3D12PipelineState> hiZCommandShader_ = nullptr;
    ComPtr<ID3D12PipelineState> gBufferShader_ = nullptr;
    ComPtr<ID3D12PipelineState> shadowShader_  = nullptr;
    ComPtr<ID3D12CommandQueue>  cmdQ_          = nullptr;
    D3D12_VIEWPORT    viewport_{};
    D3D12_RECT        scissorRect_{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvGB_[4]{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvGB_{};
    Fence* pFence_ = nullptr;
    ThreadPool*       threadPool_  = nullptr;
    CommandListPool*  cmdListPool_ = nullptr;
    Resources*        pResources_  = nullptr;
    std::vector<DrawEvent> drawEvents_{};
    std::vector<u32t> instanceGroups_{};
    std::vector<LightData> lightData_{};
    LightData  mainDirectionalLightData_{};
    CameraData cameraData_{};
    FrameData  frameData_{};
    std::size_t roomIdx_{};

    UINT rootParamIdxFirstInstOffset_{};
    UINT rootParamIdxPID_{};
    UINT rootParamIdxPDD_{};
    UINT rootParamIdxPFD_{};
    UINT rootParamIdxLightData_{};
    UINT rootParamIdxBoneData_{};
    UINT rootParamIdxTexPool_{};
    UINT rootParamIdxTexArrayPool_{};
    UINT rootParamIdxTexCubePool_{};
    UINT rootParamIdxSamPool_{};
    UINT rootParamIdxCmpSamPool_{};
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

}  // namespace PBRDeferredSkinnedPipeline

#endif  // __pbrDeferredSkinnedPipeline_HPP
