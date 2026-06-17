#include "pch.hpp"
#include "pbrDeferredPipeline.hpp"
#include "sharedResources.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"


namespace PBRDeferredPipeline {

// --- Mesh layout helpers ---

void __layoutMeshIfNeededShadowPass(const Mesh& mesh) {
    if (mesh.vbViewsByPipeline.contains("PBRDeferredPipeline_Shadow")) return;
    auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("PBRDeferredPipeline_Shadow");
    auto& vbViews = pvbViews->second;
    vbViews.reserve(1u);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_Position"),
        "[GFX Error] PBRDeferredPipeline: " + mesh.name + "_VB_Position not found.", false);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Position")]);
}

// Masked (foliage) shadow pass needs UV so the PS can sample the albedo alpha. Slot 0 =
// Position, slot 1 = UV (matches createShadowMapCSMMaskedShader's input layout).
void __layoutMeshIfNeededShadowMaskedPass(const Mesh& mesh) {
    if (mesh.vbViewsByPipeline.contains("PBRDeferredPipeline_ShadowMasked")) return;
    auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("PBRDeferredPipeline_ShadowMasked");
    auto& vbViews = pvbViews->second;
    vbViews.reserve(2u);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_Position"),
        "[GFX Error] PBRDeferredPipeline: " + mesh.name + "_VB_Position not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_UV"),
        "[GFX Error] PBRDeferredPipeline: " + mesh.name + "_VB_UV not found.", false);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Position")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_UV")]);
}

void __layoutMeshIfNeededGBufferPass(const Mesh& mesh) {
    if (mesh.vbViewsByPipeline.contains("PBRDeferredPipeline_GBuffer")) return;
    auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("PBRDeferredPipeline_GBuffer");
    auto& vbViews = pvbViews->second;
    vbViews.reserve(5u);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_Position"),
        "[GFX Error] PBRDeferredPipeline: " + mesh.name + "_VB_Position not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_Normal"),
        "[GFX Error] PBRDeferredPipeline: " + mesh.name + "_VB_Normal not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_Tangent"),
        "[GFX Error] PBRDeferredPipeline: " + mesh.name + "_VB_Tangent not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_Bitangent"),
        "[GFX Error] PBRDeferredPipeline: " + mesh.name + "_VB_Bitangent not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_UV"),
        "[GFX Error] PBRDeferredPipeline: " + mesh.name + "_VB_UV not found.", false);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Position")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Normal")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Tangent")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Bitangent")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_UV")]);
}

void layoutMeshIfNeeded(const Mesh& mesh) {
    __layoutMeshIfNeededShadowPass(mesh);
    __layoutMeshIfNeededShadowMaskedPass(mesh);
    __layoutMeshIfNeededGBufferPass(mesh);
}

// --- Dispatcher constructor ---

Dispatcher::Dispatcher(
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
) : descriptorHeaps_(descriptorHeaps),
    pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
    pTexCubePool_(pTexCubePool), pSamPool_(pSamPool),
    pCmpSamPool_(pCmpSamPool), pDsvPool_(pDsvPool),
    rootSig_(rootSig), cmdSig_(cmdSig),
    hiZClearShader_(hiZClearShader), hiZCullShader_(hiZCullShader),
    prefixSumShader_(prefixSumShader), hiZCompactShader_(hiZCompactShader),
    hiZCommandShader_(hiZCommandShader), occluderShader_(occluderShader),
    gBufferShader_(gBufferShader), indirectGBufferShader_(indirectGBufferShader),
    shadowShader_(shadowShader), shadowMaskedShader_(shadowMaskedShader),
    cmdQ_(cmdQ), viewport_(viewport), scissorRect_(scissorRect),
    rtvGB_{}, dsvGB_(SharedResources::GBuffer::gBufferData[roomIdx].dsvHandle),
    pFence_(pFence), pResources_(pResources),
    threadPool_(threadPool), cmdListPool_(commandListPool),
    drawEvents_(std::move(drawEvents)),
    occluderInfos_(std::move(occluderInfos)),
    lightData_(std::move(lightData)),
    mainDirectionalLightData_(mainDirectionalLightData),
    cameraData_(cameraData), frameData_(frameData),
    roomIdx_(roomIdx),
    rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
    rootParamIdxPID_(rootSig->paramIdx("PerInstanceData")),
    rootParamIdxPFD_(rootSig->paramIdx("PerFrameData")),
    rootParamIdxLightData_(rootSig->paramIdx("LightData")),
    rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
    rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
    rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
    rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
    rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool")),
    rootParamIdxFirstInstOffset_(rootSig->paramIdx("FirstInstanceOffset")),
    rootParamIdxSrcCnts0_(rootSig->paramIdx("SrcCnts0")),
    rootParamIdxSrcCnts1_(rootSig->paramIdx("SrcCnts1")),
    rootParamIdxPerGroupData_(rootSig->paramIdx("PerGroupData")),
    rootParamIdxDestCnts0_(rootSig->paramIdx("DestCnts0")),
    rootParamIdxDestCnts1_(rootSig->paramIdx("DestCnts1")),
    rootParamIdxOutPerGroupData_(rootSig->paramIdx("OutPerGroupData")),
    rootParamIdxIndirectCmd_(rootSig->paramIdx("IndirectCommand")),
    rootParamIdxHiZMap_(rootSig->paramIdx("SrcTex"))
{
    std::ranges::copy(
        SharedResources::GBuffer::gBufferData[roomIdx].rtvHandles,
        std::begin(rtvGB_)
    );
    SharedResources::ShadowMap::validateRequiredKeys({SharedResources::ShadowMap::kDefaultKey});
}

void Dispatcher::sortDrawEvents() {
    std::sort(drawEvents_.begin(), drawEvents_.end());
    gBufferEvents_.clear();
    hiZEvents_.clear();
    shadowEvents_.clear();
    for (const auto& e : drawEvents_) {
        if (!e.viewFrustumCulled) {
            // Occludee candidates (BVH props, Hi-Z on) go to the indirect path; everything
            // else (static objects, non-BVH foliage, or Hi-Z off) is drawn directly.
            if (e.occludeeCandidate) hiZEvents_.push_back(e);
            else                     gBufferEvents_.push_back(e);
        }
        if (!e.shadowCulled)      shadowEvents_.push_back(e);
    }

    // Group boundaries within hiZEvents_ (one entry per (mesh,subMesh,material) run,
    // plus a trailing sentinel). Mirrors the skinned pipeline's instanceGroups_.
    instanceGroups_.clear();
    auto it = hiZEvents_.cbegin();
    while (it != hiZEvents_.cend()) {
        instanceGroups_.push_back(static_cast<u32t>( std::distance(hiZEvents_.cbegin(), it) ));
        it = std::upper_bound(it, hiZEvents_.cend(), *it);
    }
    instanceGroups_.push_back(std::numeric_limits<u32t>::max());
}

void Dispatcher::occluderPass() { occluderUpdate(); occluderDraw(); }
void Dispatcher::hiZPass()      { hiZPassUpdate();  hiZPassCompute(); }
void Dispatcher::shadowPass()   { shadowUpdate();   shadowDraw();   }
void Dispatcher::shadowPassMT() { shadowUpdateMT(); shadowDrawMT(); }
void Dispatcher::gBufferPass()  { gBufferUpdate();  gBufferDraw();  }
void Dispatcher::gBufferPassMT(){ gBufferUpdateMT(); gBufferDrawMT(); }
void Dispatcher::gBufferIndirectPass()   { gBufferIndirectUpdate(); gBufferIndirectDraw(); }
// Indirect draw is a handful of ExecuteIndirect calls on one command list — no MT split.
void Dispatcher::gBufferIndirectPassMT() { gBufferIndirectUpdate(); gBufferIndirectDraw(); }

// ============================================================
// Shadow pass implementation (identical to PBRPipeline)
// ============================================================

