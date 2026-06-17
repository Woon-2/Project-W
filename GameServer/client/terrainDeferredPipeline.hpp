#ifndef __terrainDeferredPipeline_HPP
#define __terrainDeferredPipeline_HPP

#include "gfxUtil.hpp"
#include "terrain.hpp"

class RootSig;
struct Mesh;

namespace TerrainDeferredPipeline {

struct OccluderInfo {
	const TerrainData* terrain = nullptr;
    mu::Mat4x4         world   = {};    // world transform; default is identity
};

// Identical layout to TerrainPipeline::CameraData.
struct CameraData {
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
    mu::Vec3   pos;
};

// Identical layout to TerrainPipeline::LightData (carries CSM cascade data for shadow pass).
struct LightData {
    enum class Type {
        PointLight,
        Spotlight,
        DirectionalLight
    };
    mu::Vec3   dir;
    mu::Vec3   color;
    float      intensity = 1.f;
    Type       type = Type::DirectionalLight;
    bool       isMainDirectionalLight = false;
    std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeViews = {};
    std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeProjs = {};
    XMFLOAT4   cascadeSplitsFarV = {};
    u32t       cascadeCount = MAX_CSM_CASCADES;
    std::array<float, MAX_CSM_CASCADES> cascadeNormalOffsets = {};
    mu::Vec3   cascadeCameraPos = {};  // camera eye for camera-relative cascade space (caster/receiver rebase)
};

// GBuffer pass only needs globalAmbient for the pre-computed lightAccum term.
struct FrameData {
    mu::Vec3 globalAmbient;
};

struct DrawEvent {
    const TerrainData* terrain = nullptr;
    mu::Mat4x4         world   = {};
};

struct Resources {
    struct OccluderPass {
        StructuredBuffer perInstanceData;   // t0
        ConstantBufferArray perDrawcallData;    // b0
    } occluderPass;
    struct ShadowPass {
        ConstantBufferArray      perDrawcallData;  // b0: TerrainShadowMapShader::PerDrawcallData
        ConstantBufferArray perFrameData;     // b1: cascade-per-slot (MAX_CSM_CASCADES)
    } shadowPass;

    struct GBufferPass {
        ConstantBufferArray perDrawcallData;  // b0: TerrainShader::PerDrawcallData
        ConstantBuffer perFrameData;     // b1: TerrainDeferredGBufferShader::PerFrameData
    } gBufferPass;
};

// Reuses the TerrainPipeline VBV cache ("TerrainPipeline" key) for GBuffer draw calls.
// No additional layout is needed since the vertex format is identical.

// Terrain deferred pipeline dispatcher: shadow pass + GBuffer pass.
// Shadow pass records geometry into the shared CSM shadow map (identical to TerrainPipeline).
// GBuffer pass records geometry into the shared GBuffer (deferred lighting path).
class Dispatcher {
public:
    Dispatcher() = default;
    Dispatcher(
        const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
        DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
        DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
        DescriptorPool* pCmpSamPool,
        const std::shared_ptr<RootSig>& rootSig,
        const ComPtr<ID3D12PipelineState>& occluderShader,
        const ComPtr<ID3D12PipelineState>& shadowShader,
        const ComPtr<ID3D12PipelineState>& gBufferShader,
        DescriptorPool* pDsvPool,
        const ComPtr<ID3D12CommandQueue>& cmdQ,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissorRect,
        Fence* pFence,
        Resources* pResources,
        ThreadPool* threadPool,
        CommandListPool* commandListPool,
        std::vector<DrawEvent>&& drawEvents,
        std::vector<OccluderInfo>&& occluderInfos,
        const LightData& mainDirectionalLightData,
        const CameraData& cameraData,
        const FrameData& frameData,
        std::size_t roomIdx
    );

    void occluderPass();
    // Records terrain geometry to the shared CSM shadow map (single-threaded).
    void shadowPass();
    // Multi-threaded path. Delegates to shadowPass(): terrain has 1-4 draw events.
    void shadowPassMT();
    // Records terrain geometry into the shared GBuffer (single-threaded).
    void gBufferPass();
    // Multi-threaded path. Delegates to gBufferPass(): terrain has 1-4 draw events.
    void gBufferPassMT();

private:
    void occluderUpdate();
    void occluderDraw();
    void shadowUpdate();
    void shadowDraw();
    void gBufferUpdate();
    void gBufferDraw();

    std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
    DescriptorPool* pTexPool_      = nullptr;
    DescriptorPool* pTexArrayPool_ = nullptr;
    DescriptorPool* pTexCubePool_  = nullptr;
    DescriptorPool* pSamPool_      = nullptr;
    DescriptorPool* pCmpSamPool_   = nullptr;
    std::shared_ptr<RootSig>          rootSig_       = nullptr;
    ComPtr<ID3D12PipelineState>       occluderShader_  = nullptr;
    ComPtr<ID3D12PipelineState>       shadowShader_  = nullptr;
    ComPtr<ID3D12PipelineState>       gBufferShader_ = nullptr;
    DescriptorPool*                   pDsvPool_      = nullptr;
    ComPtr<ID3D12CommandQueue>        cmdQ_          = nullptr;
    D3D12_VIEWPORT                    viewport_{};
    D3D12_RECT                        scissorRect_{};
    D3D12_CPU_DESCRIPTOR_HANDLE       rtvGB_[5]{};
    D3D12_CPU_DESCRIPTOR_HANDLE       dsvGB_{};
    Fence*                            pFence_        = nullptr;
    Resources*                        pResources_    = nullptr;
    ThreadPool*                       threadPool_    = nullptr;
    CommandListPool*                  cmdListPool_   = nullptr;
    std::vector<DrawEvent>            drawEvents_{};
    std::vector<OccluderInfo>         occluderInfos_{};
    LightData                         mainDirectionalLightData_{};
    CameraData                        cameraData_{};
    FrameData                         frameData_{};
    std::size_t                       roomIdx_{};

    UINT rootParamIdxPID_{};
    UINT rootParamIdxPDD_{};
    UINT rootParamIdxPFD_{};
    UINT rootParamIdxTexPool_{};
    UINT rootParamIdxTexArrayPool_{};
    UINT rootParamIdxTexCubePool_{};
    UINT rootParamIdxSamPool_{};
    UINT rootParamIdxCmpSamPool_{};
};

}   // namespace TerrainDeferredPipeline

#endif  // __terrainDeferredPipeline_HPP
