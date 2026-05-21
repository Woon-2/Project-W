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
    __layoutMeshIfNeededGBufferPass(mesh);
}

// --- Dispatcher constructor ---

Dispatcher::Dispatcher(
    const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
    DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
    DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
    DescriptorPool* pCmpSamPool, DescriptorPool* pDsvPool,
    const std::shared_ptr<RootSig>& rootSig,
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
) : descriptorHeaps_(descriptorHeaps),
    pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
    pTexCubePool_(pTexCubePool), pSamPool_(pSamPool),
    pCmpSamPool_(pCmpSamPool), pDsvPool_(pDsvPool),
    rootSig_(rootSig),
    gBufferShader_(gBufferShader), shadowShader_(shadowShader),
    cmdQ_(cmdQ), viewport_(viewport), scissorRect_(scissorRect),
    rtvGB_{}, dsvGB_(SharedResources::GBuffer::gBufferData[roomIdx].dsvHandle),
    pFence_(pFence), pResources_(pResources),
    threadPool_(threadPool), cmdListPool_(commandListPool),
    drawEvents_(std::move(drawEvents)),
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
    rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool"))
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
    shadowEvents_.clear();
    for (const auto& e : drawEvents_) {
        if (!e.viewFrustumCulled) gBufferEvents_.push_back(e);
        if (!e.shadowCulled)      shadowEvents_.push_back(e);
    }
}

void Dispatcher::shadowPass()   { shadowUpdate();   shadowDraw();   }
void Dispatcher::shadowPassMT() { shadowUpdateMT(); shadowDrawMT(); }
void Dispatcher::gBufferPass()  { gBufferUpdate();  gBufferDraw();  }
void Dispatcher::gBufferPassMT(){ gBufferUpdateMT(); gBufferDrawMT(); }

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

            pResources_->shadowPass.perDrawcallData.cbuffers[idxDC].bind(cmdList, rootParamIdxPDD_, roomIdx_);
            auto pdd = ShadowMapCSMShader::PerDrawcallData{
                .firstInstanceOffset = static_cast<u32t>(gFirst - shadowEvents_.begin())
            };
            pResources_->shadowPass.perDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);

            layoutMeshIfNeeded(*de.mesh);
            auto& vbViews = de.mesh->vbViewsByPipeline.at("PBRDeferredPipeline_Shadow");
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
        auto pdd = ShadowMapCSMShader::PerDrawcallData{
            .firstInstanceOffset = static_cast<u32t>(instancingGroups[k] - shadowEvents_.begin())
        };
        pResources_->shadowPass.perDrawcallData.cbuffers[k].stage(roomIdx_, &pdd, 1u);
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
    DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(4u, rtvGB_, false, &dsvGB_), false);
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
        DISPLAY_ERROR_DX_VOID(threadCmdList->OMSetRenderTargets(4u, rtvGB_, false, &dsvGB_), false);
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

        std::size_t idxDC = firstDrawcallIdx;
        auto pGroup = pItFirst;

        while (pGroup != pItLast) {
            auto gFirst = *pGroup;
            auto gLast  = *(pGroup + 1);
            const auto& de = *gFirst;

            pResources_->shadowPass.perDrawcallData.cbuffers[idxDC].bind(
                threadCmdList, rootParamIdxPDD_, roomIdx_);

            auto& vbViews = de.mesh->vbViewsByPipeline.at("PBRDeferredPipeline_Shadow");
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

}  // namespace PBRDeferredPipeline