void Dispatcher::shadowUpdate() {
    if (shadowEvents_.empty()) return;

    static auto perInstanceData = std::vector<ShadowMapCSMShader::PerInstanceData>();
    perInstanceData.resize(shadowEvents_.size());

    std::ranges::transform(shadowEvents_, perInstanceData.begin(),
        [](const DrawEvent& e) -> ShadowMapCSMShader::PerInstanceData {
            return ShadowMapCSMShader::PerInstanceData{ .world = mu::transpose(e.world).getXmf() };
        });

    pResources_->shadowPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();

    for (u32t ci = 0u; ci < mainDirectionalLightData_.cascadeCount; ++ci) {
        ShadowMapCSMShader::PerFrameData pfd{};
        pfd.lightVP = mu::transpose(
            mainDirectionalLightData_.cascadeViews[ci] * mainDirectionalLightData_.cascadeProjs[ci]
        ).getXmf();
        pfd.cascadeIdx = ci;
        pfd.camPos     = mainDirectionalLightData_.cascadeCameraPos.getXmf();  // camera-relative caster rebase
        pResources_->shadowPass.perFrameData.cbuffers[ci].stage(roomIdx_, &pfd, 1u);
    }
}

void Dispatcher::shadowUpdateMT() {
    if (shadowEvents_.empty()) return;

    static auto perInstanceData = std::vector<ShadowMapCSMShader::PerInstanceData>();
    perInstanceData.resize(shadowEvents_.size());

    auto latch = std::latch(shadowEvents_.size() / jobSizeUpdate_
        + ((shadowEvents_.size() % jobSizeUpdate_) != 0));

    std::size_t accEventCnt = 0u;
    while (accEventCnt + (jobSizeUpdate_ - 1) < shadowEvents_.size()) {
        addJobShadowUpdate(shadowEvents_.data() + accEventCnt,
            shadowEvents_.data() + accEventCnt + jobSizeUpdate_,
            perInstanceData.data() + accEventCnt, latch);
        accEventCnt += jobSizeUpdate_;
    }
    if (accEventCnt != shadowEvents_.size()) {
        addJobShadowUpdate(shadowEvents_.data() + accEventCnt,
            shadowEvents_.data() + shadowEvents_.size(),
            perInstanceData.data() + accEventCnt, latch);
    }

    for (u32t ci = 0u; ci < mainDirectionalLightData_.cascadeCount; ++ci) {
        ShadowMapCSMShader::PerFrameData pfd{};
        pfd.lightVP = mu::transpose(
            mainDirectionalLightData_.cascadeViews[ci] * mainDirectionalLightData_.cascadeProjs[ci]
        ).getXmf();
        pfd.cascadeIdx = ci;
        pfd.camPos     = mainDirectionalLightData_.cascadeCameraPos.getXmf();  // camera-relative caster rebase
        pResources_->shadowPass.perFrameData.cbuffers[ci].stage(roomIdx_, &pfd, 1u);
    }

    latch.wait();
    pResources_->shadowPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();
}

void Dispatcher::shadowDraw() {
    if (shadowEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] PBRDeferredPipeline::shadowDraw: no command list.", false);
    if (!cmdCtx.cmdList) return;

    auto cmdList  = cmdCtx.cmdList.Get();
    auto cmdAlloc = cmdCtx.cmdAlloc.Get();
    DISPLAY_ERROR_DX_HR(cmdAlloc->Reset(), false);
    DISPLAY_ERROR_DX_HR(cmdList->Reset(cmdAlloc, nullptr), false);

    const auto& csmData    = SharedResources::ShadowMap::csmShadowMapData.at(
        std::string(SharedResources::ShadowMap::kDefaultKey))[roomIdx_];
    const u32t cascadeCount = csmData.cascadeCount;

    DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(shadowShader_.Get()), false);

    auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
    std::ranges::transform(descriptorHeaps_, heapsRaw.begin(),
        [](ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
    DISPLAY_ERROR_DX_VOID(cmdList->SetDescriptorHeaps(
        static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

    DISPLAY_ERROR_DX_VOID(cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);
    pResources_->shadowPass.perInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);

    // Bind the shared bindless texture/sampler pools so the masked (foliage) shadow PS can
    // sample the albedo alpha. Harmless for the opaque PSO, which ignores them.
    pTexPool_->bind(cmdList, rootParamIdxTexPool_);
    pTexArrayPool_->bind(cmdList, rootParamIdxTexArrayPool_);
    pSamPool_->bind(cmdList, rootParamIdxSamPool_);

    // Track the bound PSO so we only switch at opaque<->masked boundaries (foliage is rare).
    ID3D12PipelineState* curPso = shadowShader_.Get();   // set above

    for (u32t ci = 0u; ci < cascadeCount; ++ci) {
        const auto& slice = csmData.cascades[ci];
        DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(0u, nullptr, false, &slice.dsv), false);

        auto svp = D3D12_VIEWPORT{ .TopLeftX=0.f,.TopLeftY=0.f,
            .Width=static_cast<float>(slice.width),.Height=static_cast<float>(slice.height),
            .MinDepth=0.f,.MaxDepth=1.f };
        auto sr  = D3D12_RECT{ .left=0,.top=0,
            .right=static_cast<LONG>(slice.width),.bottom=static_cast<LONG>(slice.height) };
        DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &svp), false);
        DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &sr), false);

        pResources_->shadowPass.perFrameData.cbuffers[ci].bind(cmdList, rootParamIdxPFD_, roomIdx_);

        u32t idxDC = 0u;
        auto gFirst = shadowEvents_.begin();
        while (gFirst != shadowEvents_.end()) {
            auto& de   = *gFirst;
            auto gLast = std::upper_bound(gFirst, shadowEvents_.end(), de);

            layoutMeshIfNeeded(*de.mesh);

            // Foliage (cAlphaCutoff > 0) goes through the masked PSO so the cutout shape is
            // honoured in the shadow; everything else keeps the fast opaque depth-only path.
            const bool masked = de.material && de.material->constantAlphaCutoff > 0.f;
            const char* vbKey  = masked ? "PBRDeferredPipeline_ShadowMasked" : "PBRDeferredPipeline_Shadow";

            if (masked) {
                if (curPso != shadowMaskedShader_.Get()) {
                    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(shadowMaskedShader_.Get()), false);
                    curPso = shadowMaskedShader_.Get();
                }
                auto pdd = ShadowMapCSMMaskedShader::PerDrawcallData{
                    .firstInstanceOffset = static_cast<u32t>(gFirst - shadowEvents_.begin()),
                    .idxAlbedo           = de.material->mapAlbedo.idxSrv,
                    .cAlphaCutoff        = de.material->constantAlphaCutoff
                };
                pResources_->shadowPass.perDrawcallDataMasked.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);
                pResources_->shadowPass.perDrawcallDataMasked.cbuffers[idxDC].bind(cmdList, rootParamIdxPDD_, roomIdx_);
            } else {
                if (curPso != shadowShader_.Get()) {
                    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(shadowShader_.Get()), false);
                    curPso = shadowShader_.Get();
                }
                auto pdd = ShadowMapCSMShader::PerDrawcallData{
                    .firstInstanceOffset = static_cast<u32t>(gFirst - shadowEvents_.begin())
                };
                pResources_->shadowPass.perDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);
                pResources_->shadowPass.perDrawcallData.cbuffers[idxDC].bind(cmdList, rootParamIdxPDD_, roomIdx_);
            }

            auto& vbViews = de.mesh->vbViewsByPipeline.at(vbKey);
            DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);
            DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&de.subMesh->ibView), false);

            const auto stride = de.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT ? sizeof(u16t) : sizeof(u32t);
            DISPLAY_ERROR_DX_VOID(cmdList->DrawIndexedInstanced(
                static_cast<UINT>(de.subMesh->ibView.SizeInBytes / stride),
                static_cast<UINT>(gLast - gFirst), 0u, 0, 0u), false);

            ++idxDC;
            gFirst = gLast;
        }
    }

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR(hrClose, false);
    if (hrClose < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    ID3D12CommandList* staged[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, staged), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

void Dispatcher::shadowDrawMT() {
    if (shadowEvents_.empty()) return;

    static auto instancingGroups = std::vector<decltype(shadowEvents_)::const_iterator>();
    auto it = shadowEvents_.cbegin();
    while (it != shadowEvents_.cend()) {
        instancingGroups.push_back(it);
        it = std::upper_bound(it, shadowEvents_.cend(), *it);
    }
    instancingGroups.push_back(shadowEvents_.cend());

    const auto& csmDataMT = SharedResources::ShadowMap::csmShadowMapData.at(
        std::string(SharedResources::ShadowMap::kDefaultKey))[roomIdx_];
    const u32t cascadeCountMT = csmDataMT.cascadeCount;

    const std::size_t jobCnt = ((instancingGroups.size() - 1u) + (jobSizeDraw_ - 1u)) / jobSizeDraw_;
    const std::size_t totalJobCnt = static_cast<std::size_t>(cascadeCountMT) * jobCnt;
    auto latch = std::latch(totalJobCnt);

    std::list<CommandContext> cmdCtxs{};
    const auto allocCnt = cmdListPool_->alloc(totalJobCnt, CommandListUsage::RenderingSlave, cmdCtxs);
    DISPLAY_ERROR_STR(allocCnt == totalJobCnt,
        "[GFX Error] PBRDeferredPipeline::shadowDrawMT: not enough command lists.", false);
    if (allocCnt != totalJobCnt) {
        cmdListPool_->free(CommandListUsage::RenderingSlave, std::move(cmdCtxs));
        instancingGroups.clear();
        return;
    }

    for (std::size_t k = 0u; k + 1u < instancingGroups.size(); ++k) {
        const auto& de = *instancingGroups[k];
        layoutMeshIfNeeded(*de.mesh);
        const u32t firstInst = static_cast<u32t>(instancingGroups[k] - shadowEvents_.begin());
        // Foliage groups stage into the masked b0 array (with bindless albedo + cutoff);
        // opaque groups stage into the plain b0 array. Slot k is shared (sparse) — a group
        // uses exactly one array, and addJobShadowDraw picks the matching one by material.
        if (de.material && de.material->constantAlphaCutoff > 0.f) {
            auto pdd = ShadowMapCSMMaskedShader::PerDrawcallData{
                .firstInstanceOffset = firstInst,
                .idxAlbedo           = de.material->mapAlbedo.idxSrv,
                .cAlphaCutoff        = de.material->constantAlphaCutoff
            };
            pResources_->shadowPass.perDrawcallDataMasked.cbuffers[k].stage(roomIdx_, &pdd, 1u);
        } else {
            auto pdd = ShadowMapCSMShader::PerDrawcallData{ .firstInstanceOffset = firstInst };
            pResources_->shadowPass.perDrawcallData.cbuffers[k].stage(roomIdx_, &pdd, 1u);
        }
    }

    auto currCmdCtx = cmdCtxs.begin();
    for (u32t ci = 0u; ci < cascadeCountMT; ++ci) {
        const auto& slice = csmDataMT.cascades[ci];
        std::size_t accDC = 0u;

        while (accDC + (jobSizeDraw_ - 1) < instancingGroups.size() - 1u) {
            DISPLAY_ERROR_DX_HR(currCmdCtx->cmdAlloc->Reset(), false);
            DISPLAY_ERROR_DX_HR(currCmdCtx->cmdList->Reset(currCmdCtx->cmdAlloc.Get(), nullptr), false);
            addJobShadowDraw(currCmdCtx->cmdList.Get(), instancingGroups.data() + accDC,
                instancingGroups.data() + accDC + jobSizeDraw_, accDC, slice, ci, latch);
            accDC += jobSizeDraw_;
            ++currCmdCtx;
        }
        if (accDC != instancingGroups.size() - 1u) {
            const auto last = instancingGroups.size() - 1u - accDC;
            DISPLAY_ERROR_DX_HR(currCmdCtx->cmdAlloc->Reset(), false);
            DISPLAY_ERROR_DX_HR(currCmdCtx->cmdList->Reset(currCmdCtx->cmdAlloc.Get(), nullptr), false);
            addJobShadowDraw(currCmdCtx->cmdList.Get(), instancingGroups.data() + accDC,
                instancingGroups.data() + accDC + last, accDC, slice, ci, latch);
            ++currCmdCtx;
        }
    }

    latch.wait();
    instancingGroups.clear();

    auto staged = std::vector<ID3D12CommandList*>();
    staged.reserve(cmdCtxs.size());
    for (auto& ctx : cmdCtxs) staged.push_back(ctx.cmdList.Get());
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(static_cast<UINT>(staged.size()), staged.data()), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
        .splice(pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].end(), std::move(cmdCtxs));
}

