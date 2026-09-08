#ifndef __trailPipeline_HPP
#define __trailPipeline_HPP

#include "gfxUtil.hpp"

#include <cstddef>

class RootSig;

namespace TrailPipeline {

inline constexpr std::size_t kMaxDrawEvents = 4096u;
inline constexpr std::size_t kMaxVerticesPerTrail = 32u;
inline constexpr std::size_t kMaxTrailVertices = kMaxDrawEvents * kMaxVerticesPerTrail;

struct CameraData {
    mu::Mat4x4 view;
    mu::Mat4x4 proj;
    mu::Vec3   pos;
};

struct FrameData {
    float time = 0.f;
};

// One captured trail vertex in CPU form. Pipeline transforms this into the
// HLSL-side TrailVertex inside updateGPUDataSingleThreaded().
struct TrailVertexCPU {
    mu::Vec3 pos             = { 0.f, 0.f, 0.f };
    float    age             = 0.f;
    float    cumulativeDist  = 0.f;
};

// One drawcall = one particle's trail (a strip of >= 2 vertices).
// The vertices field is owned by the DrawEvent; Pipeline copies them into a
// system-wide StructuredBuffer at update time and remembers the offset.
struct DrawEvent {
    std::vector<TrailVertexCPU> vertices;        // size N: drawn as (N-1) quads

    // Per-trail constants -> TrailShader::PerDrawcallData (b0)
    mu::Mat4x4    localToWorld;                  // identity for World-space sim
    mu::Vec4      baseColor          = { 1.f, 1.f, 1.f, 1.f };
    float         widthStart         = 1.f;
    float         widthEnd           = 1.f;
    float         widthMultiplier    = 1.f;
    float         tileLength         = 1.f;
    u32t          textureMode        = 0u;       // 0 = Stretch, 1 = Tile
    u32t          inheritParticleColor = 1u;     // reserved
    float         trailLifetime      = 1.f;
    float         currentSystemTime  = 0.f;
    float         flowSpeed          = 0.f;      // Tile-mode UV scroll along the trail; 0 = static
    u32t          alignMode          = 0u;       // 0 = camera-facing (default), 1 = ground-aligned

    // Premultiplies alpha into rgb before output so additive (One/One) blending
    // actually fades by per-vertex age instead of ignoring alpha. Only meaningful
    // for additive draws; leave 0 for the standard SRC_ALPHA-blended PSO.
    u32t          premultiplyAlpha   = 0u;

    // 1 = overlay a procedural flowing-chevron mask (directional arrows toward the
    // head) on top of the texture. 0 = texture unchanged (default). Used by the
    // path-guidance ribbon; leave 0 for ordinary particle trails.
    u32t          patternMode        = 0u;

    // Material / sort
    const Texture* pMainTex          = nullptr;
    bool          additive           = false;
    int           renderOrder        = 0;
};

struct Resources {
    StructuredBuffer    perInstanceData;   // t0  — system-wide TrailVertex pool
    ConstantBufferArray perDrawcallData;   // b0  — one entry per DrawEvent
    ConstantBuffer      perFrameData;      // b1
};

// Draws camera-facing ribbon strips for Unity-style particle trails.
// No vertex/index buffers are bound — VS pulls vertices from perInstanceData
// indexed by SV_VertexID, using the trailStart offset from PerDrawcallData.
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
        const ComPtr<ID3D12PipelineState>& shaderAdditive,
        RenderSubmitter* submitter,
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

    // Single-threaded fallbacks for the multi-threaded entry points used by GFX.
    // Trail draws are typically few enough that the extra job overhead is not
    // worth it; we route both modes to the single-threaded implementation.
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
    ComPtr<ID3D12PipelineState> shaderAdditive_{};
    RenderSubmitter* submitter_{};
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

    // Per-drawcall offset into perInstanceData, filled at updateGPU time and
    // read at drawSingleThreaded time. Same size as drawEvents_.
    std::vector<u32t> trailStartOffsets_{};

    UINT rootParamIdxPDD_{};
    UINT rootParamIdxPID_{};
    UINT rootParamIdxPFD_{};
    UINT rootParamIdxTexPool_{};
    UINT rootParamIdxTexArrayPool_{};
    UINT rootParamIdxTexCubePool_{};
    UINT rootParamIdxSamPool_{};
    UINT rootParamIdxCmpSamPool_{};
};

}  // namespace TrailPipeline

#endif  // __trailPipeline_HPP
