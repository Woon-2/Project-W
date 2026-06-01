#include "pch.hpp"
#include "swordSlashPipeline.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace SwordSlashPipeline {

Dispatcher::Dispatcher(
    const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
    DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
    DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
    DescriptorPool* pCmpSamPool,
    const std::shared_ptr<RootSig>& rootSig,
    const ComPtr<ID3D12PipelineState>& shader,
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
    rootSig_(rootSig), shader_(shader), cmdQ_(cmdQ),
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

    static auto perInstanceData = std::vector<SwordSlashShader::PerInstanceData>();
    perInstanceData.resize(drawEvents_.size());

    std::ranges::transform(drawEvents_, perInstanceData.begin(),
        [](const DrawEvent& e) {
            return SwordSlashShader::PerInstanceData{
                .world             = mu::transpose(e.world).getXmf(),
                .tint              = e.tint.getXmf(),
                .custom1           = { e.custom1.x(), e.custom1.y() },
                .custom2           = { e.custom2.x(), e.custom2.y() },
                .t                 = e.t,
                .customDataEnabled = e.customDataEnabled ? 1.f : 0.f,
                .pad               = {},
            };
        }
    );

    pResources_->perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();

    // Per-drawcall: bindless indices + FX params (one cbuffer per draw event)
    for (std::size_t i = 0; i < drawEvents_.size(); ++i) {
        const auto& e = drawEvents_[i];

        // Null textures get a zeroed bindless index (flowTex may legitimately be null)
        BindlessIndex idxMain     = e.pMainTex     ? e.pMainTex->idxSrv     : BindlessIndex{};
        BindlessIndex idxEmission = e.pEmissionTex ? e.pEmissionTex->idxSrv : BindlessIndex{};
        BindlessIndex idxDissolve = e.pDissolveTex ? e.pDissolveTex->idxSrv : BindlessIndex{};
        BindlessIndex idxFlow     = e.pFlowTex     ? e.pFlowTex->idxSrv     : BindlessIndex{};

        auto pdd = SwordSlashShader::PerDrawcallData{
            .idxMainTex            = idxMain,
            .idxEmissionTex        = idxEmission,
            .idxDissolveTex        = idxDissolve,
            .idxFlowTex            = idxFlow,
            .firstInstanceOffset   = static_cast<u32t>(i),
            .pad0                  = {},
            .speedMainTexUV        = { e.speedMainTexUV.x(), e.speedMainTexUV.y() },
            .speedDissolveUV       = { e.speedDissolveUV.x(), e.speedDissolveUV.y() },
            .speedFlow             = { e.speedFlow.x(), e.speedFlow.y() },
            .padUV0                = {},
            .mainTexST             = e.mainTexST.getXmf(),
            .emissionTexST         = e.emissionTexST.getXmf(),
            .dissolveTexST         = e.dissolveTexST.getXmf(),
            .flowTexST             = e.flowTexST.getXmf(),
            .addColor              = e.addColor.getXmf(),
            .emission              = e.emission,
            .desaturation          = e.desaturation,
            .opacity               = e.opacity,
            .flowPower             = e.flowPower,
            .useSmoothDissolve     = e.useSmoothDissolve ? 1.f : 0.f,
            .time                  = frameData_.time,
            .remapMinMax           = { e.remap.x(), e.remap.y() },
            .pad1                  = {},
        };
        pResources_->perDrawcallData.cbuffers[i].stage(roomIdx_, &pdd, 1u);
    }

    const auto vp = cameraData_.view * cameraData_.proj;
    auto pfd = SwordSlashShader::PerFrameData{
        .matViewProj = mu::transpose(vp).getXmf(),
    };
    pResources_->perFrameData.stage(roomIdx_, &pfd, 1u);
}

void Dispatcher::drawSingleThreaded() {
    if (drawEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] SwordSlashPipeline::drawSingleThreaded: failed to alloc command list", false);
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

    u32t idxDrawcall = 0u;

    for (const auto& e : drawEvents_) {
        // Lazily build pipeline-specific VB view array for this mesh (Position, UV, Color).
        if (e.pMesh->vbViewsByPipeline.find("SwordSlashPipeline") == e.pMesh->vbViewsByPipeline.end()) {
            auto& views = e.pMesh->vbViewsByPipeline["SwordSlashPipeline"];
            views.resize(3u);
            views[0] = e.pMesh->vbViews[e.pMesh->vbIdxMap.at(e.pMesh->name + "_VB_Position")];
            views[1] = e.pMesh->vbViews[e.pMesh->vbIdxMap.at(e.pMesh->name + "_VB_UV")];
            views[2] = e.pMesh->vbViews[e.pMesh->vbIdxMap.at(e.pMesh->name + "_VB_Color")];
        }
        const auto& vbViews = e.pMesh->vbViewsByPipeline.at("SwordSlashPipeline");

        pResources_->perDrawcallData.cbuffers[idxDrawcall].bind(cmdList, rootParamIdxPDD_, roomIdx_);

        DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);
        DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&e.pSubMesh->ibView), false);

        const UINT indexCount = e.pSubMesh->ibView.SizeInBytes / sizeof(u16t);
        DISPLAY_ERROR_DX_VOID(cmdList->DrawIndexedInstanced(indexCount, 1u, 0u, 0, 0u), false);

        ++idxDrawcall;
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

}  // namespace SwordSlashPipeline