// ============================================================
// GBuffer pass implementation
// ============================================================

void Dispatcher::gBufferUpdate() {
    if (gBufferEvents_.empty()) return;

    static auto perInstanceData = std::vector<PBRDeferredGBufferShader::PerInstanceData>();
    perInstanceData.resize(gBufferEvents_.size());

    const auto view     = cameraData_.view;
    const auto viewProj = cameraData_.view * cameraData_.proj;

    std::ranges::transform(gBufferEvents_, perInstanceData.begin(),
        [view, viewProj](const DrawEvent& e) -> PBRDeferredGBufferShader::PerInstanceData {
            return PBRDeferredGBufferShader::PerInstanceData{
                .world      = mu::transpose(e.world).getXmf(),
                .wvp        = mu::transpose(e.world * viewProj).getXmf(),
                .wv         = mu::transpose(e.world * view).getXmf(),
                .wvNormal   = mu::inverse(mu::Mat3x3(e.world * view)).getXmf(),
                .worldNormal = mu::inverse(mu::Mat3x3(e.world)).getXmf()
            };
        });

    pResources_->gBufferPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();

    static auto lightData = std::vector<PBRDeferredGBufferShader::Light>();
    lightData.resize(lightData_.size());
    std::ranges::transform(lightData_, lightData.begin(),
        [view](const LightData& ld) {
            return PBRDeferredGBufferShader::Light{
                .color    = ld.color.getXmf(),
                .falloff  = ld.falloff,
                .posV     = mu::Vec3(mu::Vec4(ld.pos, 1.f) * view).getXmf(),
                .cosTheta = ld.cosTheta,
                .dirV     = mu::NVec3(mu::Vec4(ld.dir, 0.f) * view).getXmf(),
                .cosPhi   = ld.cosPhi,
                .atten    = ld.atten.getXmf(),
                .intensity = ld.intensity,
                .type     = etoi(ld.type)
            };
        });

    const auto& csmData = SharedResources::ShadowMap::csmShadowMapData.at(
        std::string(SharedResources::ShadowMap::kDefaultKey))[roomIdx_];

    PBRDeferredGBufferShader::PerFrameData pfd{};
    pfd.globalAmbient  = frameData_.globalAmbient.getXmf();
    pfd.lightCnt       = static_cast<u32t>(lightData.size());
    pfd.cascadeCount   = mainDirectionalLightData_.cascadeCount;
    for (u32t ci = 0u; ci < mainDirectionalLightData_.cascadeCount; ++ci) {
        pfd.idxShadowMap[ci] = csmData.cascades[ci].tex.idxSrv;
    }
    pfd.cascadeSplitsFarV = mainDirectionalLightData_.cascadeSplitsFarV;
    for (u32t i = 0u; i < mainDirectionalLightData_.cascadeCount; ++i) {
        pfd.lightVP[i] = mu::transpose(
            mainDirectionalLightData_.cascadeViews[i] * mainDirectionalLightData_.cascadeProjs[i]
        ).getXmf();
    }
    {
        const auto& o = mainDirectionalLightData_.cascadeNormalOffsets;
        pfd.cascadeNormalOffsets = XMFLOAT4(o[0], o[1], o[2], o[3]);
    }
    pResources_->gBufferPass.perFrameData.stage(roomIdx_, &pfd, 1u);
    pResources_->gBufferPass.lightData.stage(roomIdx_, lightData);
    lightData.clear();
}

