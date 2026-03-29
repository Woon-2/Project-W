#ifndef __terrainPipeline_HPP
#define __terrainPipeline_HPP

#include "gfxUtil.hpp"
#include "terrain.hpp"

class RootSig;
struct Mesh;

namespace TerrainPipeline {

struct CameraData {
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
    mu::Vec3   pos;
};

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
    // CSM cascade data (isMainDirectionalLight == true 일 때만 유효)
    std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeViews = {};
    std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeProjs = {};
    XMFLOAT4   cascadeSplitsFarV = {};  // view-space far depth per cascade
    u32t       cascadeCount = MAX_CSM_CASCADES;
    std::array<float, MAX_CSM_CASCADES> cascadeNormalOffsets = {};
};

struct FrameData {
    mu::Vec3 globalAmbient;
    u32t     lightCount = 0u;
};

struct DrawEvent {
    const TerrainData* terrain = nullptr;
    mu::Mat4x4         world   = {};    // world transform; default is identity
};

struct Resources {
    struct ShadowPass {
        ConstantBuffer      perDrawcallData;  // b0: TerrainShadowMapShader::PerDrawcallData
        ConstantBufferArray perFrameData;     // b1: cascade별 별도 슬롯 (MAX_CSM_CASCADES)
    } shadowPass;

    struct MainPass {
        ConstantBuffer   perDrawcallData;  // b0
        ConstantBuffer   perFrameData;     // b1
        StructuredBuffer lightData;        // t1
    } mainPass;
};

// Ensures that the TerrainPipeline VBV cache exists in mesh.vbViewsByPipeline.
// Slots: 0 = Position, 1 = Normal, 2 = UV
void layoutMeshIfNeeded(const Mesh& mesh);

// Terrain pipeline dispatcher. Supports shadow pass and main pass.
// Supports single-threaded (shadowPass/mainPass) and multi-threaded (shadowPassMT/mainPassMT) paths
// for API symmetry with PBRPipeline::Dispatcher.
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
        const ComPtr<ID3D12PipelineState>& shadowShader,
        DescriptorPool* pDsvPool,
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
        std::vector<LightData>&& lightData,
        const LightData& mainDirectionalLightData,
        const CameraData& cameraData,
        const FrameData& frameData,
        std::size_t roomIdx
    );

    // Records terrain geometry to the shared shadow map (single-threaded).
    // Does nothing if drawEvents is empty.
    void shadowPass();
    // Records terrain geometry to the shared shadow map (multi-threaded path).
    // Delegates to shadowPass(): terrain has 1-4 draw events, no MT benefit.
    void shadowPassMT();
    // Stages GPU constant buffers then issues draw calls (single-threaded).
    // Does nothing if drawEvents is empty.
    void mainPass();
    // Stages GPU constant buffers then issues draw calls (multi-threaded path).
    // Does nothing if drawEvents is empty.
    void mainPassMT();

private:
    // Stages shadow pass GPU constant buffer (PerFrameData) and records draw calls.
    void shadowUpdate();
    void shadowDraw();
    // Stages per-frame GPU constant buffer (PerFrameData).
    // Single-threaded. Does nothing if drawEvents is empty.
    void mainUpdate();
    // Stages per-frame GPU constant buffer (PerFrameData).
    // Multi-threaded path. See implementation for why MT is not applied here.
    void mainUpdateMT();
    // Allocates a command context and records all draw calls.
    // Single-threaded. Does nothing if drawEvents is empty.
    void mainDraw();
    // Allocates a command context and records all draw calls.
    // Multi-threaded path. See implementation for why MT is not applied here.
    void mainDrawMT();
    std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
    DescriptorPool* pTexPool_      = nullptr;
    DescriptorPool* pTexArrayPool_ = nullptr;
    DescriptorPool* pTexCubePool_  = nullptr;
    DescriptorPool* pSamPool_      = nullptr;
    DescriptorPool* pCmpSamPool_   = nullptr;
    std::shared_ptr<RootSig>          rootSig_      = nullptr;
    ComPtr<ID3D12PipelineState>       shader_       = nullptr;
    ComPtr<ID3D12PipelineState>       shadowShader_ = nullptr;
    DescriptorPool*                   pDsvPool_     = nullptr;
    ComPtr<ID3D12CommandQueue>        cmdQ_         = nullptr;
    D3D12_VIEWPORT                    viewport_{};
    D3D12_RECT                        scissorRect_{};
    D3D12_CPU_DESCRIPTOR_HANDLE       rtv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE       dsv_{};
    Fence*                            pFence_      = nullptr;
    ThreadPool*                       threadPool_  = nullptr;
    CommandListPool*                  cmdListPool_ = nullptr;
    Resources*                        pResources_  = nullptr;
    std::vector<DrawEvent>            drawEvents_{};
    std::vector<LightData>            lightData_{};
    LightData                         mainDirectionalLightData_{};
    CameraData                        cameraData_{};
    FrameData                         frameData_{};
    std::size_t                       roomIdx_{};

    UINT rootParamIdxPDD_{};
    UINT rootParamIdxPFD_{};
    UINT rootParamIdxLightData_{};
    UINT rootParamIdxTexPool_{};
    UINT rootParamIdxTexArrayPool_{};
    UINT rootParamIdxTexCubePool_{};
    UINT rootParamIdxSamPool_{};
    UINT rootParamIdxCmpSamPool_{};
};

}   // namespace TerrainPipeline

#endif  // __terrainPipeline_HPP
