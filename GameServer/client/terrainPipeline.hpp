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
    mu::Vec3   dir;
    mu::Vec3   color;
    float      intensity = 1.f;
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
};

struct FrameData {
    mu::Vec3     globalAmbient;
    BindlessIndex idxShadowMap;
};

struct DrawEvent {
    const TerrainData* terrain = nullptr;
};

struct Resources {
    struct MainPass {
        ConstantBuffer perDrawcallData;  // b0
        ConstantBuffer perFrameData;     // b1
    } mainPass;
};

// Ensures that the TerrainPipeline VBV cache exists in mesh.vbViewsByPipeline.
// Slots: 0 = Position, 1 = Normal, 2 = UV
void layoutMeshIfNeeded(const Mesh& mesh);

// Terrain pipeline dispatcher - single-threaded, no shadow pass.
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
        CommandListPool* commandListPool,
        std::vector<DrawEvent>&& drawEvents,
        const LightData& lightData,
        const CameraData& cameraData,
        const FrameData& frameData,
        std::size_t roomIdx
    );

    // Updates GPU constant buffers and issues draw calls.
    // Does nothing if drawEvents is empty.
    void mainPass();

private:
    std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
    DescriptorPool* pTexPool_      = nullptr;
    DescriptorPool* pTexArrayPool_ = nullptr;
    DescriptorPool* pTexCubePool_  = nullptr;
    DescriptorPool* pSamPool_      = nullptr;
    DescriptorPool* pCmpSamPool_   = nullptr;
    std::shared_ptr<RootSig>          rootSig_ = nullptr;
    ComPtr<ID3D12PipelineState>       shader_  = nullptr;
    ComPtr<ID3D12CommandQueue>        cmdQ_    = nullptr;
    D3D12_VIEWPORT                    viewport_{};
    D3D12_RECT                        scissorRect_{};
    D3D12_CPU_DESCRIPTOR_HANDLE       rtv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE       dsv_{};
    Fence*                            pFence_     = nullptr;
    CommandListPool*                  cmdListPool_ = nullptr;
    Resources*                        pResources_  = nullptr;
    std::vector<DrawEvent>            drawEvents_{};
    LightData                         lightData_{};
    CameraData                        cameraData_{};
    FrameData                         frameData_{};
    std::size_t                       roomIdx_{};

    UINT rootParamIdxPDD_{};
    UINT rootParamIdxPFD_{};
    UINT rootParamIdxTexPool_{};
    UINT rootParamIdxTexArrayPool_{};
    UINT rootParamIdxTexCubePool_{};
    UINT rootParamIdxSamPool_{};
    UINT rootParamIdxCmpSamPool_{};
};

}   // namespace TerrainPipeline

#endif  // __terrainPipeline_HPP