void Dispatcher::gBufferUpdateMT() {
    if (gBufferEvents_.empty()) return;

    static auto perInstanceData = std::vector<PBRDeferredGBufferShader::PerInstanceData>();
    perInstanceData.resize(gBufferEvents_.size());

    auto latch = std::latch(gBufferEvents_.size() / jobSizeUpdate_
        + ((gBufferEvents_.size() % jobSizeUpdate_) != 0));

    const auto viewProj = cameraData_.view * cameraData_.proj;

    std::size_t accEventCnt = 0u;
    while (accEventCnt + (jobSizeUpdate_ - 1) < gBufferEvents_.size()) {
        addJobGBufferUpdate(cameraData_.view, viewProj,
            gBufferEvents_.data() + accEventCnt,
            gBufferEvents_.data() + accEventCnt + jobSizeUpdate_,
            perInstanceData.data() + accEventCnt, latch);
        accEventCnt += jobSizeUpdate_;
    }
    if (accEventCnt != gBufferEvents_.size()) {
        addJobGBufferUpdate(cameraData_.view, viewProj,
            gBufferEvents_.data() + accEventCnt,
            gBufferEvents_.data() + gBufferEvents_.size(),
            perInstanceData.data() + accEventCnt, latch);
    }

    // lightData and pfd are small — no MT
    static auto lightData = std::vector<PBRDeferredGBufferShader::Light>();
    lightData.resize(lightData_.size());
    const auto view = cameraData_.view;
    std::ranges::transform(lightData_, lightData.begin(),
        [view](const LightData& ld) {
            return PBRDeferredGBufferShader::Light{
                .color    = ld.color.getXmf(),
                .falloff  = ld.falloff,
                .posV     = mu::Vec3(mu::Vec4(ld.pos, 1.f) * view).getXmf(),
                .cosTheta = ld.cosTheta,
                .dirV     = mu::NVec3(mu::Vec4(ld.dir, 0.f) * view).getXmf(),
                .cosPhi   = ld.cosPhi,
                .atten    = ld.atten.getXmf(),
                .intensity = ld.intensity,
                .type     = etoi(ld.type)
            };
        });

    const auto& csmData = SharedResources::ShadowMap::csmShadowMapData.at(
        std::string(SharedResources::ShadowMap::kDefaultKey))[roomIdx_];

    PBRDeferredGBufferShader::PerFrameData pfd{};
    pfd.globalAmbient  = frameData_.globalAmbient.getXmf();
    pfd.lightCnt       = static_cast<u32t>(lightData.size());
    pfd.cascadeCount   = mainDirectionalLightData_.cascadeCount;
    for (u32t ci = 0u; ci < mainDirectionalLightData_.cascadeCount; ++ci)
        pfd.idxShadowMap[ci] = csmData.cascades[ci].tex.idxSrv;
    pfd.cascadeSplitsFarV = mainDirectionalLightData_.cascadeSplitsFarV;
    for (u32t i = 0u; i < mainDirectionalLightData_.cascadeCount; ++i)
        pfd.lightVP[i] = mu::transpose(
            mainDirectionalLightData_.cascadeViews[i] * mainDirectionalLightData_.cascadeProjs[i]
        ).getXmf();
    {
        const auto& o = mainDirectionalLightData_.cascadeNormalOffsets;
        pfd.cascadeNormalOffsets = XMFLOAT4(o[0], o[1], o[2], o[3]);
    }
    pResources_->gBufferPass.perFrameData.stage(roomIdx_, &pfd, 1u);
    pResources_->gBufferPass.lightData.stage(roomIdx_, lightData);
    lightData.clear();

    latch.wait();
    pResources_->gBufferPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();
}

void Dispatcher::gBufferDraw() {
    if (gBufferEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] PBRDeferredPipeline::gBufferDraw: no command list.", false);
    if (!cmdCtx.cmdList) return;

    auto cmdList  = cmdCtx.cmdList.Get();
    auto cmdAlloc = cmdCtx.cmdAlloc.Get();
    DISPLAY_ERROR_DX_HR(cmdAlloc->Reset(), false);
    DISPLAY_ERROR_DX_HR(cmdList->Reset(cmdAlloc, nullptr), false);

    DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(gBufferShader_.Get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(5u, rtvGB_, false, &dsvGB_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &viewport_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &scissorRect_), false);

    auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
    std::ranges::transform(descriptorHeaps_, heapsRaw.begin(),
        [](ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
    DISPLAY_ERROR_DX_VOID(cmdList->SetDescriptorHeaps(
        static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

    pTexPool_->bind(cmdList, rootParamIdxTexPool_);
    pTexArrayPool_->bind(cmdList, rootParamIdxTexArrayPool_);
    pTexCubePool_->bind(cmdList, rootParamIdxTexCubePool_);
    pSamPool_->bind(cmdList, rootParamIdxSamPool_);
    pCmpSamPool_->bind(cmdList, rootParamIdxCmpSamPool_);

    DISPLAY_ERROR_DX_VOID(cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);

    pResources_->gBufferPass.perInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);
    pResources_->gBufferPass.lightData.bind(cmdList, rootParamIdxLightData_, roomIdx_);
    pResources_->gBufferPass.perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);

    u32t idxDC = 0u;
    auto gFirst = gBufferEvents_.begin();
    while (gFirst != gBufferEvents_.end()) {
        auto& de   = *gFirst;
        auto gLast = std::upper_bound(gFirst, gBufferEvents_.end(), de);

        pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].bind(cmdList, rootParamIdxPDD_, roomIdx_);

        auto pdd = PBRDeferredGBufferShader::PerDrawcallData{
            .material = PBRDeferredGBufferShader::Material{
                .idxAlbedo            = de.material->mapAlbedo.idxSrv,
                .idxMetallicSmoothness = de.material->mapMetallicSmoothness.idxSrv,
                .idxNormal            = de.material->mapNormal.idxSrv,
                .idxEmmisive          = de.material->mapEmmisive.idxSrv,
                .idxAmbientOcllusion  = de.material->mapAmbientOcclusion.idxSrv,
                .cAlbedo              = de.material->constantAlbedo,
                .cRoughness           = de.material->constantRoughness,
                .cMetallic            = de.material->constantMetallic,
                .cAOStrength          = de.material->constantAOStrength,
                .cAlphaCutoff         = de.material->constantAlphaCutoff,
                .cEmmisive            = de.material->constantEmmisive
            },
            .firstInstanceOffset = static_cast<u32t>(gFirst - gBufferEvents_.begin())
        };
        pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);

        layoutMeshIfNeeded(*de.mesh);
        auto& vbViews = de.mesh->vbViewsByPipeline.at("PBRDeferredPipeline_GBuffer");
        DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);
        DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&de.subMesh->ibView), false);

        const auto stride = de.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT ? sizeof(u16t) : sizeof(u32t);
        DISPLAY_ERROR_DX_VOID(cmdList->DrawIndexedInstanced(
            static_cast<UINT>(de.subMesh->ibView.SizeInBytes / stride),
            static_cast<UINT>(gLast - gFirst), 0u, 0, 0u), false);

        ++idxDC;
        gFirst = gLast;
    }

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR(hrClose, false);
    if (hrClose < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    ID3D12CommandList* staged[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, staged), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

void Dispatcher::gBufferDrawMT() {
    if (gBufferEvents_.empty()) return;

    static auto instancingGroups = std::vector<decltype(gBufferEvents_)::const_iterator>();
    auto it = gBufferEvents_.cbegin();
    while (it != gBufferEvents_.cend()) {
        instancingGroups.push_back(it);
        it = std::upper_bound(it, gBufferEvents_.cend(), *it);
    }
    instancingGroups.push_back(gBufferEvents_.cend());

    const std::size_t jobCnt = ((instancingGroups.size() - 1u) + (jobSizeDraw_ - 1u)) / jobSizeDraw_;
    auto latch = std::latch(jobCnt);

    std::list<CommandContext> cmdCtxs{};
    const auto allocCnt = cmdListPool_->alloc(jobCnt, CommandListUsage::RenderingSlave, cmdCtxs);
    DISPLAY_ERROR_STR(allocCnt == jobCnt,
        "[GFX Error] PBRDeferredPipeline::gBufferDrawMT: not enough command lists.", false);
    if (allocCnt != jobCnt) {
        cmdListPool_->free(CommandListUsage::RenderingSlave, std::move(cmdCtxs));
        instancingGroups.clear();
        return;
    }

    std::size_t accDC    = 0u;
    auto        currCtx  = cmdCtxs.begin();

    while (accDC + (jobSizeDraw_ - 1) < instancingGroups.size() - 1u) {
        DISPLAY_ERROR_DX_HR(currCtx->cmdAlloc->Reset(), false);
        DISPLAY_ERROR_DX_HR(currCtx->cmdList->Reset(currCtx->cmdAlloc.Get(), nullptr), false);
        addJobGBufferDraw(currCtx->cmdList.Get(), instancingGroups.data() + accDC,
            instancingGroups.data() + accDC + jobSizeDraw_, accDC, latch);
        accDC += jobSizeDraw_;
        ++currCtx;
    }
    if (accDC != instancingGroups.size() - 1u) {
        const auto last = instancingGroups.size() - 1u - accDC;
        DISPLAY_ERROR_DX_HR(currCtx->cmdAlloc->Reset(), false);
        DISPLAY_ERROR_DX_HR(currCtx->cmdList->Reset(currCtx->cmdAlloc.Get(), nullptr), false);
        addJobGBufferDraw(currCtx->cmdList.Get(), instancingGroups.data() + accDC,
            instancingGroups.data() + accDC + last, accDC, latch);
    }

    latch.wait();
    instancingGroups.clear();

    auto staged = std::vector<ID3D12CommandList*>();
    staged.reserve(cmdCtxs.size());
    for (auto& ctx : cmdCtxs) staged.push_back(ctx.cmdList.Get());
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(static_cast<UINT>(staged.size()), staged.data()), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
        .splice(pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].end(), std::move(cmdCtxs));
}

