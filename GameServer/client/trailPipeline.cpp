#include "pch.hpp"
#include "trailPipeline.hpp"
#include "shader.hpp"
#include "errorHandling.hpp"

#include <algorithm>

namespace TrailPipeline {

namespace {

void sortDrawEvents(std::vector<DrawEvent>& drawEvents) {
    // Group non-additive first, additive second, then by renderOrder.
    // std::sort (introsort, in-place) is used instead of std::stable_sort
    // because DrawEvent has SIMD alignment (>= 16B) and stable_sort's
    // aux-buffer aligned_storage trips a static_assert under MSVC.
    std::sort(drawEvents.begin(), drawEvents.end(),
        [](const DrawEvent& lhs, const DrawEvent& rhs) {
            if (lhs.additive != rhs.additive)
                return !lhs.additive && rhs.additive;
            return lhs.renderOrder < rhs.renderOrder;
        }
    );
}

}  // namespace

Dispatcher::Dispatcher(
    const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
    DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
    DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
    DescriptorPool* pCmpSamPool,
    const std::shared_ptr<RootSig>& rootSig,
    const ComPtr<ID3D12PipelineState>& shader,
    const ComPtr<ID3D12PipelineState>& shaderAdditive,
    const ComPtr<ID3D12CommandQueue>& cmdQ,
    const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv,
    Fence* pFence, Resources* pResources,
    ThreadPool* threadPool, CommandListPool* commandListPool,
    std::vector<DrawEvent>&& drawEvents,
    const CameraData& cameraData, const FrameData& frameData,
    std::size_t roomIdx
) : descriptorHeaps_(descriptorHeaps),
    pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
    pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
    rootSig_(rootSig), shader_(shader), shaderAdditive_(shaderAdditive), cmdQ_(cmdQ),
    viewport_(viewport), scissorRect_(scissorRect), rtv_(rtv), dsv_(dsv),
    pFence_(pFence), pResources_(pResources),
    threadPool_(threadPool), cmdListPool_(commandListPool),
    drawEvents_(std::move(drawEvents)),
    cameraData_(cameraData), frameData_(frameData), roomIdx_(roomIdx),
    rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
    rootParamIdxPID_(rootSig->paramIdx("PerInstanceData")),
    rootParamIdxPFD_(rootSig->paramIdx("PerFrameData")),
    rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
    rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
    rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
    rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
    rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool"))
{}

void Dispatcher::updateGPUDataSingleThreaded() {
    if (drawEvents_.empty()) return;

    sortDrawEvents(drawEvents_);

    const std::size_t maxDrawEvents = std::min(
        pResources_->perDrawcallData.cbuffers.size(),
        kMaxDrawEvents
    );
    if (drawEvents_.size() > maxDrawEvents)
        drawEvents_.resize(maxDrawEvents);

    // Pack every trail's vertices into one StructuredBuffer for the frame.
    // trailStartOffsets_[i] records where DrawEvent i's vertices begin.
    static auto perInstanceData = std::vector<TrailShader::PerInstanceData>();
    perInstanceData.clear();

    trailStartOffsets_.clear();
    trailStartOffsets_.reserve(drawEvents_.size());

    std::size_t totalVerts = 0u;
    for (const auto& e : drawEvents_) totalVerts += e.vertices.size();
    perInstanceData.reserve(totalVerts);

    for (const auto& e : drawEvents_) {
        trailStartOffsets_.push_back(static_cast<u32t>(perInstanceData.size()));
        for (const auto& v : e.vertices) {
            perInstanceData.push_back(TrailShader::PerInstanceData{
                .pos            = { v.pos.x(), v.pos.y(), v.pos.z() },
                .age            = v.age,
                .cumulativeDist = v.cumulativeDist,
                .pad            = {},
            });
        }
    }

    if (!perInstanceData.empty()) {
        pResources_->perInstanceData.stage(roomIdx_, perInstanceData);
    }

    // Per-drawcall constants.
    for (std::size_t i = 0; i < drawEvents_.size(); ++i) {
        const auto& e = drawEvents_[i];
        BindlessIndex idxMain = e.pMainTex ? e.pMainTex->idxSrv : BindlessIndex{};

        auto pdd = TrailShader::PerDrawcallData{
            .localToWorld         = mu::transpose(e.localToWorld).getXmf(),
            .idxMainTex           = idxMain,
            .baseColor            = e.baseColor.getXmf(),
            .trailStart           = trailStartOffsets_[i],
            .trailCount           = static_cast<u32t>(e.vertices.size()),
            .textureMode          = e.textureMode,
            .inheritParticleColor = e.inheritParticleColor,
            .widthStart           = e.widthStart,
            .widthEnd             = e.widthEnd,
            .widthMultiplier      = e.widthMultiplier,
            .tileLength           = e.tileLength,
            .trailLifetime        = e.trailLifetime,
            .currentSystemTime    = e.currentSystemTime,
            .flowSpeed            = e.flowSpeed,
            .alignMode            = e.alignMode,
        };
        pResources_->perDrawcallData.cbuffers[i].stage(roomIdx_, &pdd, 1u);
    }

    // Per-frame constants.
    const auto vp = cameraData_.view * cameraData_.proj;
    auto pfd = TrailShader::PerFrameData{
        .matViewProj = mu::transpose(vp).getXmf(),
        .cameraPos   = { cameraData_.pos.x(), cameraData_.pos.y(), cameraData_.pos.z() },
        .pad         = 0.f,
    };
    pResources_->perFrameData.stage(roomIdx_, &pfd, 1u);
}

void Dispatcher::drawSingleThreaded() {
    if (drawEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] TrailPipeline::drawSingleThreaded: failed to alloc command list", false);
    if (!cmdCtx.cmdList) return;

    auto* cmdList  = cmdCtx.cmdList.Get();
    auto* cmdAlloc = cmdCtx.cmdAlloc.Get();

    {
        auto hr = cmdAlloc->Reset();
        DISPLAY_ERROR_DX_HR(hr, false);
        if (hr < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }
    }
    {
        auto hr = cmdList->Reset(cmdAlloc, nullptr);
        DISPLAY_ERROR_DX_HR(hr, false);
        if (hr < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }
    }

    DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(shader_.Get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(1u, &rtv_, false, &dsv_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &viewport_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &scissorRect_), false);

    auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
    std::ranges::transform(descriptorHeaps_, descriptorHeapsRaw.begin(),
        [](ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
    DISPLAY_ERROR_DX_VOID(cmdList->SetDescriptorHeaps(
        static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()), false);

    pTexPool_->bind(cmdList, rootParamIdxTexPool_);
    pTexArrayPool_->bind(cmdList, rootParamIdxTexArrayPool_);
    pTexCubePool_->bind(cmdList, rootParamIdxTexCubePool_);
    pSamPool_->bind(cmdList, rootParamIdxSamPool_);
    pCmpSamPool_->bind(cmdList, rootParamIdxCmpSamPool_);

    DISPLAY_ERROR_DX_VOID(cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);

    pResources_->perInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);
    pResources_->perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);

    bool currentAdditive = false;

    for (std::size_t i = 0; i < drawEvents_.size(); ++i) {
        const auto& e = drawEvents_[i];

        if (e.vertices.size() < 2u) continue;  // need at least one segment

        if (e.additive != currentAdditive) {
            currentAdditive = e.additive;
            auto* pso = currentAdditive ? shaderAdditive_.Get() : shader_.Get();
            DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(pso), false);
        }

        pResources_->perDrawcallData.cbuffers[i].bind(cmdList, rootParamIdxPDD_, roomIdx_);

        const UINT vertexCount = static_cast<UINT>((e.vertices.size() - 1u) * 6u);
        DISPLAY_ERROR_DX_VOID(cmdList->DrawInstanced(vertexCount, 1u, 0u, 0u), false);
    }

    {
        auto hr = cmdList->Close();
        DISPLAY_ERROR_DX_HR(hr, false);
        if (hr < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }
    }

    ID3D12CommandList* stagedCmdLists[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, stagedCmdLists), false);

    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

}  // namespace TrailPipeline