// ============================================================
// Thread job helpers
// ============================================================

void MU_CALLCONV Dispatcher::addJobGBufferUpdate(
    mu::Mat4x4 view, const mu::Mat4x4& viewProj,
    const DrawEvent* pFirst, const DrawEvent* pLast,
    PBRDeferredGBufferShader::PerInstanceData* pOut,
    std::latch& latch
) {
    threadPool_->addJob([=, &latch]() {
        std::transform(pFirst, pLast, pOut,
            [view, viewProj](const DrawEvent& e) -> PBRDeferredGBufferShader::PerInstanceData {
                return PBRDeferredGBufferShader::PerInstanceData{
                    .world       = mu::transpose(e.world).getXmf(),
                    .wvp         = mu::transpose(e.world * viewProj).getXmf(),
                    .wv          = mu::transpose(e.world * view).getXmf(),
                    .wvNormal    = mu::inverse(mu::Mat3x3(e.world * view)).getXmf(),
                    .worldNormal = mu::inverse(mu::Mat3x3(e.world)).getXmf()
                };
            });
        latch.count_down();
    });
}

void Dispatcher::addJobGBufferDraw(
    ID3D12GraphicsCommandList* threadCmdList,
    const std::vector<DrawEvent>::const_iterator* pItFirst,
    const std::vector<DrawEvent>::const_iterator* pItLast,
    std::size_t firstDrawcallIdx, std::latch& latch
) {
    threadPool_->addJob([=, &latch]() {
        DISPLAY_ERROR_DX_VOID(threadCmdList->SetGraphicsRootSignature(rootSig_->get()), false);
        DISPLAY_ERROR_DX_VOID(threadCmdList->SetPipelineState(gBufferShader_.Get()), false);
        DISPLAY_ERROR_DX_VOID(threadCmdList->OMSetRenderTargets(5u, rtvGB_, false, &dsvGB_), false);
        DISPLAY_ERROR_DX_VOID(threadCmdList->RSSetViewports(1u, &viewport_), false);
        DISPLAY_ERROR_DX_VOID(threadCmdList->RSSetScissorRects(1u, &scissorRect_), false);

        auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
        std::ranges::transform(descriptorHeaps_, heapsRaw.begin(),
            [](ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
        DISPLAY_ERROR_DX_VOID(threadCmdList->SetDescriptorHeaps(
            static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

        pTexPool_->bind(threadCmdList, rootParamIdxTexPool_);
        pTexArrayPool_->bind(threadCmdList, rootParamIdxTexArrayPool_);
        pTexCubePool_->bind(threadCmdList, rootParamIdxTexCubePool_);
        pSamPool_->bind(threadCmdList, rootParamIdxSamPool_);
        pCmpSamPool_->bind(threadCmdList, rootParamIdxCmpSamPool_);

        DISPLAY_ERROR_DX_VOID(threadCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);

        pResources_->gBufferPass.perInstanceData.bind(threadCmdList, rootParamIdxPID_, roomIdx_);
        pResources_->gBufferPass.perFrameData.bind(threadCmdList, rootParamIdxPFD_, roomIdx_);
        pResources_->gBufferPass.lightData.bind(threadCmdList, rootParamIdxLightData_, roomIdx_);

        std::size_t idxDC = firstDrawcallIdx;
        auto pGroup = pItFirst;

        while (pGroup != pItLast) {
            auto gFirst = *pGroup;
            auto gLast  = *(pGroup + 1);
            const auto& de = *gFirst;

            pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].bind(
                threadCmdList, rootParamIdxPDD_, roomIdx_);

            auto pdd = PBRDeferredGBufferShader::PerDrawcallData{
                .material = PBRDeferredGBufferShader::Material{
                    .idxAlbedo             = de.material->mapAlbedo.idxSrv,
                    .idxMetallicSmoothness = de.material->mapMetallicSmoothness.idxSrv,
                    .idxNormal             = de.material->mapNormal.idxSrv,
                    .idxEmmisive           = de.material->mapEmmisive.idxSrv,
                    .idxAmbientOcllusion   = de.material->mapAmbientOcclusion.idxSrv,
                    .cAlbedo               = de.material->constantAlbedo,
                    .cRoughness            = de.material->constantRoughness,
                    .cMetallic             = de.material->constantMetallic,
                    .cAOStrength           = de.material->constantAOStrength,
                    .cAlphaCutoff          = de.material->constantAlphaCutoff,
                    .cEmmisive             = de.material->constantEmmisive
                },
                .firstInstanceOffset = static_cast<u32t>(gFirst - gBufferEvents_.begin())
            };
            pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);

            layoutMeshIfNeeded(*de.mesh);
            auto& vbViews = de.mesh->vbViewsByPipeline.at("PBRDeferredPipeline_GBuffer");
            DISPLAY_ERROR_DX_VOID(threadCmdList->IASetVertexBuffers(
                0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);
            DISPLAY_ERROR_DX_VOID(threadCmdList->IASetIndexBuffer(&de.subMesh->ibView), false);

            const auto stride = de.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT ? sizeof(u16t) : sizeof(u32t);
            DISPLAY_ERROR_DX_VOID(threadCmdList->DrawIndexedInstanced(
                static_cast<UINT>(de.subMesh->ibView.SizeInBytes / stride),
                static_cast<UINT>(gLast - gFirst), 0u, 0, 0u), false);

            ++idxDC;
            ++pGroup;
        }

        DISPLAY_ERROR_DX_HR(threadCmdList->Close(), false);
        latch.count_down();
    });
}

void Dispatcher::addJobShadowUpdate(
    const DrawEvent* pFirst, const DrawEvent* pLast,
    ShadowMapCSMShader::PerInstanceData* pOut, std::latch& latch
) {
    threadPool_->addJob([=, &latch]() {
        std::transform(pFirst, pLast, pOut,
            [](const DrawEvent& e) -> ShadowMapCSMShader::PerInstanceData {
                return ShadowMapCSMShader::PerInstanceData{ .world = mu::transpose(e.world).getXmf() };
            });
        latch.count_down();
    });
}

void Dispatcher::addJobShadowDraw(
    ID3D12GraphicsCommandList* threadCmdList,
    const std::vector<DrawEvent>::const_iterator* pItFirst,
    const std::vector<DrawEvent>::const_iterator* pItLast,
    std::size_t firstDrawcallIdx,
    const CSMShadowMapData::CascadeSlice& slice, u32t cascadeIdx,
    std::latch& latch
) {
    threadPool_->addJob([=, &latch, &slice]() {
        auto svp = D3D12_VIEWPORT{ .TopLeftX=0.f,.TopLeftY=0.f,
            .Width=static_cast<float>(slice.width),.Height=static_cast<float>(slice.height),
            .MinDepth=0.f,.MaxDepth=1.f };
        auto sr  = D3D12_RECT{ .left=0,.top=0,
            .right=static_cast<LONG>(slice.width),.bottom=static_cast<LONG>(slice.height) };

        DISPLAY_ERROR_DX_VOID(threadCmdList->SetGraphicsRootSignature(rootSig_->get()), false);
        DISPLAY_ERROR_DX_VOID(threadCmdList->SetPipelineState(shadowShader_.Get()), false);
        DISPLAY_ERROR_DX_VOID(threadCmdList->OMSetRenderTargets(0u, nullptr, false, &slice.dsv), false);
        DISPLAY_ERROR_DX_VOID(threadCmdList->RSSetViewports(1u, &svp), false);
        DISPLAY_ERROR_DX_VOID(threadCmdList->RSSetScissorRects(1u, &sr), false);

        auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
        std::ranges::transform(descriptorHeaps_, heapsRaw.begin(),
            [](ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
        DISPLAY_ERROR_DX_VOID(threadCmdList->SetDescriptorHeaps(
            static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

        DISPLAY_ERROR_DX_VOID(threadCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);

        pResources_->shadowPass.perInstanceData.bind(threadCmdList, rootParamIdxPID_, roomIdx_);
        pResources_->shadowPass.perFrameData.cbuffers[cascadeIdx].bind(threadCmdList, rootParamIdxPFD_, roomIdx_);

        // Bindless pools for the masked (foliage) shadow PS; harmless for the opaque PSO.
        pTexPool_->bind(threadCmdList, rootParamIdxTexPool_);
        pTexArrayPool_->bind(threadCmdList, rootParamIdxTexArrayPool_);
        pSamPool_->bind(threadCmdList, rootParamIdxSamPool_);

        ID3D12PipelineState* curPso = shadowShader_.Get();   // set above

        std::size_t idxDC = firstDrawcallIdx;
        auto pGroup = pItFirst;

        while (pGroup != pItLast) {
            auto gFirst = *pGroup;
            auto gLast  = *(pGroup + 1);
            const auto& de = *gFirst;

            const bool masked = de.material && de.material->constantAlphaCutoff > 0.f;
            const char* vbKey  = masked ? "PBRDeferredPipeline_ShadowMasked" : "PBRDeferredPipeline_Shadow";

            if (masked) {
                if (curPso != shadowMaskedShader_.Get()) {
                    DISPLAY_ERROR_DX_VOID(threadCmdList->SetPipelineState(shadowMaskedShader_.Get()), false);
                    curPso = shadowMaskedShader_.Get();
                }
                pResources_->shadowPass.perDrawcallDataMasked.cbuffers[idxDC].bind(
                    threadCmdList, rootParamIdxPDD_, roomIdx_);
            } else {
                if (curPso != shadowShader_.Get()) {
                    DISPLAY_ERROR_DX_VOID(threadCmdList->SetPipelineState(shadowShader_.Get()), false);
                    curPso = shadowShader_.Get();
                }
                pResources_->shadowPass.perDrawcallData.cbuffers[idxDC].bind(
                    threadCmdList, rootParamIdxPDD_, roomIdx_);
            }

            auto& vbViews = de.mesh->vbViewsByPipeline.at(vbKey);
            DISPLAY_ERROR_DX_VOID(threadCmdList->IASetVertexBuffers(
                0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);
            DISPLAY_ERROR_DX_VOID(threadCmdList->IASetIndexBuffer(&de.subMesh->ibView), false);

            const auto stride = de.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT ? sizeof(u16t) : sizeof(u32t);
            DISPLAY_ERROR_DX_VOID(threadCmdList->DrawIndexedInstanced(
                static_cast<UINT>(de.subMesh->ibView.SizeInBytes / stride),
                static_cast<UINT>(gLast - gFirst), 0u, 0, 0u), false);

            ++idxDC;
            ++pGroup;
        }

        DISPLAY_ERROR_DX_HR(threadCmdList->Close(), false);
        latch.count_down();
    });
}

// ============================================================
// Hi-Z occluder pass (near-camera collidable props, depth-only)
// ============================================================

void Dispatcher::occluderUpdate() {
    if (occluderInfos_.empty()) return;

    std::sort(occluderInfos_.begin(), occluderInfos_.end(),
        [](const OccluderInfo& a, const OccluderInfo& b) {
            if (a.mesh != b.mesh) return a.mesh < b.mesh;
            return a.subMesh < b.subMesh;
        });

    static auto perInstanceData = std::vector<HiZOccluderShader::PerInstanceData>();
    perInstanceData.resize(occluderInfos_.size());

    const auto viewProj = cameraData_.view * cameraData_.proj;
    std::ranges::transform(occluderInfos_, perInstanceData.begin(),
        [viewProj](const OccluderInfo& e) {
            return HiZOccluderShader::PerInstanceData{ .wvp = mu::transpose(e.world * viewProj).getXmf() };
        });

    pResources_->occluderPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();
}

void Dispatcher::occluderDraw() {
    if (occluderInfos_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] PBRDeferredPipeline::occluderDraw: no command list.", false);
    if (!cmdCtx.cmdList) return;

    auto cmdList  = cmdCtx.cmdList.Get();
    auto cmdAlloc = cmdCtx.cmdAlloc.Get();
    DISPLAY_ERROR_DX_HR(cmdAlloc->Reset(), false);
    DISPLAY_ERROR_DX_HR(cmdList->Reset(cmdAlloc, nullptr), false);

    DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(occluderShader_.Get()), false);

    auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
    std::ranges::transform(descriptorHeaps_, heapsRaw.begin(),
        [](ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
    DISPLAY_ERROR_DX_VOID(cmdList->SetDescriptorHeaps(
        static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

    DISPLAY_ERROR_DX_VOID(cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);
    DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(
        0u, nullptr, false, &SharedResources::HiZMap::hiZMaps[roomIdx_].dsvHandle), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &viewport_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &scissorRect_), false);

    pResources_->occluderPass.perInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);

    u32t idxDC = 0u;
    auto gFirst = occluderInfos_.begin();
    while (gFirst != occluderInfos_.end()) {
        auto gLast = gFirst;
        while (gLast != occluderInfos_.end()
            && gLast->mesh == gFirst->mesh && gLast->subMesh == gFirst->subMesh) {
            ++gLast;
        }

        const auto& info = *gFirst;
        if (!info.mesh || !info.subMesh) { gFirst = gLast; continue; }

        auto pdd = HiZOccluderShader::PerDrawcallData{
            .firstInstanceOffset = static_cast<u32t>(gFirst - occluderInfos_.begin())
        };
        pResources_->occluderPass.perDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);
        pResources_->occluderPass.perDrawcallData.cbuffers[idxDC].bind(cmdList, rootParamIdxPDD_, roomIdx_);

        layoutMeshIfNeeded(*info.mesh);
        auto& vbViews = info.mesh->vbViewsByPipeline.at("PBRDeferredPipeline_Shadow");
        DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(0u, 1u, &vbViews[0]), false);
        DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&info.subMesh->ibView), false);

        const auto stride = info.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT ? sizeof(u16t) : sizeof(u32t);
        DISPLAY_ERROR_DX_VOID(cmdList->DrawIndexedInstanced(
            static_cast<UINT>(info.subMesh->ibView.SizeInBytes / stride),
            static_cast<UINT>(gLast - gFirst), 0u, 0, 0u), false);

        ++idxDC;
        gFirst = gLast;
    }

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR(hrClose, false);
    if (hrClose < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    ID3D12CommandList* staged[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, staged), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

// ============================================================
// Hi-Z occlusion cull (compute) — occludee candidates (BVH props)
// ============================================================

void Dispatcher::hiZPassUpdate() {
    if (hiZEvents_.empty()) return;

    // 1. Cull pass input. Static props have no skinning/ragdoll, so the world-space AABB
    //    is simply the rest-pose mesh bounds transformed by the instance world matrix.
    static auto perInstanceDataCull = std::vector<HiZCullShader::PerInstanceData>();
    perInstanceDataCull.resize(hiZEvents_.size());
    for (u32t i = 0u, gid = 0u; i < hiZEvents_.size(); ++i) {
        if (i >= instanceGroups_[gid + 1u]) ++gid;
        const auto& e = hiZEvents_[i];
        perInstanceDataCull[i] = HiZCullShader::PerInstanceData{
            .world           = mu::transpose(e.world).getXmf(),
            .aabbMin         = (e.mesh->bounds.center - e.mesh->bounds.size * 0.5f).getXmf(),
            .aabbMax         = (e.mesh->bounds.center + e.mesh->bounds.size * 0.5f).getXmf(),
            .instanceGroupId = gid,
            .instanceObjId   = i   // no CPU feedback; only present for the shared shader layout
        };
    }
    pResources_->hiZPass.perInstanceDataCull.stage(roomIdx_, perInstanceDataCull);
    perInstanceDataCull.clear();

    const auto pfdCull = HiZCullShader::PerFrameData{
        .viewProj   = mu::transpose(cameraData_.view * cameraData_.proj).getXmf(),
        .screenSize = XMFLOAT2(viewport_.Width, viewport_.Height),
        .objCnt     = static_cast<u32t>(hiZEvents_.size()),
    };
    pResources_->hiZPass.perFrameDataCull.stage(roomIdx_, &pfdCull, 1u);

    // 2. Compact pass input
    static auto perInstanceDataCompact = std::vector<HiZCompactShader::PerInstanceData>();
    perInstanceDataCompact.resize(hiZEvents_.size());
    for (u32t i = 0u, gid = 0u; i < hiZEvents_.size(); ++i) {
        if (i >= instanceGroups_[gid + 1u]) ++gid;
        const auto& e = hiZEvents_[i];
        const auto stride = e.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT ? sizeof(u16t) : sizeof(u32t);
        perInstanceDataCompact[i] = HiZCompactShader::PerInstanceData{
            .instanceGroupId = gid,
            .idxCnt = static_cast<u32t>( e.subMesh->ibView.SizeInBytes / stride )
        };
    }
    pResources_->hiZPass.perInstanceDataCompact.stage(roomIdx_, perInstanceDataCompact);
    perInstanceDataCompact.clear();

    const auto pfdCompact = HiZCompactShader::PerFrameData{ .objCnt = static_cast<u32t>( hiZEvents_.size() ) };
    pResources_->hiZPass.perFrameDataCompact.stage(roomIdx_, &pfdCompact, 1u);

    const auto pfdClear = HiZClearShader::PerFrameData{ .groupCnt = static_cast<u32t>( instanceGroups_.size() - 1u ) };
    pResources_->hiZPass.perFrameDataClear.stage(roomIdx_, &pfdClear, 1u);

    const auto pfdCommand = HiZCommandShader::PerFrameData{ .groupCnt = static_cast<u32t>( instanceGroups_.size() - 1u ) };
    pResources_->hiZPass.perFrameDataCommand.stage(roomIdx_, &pfdCommand, 1u);
}

void Dispatcher::hiZPassCompute() {
    if (hiZEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] PBRDeferredPipeline::hiZPass: no command list.", false);
    if (!cmdCtx.cmdList) return;

    auto cmdList  = cmdCtx.cmdList.Get();
    auto cmdAlloc = cmdCtx.cmdAlloc.Get();
    DISPLAY_ERROR_DX_HR(cmdAlloc->Reset(), false);
    DISPLAY_ERROR_DX_HR(cmdList->Reset(cmdAlloc, nullptr), false);

    DISPLAY_ERROR_DX_VOID(cmdList->SetComputeRootSignature(rootSig_->get()), false);

    auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
    std::ranges::transform(descriptorHeaps_, heapsRaw.begin(),
        [](ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
    DISPLAY_ERROR_DX_VOID(cmdList->SetDescriptorHeaps(
        static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

    const auto groupCnt = instanceGroups_.size() - 1u;
    static constexpr auto dispatchUnit = 64u;

    // 0. Clear — reset perGroupCnt and perGroupData.instCnt to 0
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(hiZClearShader_.Get()), false);
    pResources_->hiZPass.perFrameDataClear.bindCompute(cmdList, rootParamIdxPFD_, roomIdx_);
    pResources_->hiZPass.perGroupCnt.bindCompute(cmdList, rootParamIdxDestCnts0_, roomIdx_);
    pResources_->hiZPass.perGroupData.bindCompute(cmdList, rootParamIdxOutPerGroupData_, roomIdx_);
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        (static_cast<u32t>(groupCnt) + dispatchUnit - 1u) / dispatchUnit, 1u, 1u), false );
    pResources_->hiZPass.perGroupCnt.uavBarrier(cmdList, roomIdx_);
    pResources_->hiZPass.perGroupData.uavBarrier(cmdList, roomIdx_);

    // 1. Cull — project each instance AABB, sample the Hi-Z pyramid, write visibility.
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(hiZCullShader_.Get()), false);
    pResources_->hiZPass.perInstanceDataCull.bindCompute(cmdList, rootParamIdxPID_, roomIdx_);
    pResources_->hiZPass.perFrameDataCull.bindCompute(cmdList, rootParamIdxPFD_, roomIdx_);
    DISPLAY_ERROR_DX_VOID( cmdList->SetComputeRootDescriptorTable(
        rootParamIdxHiZMap_, SharedResources::HiZMap::hiZMaps[roomIdx_].srvHandle), false );
    pResources_->hiZPass.perGroupCnt.bindCompute(cmdList, rootParamIdxDestCnts0_, roomIdx_);
    pResources_->hiZPass.visibleFlags.bindCompute(cmdList, rootParamIdxDestCnts1_, roomIdx_);
    // The cull shader always writes a (objId<<1|vis) feedback entry to u3; route it into the
    // throwaway scratch buffer (static props need no CPU visibility readback).
    pResources_->hiZPass.cullScratch.bindCompute(cmdList, rootParamIdxOutPerGroupData_, roomIdx_);
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        static_cast<u32t>( (hiZEvents_.size() + dispatchUnit - 1u) / dispatchUnit ), 1u, 1u), false );
    pResources_->hiZPass.perGroupCnt.uavBarrier(cmdList, roomIdx_);
    pResources_->hiZPass.visibleFlags.uavBarrier(cmdList, roomIdx_);

    // 2. Prefix sum → groupOffsets
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(prefixSumShader_.Get()), false);
    pResources_->hiZPass.perGroupCnt.bindComputeAsSRV(cmdList, rootParamIdxSrcCnts0_, roomIdx_);
    pResources_->hiZPass.groupOffsets.bindCompute(cmdList, rootParamIdxDestCnts0_, roomIdx_);
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        (static_cast<u32t>(groupCnt) + dispatchUnit - 1u) / dispatchUnit, 1u, 1u), false );
    pResources_->hiZPass.groupOffsets.uavBarrier(cmdList, roomIdx_);

    // 3. Compact → visibleIndices + perGroupData
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(hiZCompactShader_.Get()), false);
    pResources_->hiZPass.perInstanceDataCompact.bindCompute(cmdList, rootParamIdxPID_, roomIdx_);
    pResources_->hiZPass.perFrameDataCompact.bindCompute(cmdList, rootParamIdxPFD_, roomIdx_);
    pResources_->hiZPass.groupOffsets.bindComputeAsSRV(cmdList, rootParamIdxSrcCnts0_, roomIdx_);
    pResources_->hiZPass.visibleFlags.bindComputeAsSRV(cmdList, rootParamIdxSrcCnts1_, roomIdx_);
    pResources_->hiZPass.visibleIndices.bindCompute(cmdList, rootParamIdxDestCnts0_, roomIdx_);
    pResources_->hiZPass.perGroupData.bindCompute(cmdList, rootParamIdxOutPerGroupData_, roomIdx_);
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        static_cast<u32t>( (hiZEvents_.size() + dispatchUnit - 1u) / dispatchUnit ), 1u, 1u), false );
    pResources_->hiZPass.perGroupData.uavBarrier(cmdList, roomIdx_);
    pResources_->hiZPass.visibleIndices.uavBarrier(cmdList, roomIdx_);

    // 4. Command → indirect draw args
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(hiZCommandShader_.Get()), false);
    pResources_->hiZPass.perFrameDataCommand.bindCompute(cmdList, rootParamIdxPFD_, roomIdx_);
    pResources_->hiZPass.perGroupData.bindComputeAsSRV(cmdList, rootParamIdxPerGroupData_, roomIdx_);
    pResources_->hiZPass.indirectCmd.bindCompute(cmdList, rootParamIdxIndirectCmd_, roomIdx_);
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        (static_cast<u32t>(groupCnt) + dispatchUnit - 1u) / dispatchUnit, 1u, 1u), false );
    pResources_->hiZPass.indirectCmd.uavBarrier(cmdList, roomIdx_);

    transitionResourceState(cmdList,
        pResources_->hiZPass.visibleIndices.resource(roomIdx_),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transitionResourceState(cmdList,
        pResources_->hiZPass.indirectCmd.resource(roomIdx_),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR(hrClose, false);
    if (hrClose < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    ID3D12CommandList* staged[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, staged), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

// ============================================================
// Indirect GBuffer draw — occludee candidates (BVH props)
// ============================================================

void Dispatcher::stageGBufferFrameAndLights() {
    static auto lightData = std::vector<PBRDeferredGBufferShader::Light>();
    lightData.resize(lightData_.size());
    const auto view = cameraData_.view;
    std::ranges::transform(lightData_, lightData.begin(),
        [view](const LightData& ld) {
            return PBRDeferredGBufferShader::Light{
                .color    = ld.color.getXmf(),
                .falloff  = ld.falloff,
                .posV     = mu::Vec3(mu::Vec4(ld.pos, 1.f) * view).getXmf(),
                .cosTheta = ld.cosTheta,
                .dirV     = mu::NVec3(mu::Vec4(ld.dir, 0.f) * view).getXmf(),
                .cosPhi   = ld.cosPhi,
                .atten    = ld.atten.getXmf(),
                .intensity = ld.intensity,
                .type     = etoi(ld.type)
            };
        });

    const auto& csmData = SharedResources::ShadowMap::csmShadowMapData.at(
        std::string(SharedResources::ShadowMap::kDefaultKey))[roomIdx_];

    PBRDeferredGBufferShader::PerFrameData pfd{};
    pfd.globalAmbient  = frameData_.globalAmbient.getXmf();
    pfd.lightCnt       = static_cast<u32t>(lightData.size());
    pfd.cascadeCount   = mainDirectionalLightData_.cascadeCount;
    for (u32t ci = 0u; ci < mainDirectionalLightData_.cascadeCount; ++ci)
        pfd.idxShadowMap[ci] = csmData.cascades[ci].tex.idxSrv;
    pfd.cascadeSplitsFarV = mainDirectionalLightData_.cascadeSplitsFarV;
    for (u32t i = 0u; i < mainDirectionalLightData_.cascadeCount; ++i)
        pfd.lightVP[i] = mu::transpose(
            mainDirectionalLightData_.cascadeViews[i] * mainDirectionalLightData_.cascadeProjs[i]
        ).getXmf();
    {
        const auto& o = mainDirectionalLightData_.cascadeNormalOffsets;
        pfd.cascadeNormalOffsets = XMFLOAT4(o[0], o[1], o[2], o[3]);
    }
    pResources_->gBufferPass.perFrameData.stage(roomIdx_, &pfd, 1u);
    pResources_->gBufferPass.lightData.stage(roomIdx_, lightData);
    lightData.clear();
}

void Dispatcher::gBufferIndirectUpdate() {
    if (hiZEvents_.empty()) return;

    static auto perInstanceData = std::vector<PBRDeferredGBufferShader::PerInstanceData>();
    perInstanceData.resize(hiZEvents_.size());

    const auto view     = cameraData_.view;
    const auto viewProj = cameraData_.view * cameraData_.proj;
    std::ranges::transform(hiZEvents_, perInstanceData.begin(),
        [view, viewProj](const DrawEvent& e) -> PBRDeferredGBufferShader::PerInstanceData {
            return PBRDeferredGBufferShader::PerInstanceData{
                .world       = mu::transpose(e.world).getXmf(),
                .wvp         = mu::transpose(e.world * viewProj).getXmf(),
                .wv          = mu::transpose(e.world * view).getXmf(),
                .wvNormal    = mu::inverse(mu::Mat3x3(e.world * view)).getXmf(),
                .worldNormal = mu::inverse(mu::Mat3x3(e.world)).getXmf()
            };
        });

    pResources_->hiZPass.gBufferPerInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();

    stageGBufferFrameAndLights();
}

void Dispatcher::gBufferIndirectDraw() {
    if (hiZEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] PBRDeferredPipeline::gBufferIndirectDraw: no command list.", false);
    if (!cmdCtx.cmdList) return;

    auto cmdList  = cmdCtx.cmdList.Get();
    auto cmdAlloc = cmdCtx.cmdAlloc.Get();
    DISPLAY_ERROR_DX_HR(cmdAlloc->Reset(), false);
    DISPLAY_ERROR_DX_HR(cmdList->Reset(cmdAlloc, nullptr), false);

    DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(indirectGBufferShader_.Get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(5u, rtvGB_, false, &dsvGB_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &viewport_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &scissorRect_), false);

    auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
    std::ranges::transform(descriptorHeaps_, heapsRaw.begin(),
        [](ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
    DISPLAY_ERROR_DX_VOID(cmdList->SetDescriptorHeaps(
        static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

    pTexPool_->bind(cmdList, rootParamIdxTexPool_);
    pTexArrayPool_->bind(cmdList, rootParamIdxTexArrayPool_);
    pTexCubePool_->bind(cmdList, rootParamIdxTexCubePool_);
    pSamPool_->bind(cmdList, rootParamIdxSamPool_);
    pCmpSamPool_->bind(cmdList, rootParamIdxCmpSamPool_);

    DISPLAY_ERROR_DX_VOID(cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);

    pResources_->hiZPass.gBufferPerInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);
    pResources_->gBufferPass.lightData.bind(cmdList, rootParamIdxLightData_, roomIdx_);
    pResources_->gBufferPass.perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);
    pResources_->hiZPass.visibleIndices.bindGraphicsAsSRV(cmdList, rootParamIdxSrcCnts0_, roomIdx_);

    u32t idxDC = 0u;
    auto gFirst = hiZEvents_.begin();
    while (gFirst != hiZEvents_.end()) {
        auto& de   = *gFirst;
        auto gLast = std::upper_bound(gFirst, hiZEvents_.end(), de);

        pResources_->hiZPass.gBufferPerDrawcallData.cbuffers[idxDC].bind(cmdList, rootParamIdxPDD_, roomIdx_);
        auto pdd = PBRDeferredGBufferShader::PerDrawcallData{
            .material = PBRDeferredGBufferShader::Material{
                .idxAlbedo            = de.material->mapAlbedo.idxSrv,
                .idxMetallicSmoothness = de.material->mapMetallicSmoothness.idxSrv,
                .idxNormal            = de.material->mapNormal.idxSrv,
                .idxEmmisive          = de.material->mapEmmisive.idxSrv,
                .idxAmbientOcllusion  = de.material->mapAmbientOcclusion.idxSrv,
                .cAlbedo              = de.material->constantAlbedo,
                .cRoughness           = de.material->constantRoughness,
                .cMetallic            = de.material->constantMetallic,
                .cAOStrength          = de.material->constantAOStrength,
                .cAlphaCutoff         = de.material->constantAlphaCutoff,
                .cEmmisive            = de.material->constantEmmisive
            },
            // firstInstanceOffset comes from the command signature root constant in the
            // indirect path; this field is unused by the indirect VS.
            .firstInstanceOffset = 0u
        };
        pResources_->hiZPass.gBufferPerDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);

        layoutMeshIfNeeded(*de.mesh);
        auto& vbViews = de.mesh->vbViewsByPipeline.at("PBRDeferredPipeline_GBuffer");
        DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);
        DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&de.subMesh->ibView), false);

        static constexpr u32t kIndirectCmdStride = sizeof(u32t) + sizeof(DrawIndexedInstancedArgs);
        DISPLAY_ERROR_DX_VOID( cmdList->ExecuteIndirect(
            cmdSig_->get(), 1u, pResources_->hiZPass.indirectCmd.resource(roomIdx_),
            idxDC * kIndirectCmdStride, nullptr, 0u), false );

        ++idxDC;
        gFirst = gLast;
    }

    transitionResourceState(cmdList,
        pResources_->hiZPass.visibleIndices.resource(roomIdx_),
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transitionResourceState(cmdList,
        pResources_->hiZPass.indirectCmd.resource(roomIdx_),
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR(hrClose, false);
    if (hrClose < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    ID3D12CommandList* staged[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, staged), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

}  // namespace PBRDeferredPipeline
