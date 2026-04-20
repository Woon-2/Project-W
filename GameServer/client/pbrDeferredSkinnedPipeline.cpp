#include "pch.hpp"
#include "pbrDeferredSkinnedPipeline.hpp"
#include "sharedResources.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace PBRDeferredSkinnedPipeline {

// --- Mesh layout helpers ---

void __layoutMeshIfNeededShadowPass(const Mesh& mesh) {
    if (mesh.vbViewsByPipeline.contains("PBRDeferredSkinnedPipeline_Shadow")) return;
    auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("PBRDeferredSkinnedPipeline_Shadow");
    auto& vbViews = pvbViews->second;
    vbViews.reserve(3u);  // position, boneIndices, boneWeights
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_Position"),
        "[GFX Error] PBRDeferredSkinnedPipeline: " + mesh.name + "_VB_Position not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_BoneIndices"),
        "[GFX Error] PBRDeferredSkinnedPipeline: " + mesh.name + "_VB_BoneIndices not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_BoneWeights"),
        "[GFX Error] PBRDeferredSkinnedPipeline: " + mesh.name + "_VB_BoneWeights not found.", false);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Position")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_BoneIndices")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_BoneWeights")]);
}

void __layoutMeshIfNeededGBufferPass(const Mesh& mesh) {
    if (mesh.vbViewsByPipeline.contains("PBRDeferredSkinnedPipeline_GBuffer")) return;
    auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("PBRDeferredSkinnedPipeline_GBuffer");
    auto& vbViews = pvbViews->second;
    vbViews.reserve(7u);  // position, normal, tangent, bitangent, uv, boneIndices, boneWeights
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_Position"),
        "[GFX Error] PBRDeferredSkinnedPipeline: " + mesh.name + "_VB_Position not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_Normal"),
        "[GFX Error] PBRDeferredSkinnedPipeline: " + mesh.name + "_VB_Normal not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_Tangent"),
        "[GFX Error] PBRDeferredSkinnedPipeline: " + mesh.name + "_VB_Tangent not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_Bitangent"),
        "[GFX Error] PBRDeferredSkinnedPipeline: " + mesh.name + "_VB_Bitangent not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_UV"),
        "[GFX Error] PBRDeferredSkinnedPipeline: " + mesh.name + "_VB_UV not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_BoneIndices"),
        "[GFX Error] PBRDeferredSkinnedPipeline: " + mesh.name + "_VB_BoneIndices not found.", false);
    DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(mesh.name + "_VB_BoneWeights"),
        "[GFX Error] PBRDeferredSkinnedPipeline: " + mesh.name + "_VB_BoneWeights not found.", false);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Position")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Normal")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Tangent")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Bitangent")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_UV")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_BoneIndices")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_BoneWeights")]);
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
) : descriptorHeaps_(descriptorHeaps),
    pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
    pTexCubePool_(pTexCubePool), pSamPool_(pSamPool),
    pCmpSamPool_(pCmpSamPool), pDsvPool_(pDsvPool),
    rootSig_(rootSig), cmdSig_(cmdSig),
    hiZClearShader_(hiZClearShader),
    hiZCullShader_(hiZCullShader), prefixSumShader_(prefixSumShader),
    hiZCompactShader_(hiZCompactShader), hiZCommandShader_(hiZCommandShader),
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
    rootParamIdxFirstInstOffset_(rootSig->paramIdx("FirstInstanceOffset")),
    rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
    rootParamIdxPID_(rootSig->paramIdx("PerInstanceData")),
    rootParamIdxPFD_(rootSig->paramIdx("PerFrameData")),
    rootParamIdxLightData_(rootSig->paramIdx("LightData")),
    rootParamIdxBoneData_(rootSig->paramIdx("BoneData")),
    rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
    rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
    rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
    rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
    rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool")),
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

    auto it = drawEvents_.cbegin();
    while (it != drawEvents_.cend()) {
        instanceGroups_.push_back(static_cast<u32t>( std::distance(drawEvents_.cbegin(), it) ));
        it = std::upper_bound(it, drawEvents_.cend(), *it);
    }
    instanceGroups_.push_back(std::numeric_limits<u32t>::max());
}

void Dispatcher::hiZPass()       { hiZPassUpdate(); hiZPassCompute(); }
void Dispatcher::shadowPass()    { shadowUpdate();    shadowDraw();    }
void Dispatcher::shadowPassMT()  { shadowUpdateMT();  shadowDrawMT();  }
void Dispatcher::gBufferIndirectPass() { gBufferIndirectUpdate();       gBufferIndirectDraw(); }
void Dispatcher::gBufferIndirectPassMT() { gBufferIndirectUpdateMT();   gBufferIndirectDrawMT(); }
void Dispatcher::gBufferPass()   { gBufferUpdate();   gBufferDraw();   }
void Dispatcher::gBufferPassMT() { gBufferUpdateMT(); gBufferDrawMT(); }

void Dispatcher::hiZPassUpdate() {
    if (drawEvents_.empty()) return;

    // 이전 프레임의 readback 결과 합산 (1-frame delay)
    if (pResources_->hiZPass.visibleCountMapped && pResources_->hiZPass.lastGroupCnt > 0u) {
        u32t visible = 0u;
        for (u32t g = 0u; g < pResources_->hiZPass.lastGroupCnt; ++g)
            visible += pResources_->hiZPass.visibleCountMapped[g];
        pResources_->hiZPass.lastVisibleCount = visible;
        pResources_->hiZPass.lastTotalCount   = pResources_->hiZPass.lastObjCnt;
    }

    // 이전 프레임 visibleFlags readback → lastVisibilityFlags + objectVisibility 갱신
    if (pResources_->hiZPass.visibilityMapped
        && pResources_->hiZPass.lastVisibilityObjCnt > 0u
        && !pResources_->hiZPass.lastDrawEventObjectIds.empty())
    {
        const u32t cnt = pResources_->hiZPass.lastVisibilityObjCnt;
        pResources_->hiZPass.lastVisibilityFlags.assign(
            pResources_->hiZPass.visibilityMapped,
            pResources_->hiZPass.visibilityMapped + cnt);

        std::fill(pResources_->hiZPass.objectVisibility.begin(),
                  pResources_->hiZPass.objectVisibility.end(), false);
        const auto& ids = pResources_->hiZPass.lastDrawEventObjectIds;
        for (u32t i = 0u; i < cnt && i < static_cast<u32t>(ids.size()); ++i) {
            const u32t oid = ids[i];
            if (oid < static_cast<u32t>(pResources_->hiZPass.objectVisibility.size())) {
                pResources_->hiZPass.objectVisibility[oid] =
                    pResources_->hiZPass.objectVisibility[oid]
                    || (pResources_->hiZPass.lastVisibilityFlags[i] != 0u);
            }
        }
    }

    // 현재 프레임 DrawEvents의 renderObjectId 저장 (다음 프레임에 사용)
    {
        auto& ids = pResources_->hiZPass.lastDrawEventObjectIds;
        ids.resize(drawEvents_.size());
        for (u32t i = 0u; i < static_cast<u32t>(drawEvents_.size()); ++i)
            ids[i] = drawEvents_[i].renderObjectId;
    }

    // 1. Hi-Z Cull Pass
    static auto perInstanceDataCull = std::vector<HiZCullShader::PerInstanceData>();
    perInstanceDataCull.resize(drawEvents_.size());

    for (u32t i = 0u, gid = 0u; i < drawEvents_.size(); ++i) {
        if (i >= instanceGroups_[gid + 1u]) {
            ++gid;
        }
        auto& e = drawEvents_[i];

        perInstanceDataCull[i] = HiZCullShader::PerInstanceData{
            .world = mu::transpose(e.world).getXmf(),
            .aabbMin = (e.mesh->bounds.center - e.mesh->bounds.size * 0.5f).getXmf(),
            .aabbMax = (e.mesh->bounds.center + e.mesh->bounds.size * 0.5f).getXmf(),
            .instanceGroupId = gid
        };
    }

    pResources_->hiZPass.perInstanceDataCull.stage(roomIdx_, perInstanceDataCull);
    perInstanceDataCull.clear();

    const auto pfdCull = HiZCullShader::PerFrameData{
        .viewProj = mu::transpose(cameraData_.view * cameraData_.proj).getXmf(),
        .screenSize = XMFLOAT2(viewport_.Width, viewport_.Height),
        .objCnt = static_cast<u32t>(drawEvents_.size()),
    };
    pResources_->hiZPass.perFrameDataCull.stage(roomIdx_, &pfdCull, 1u);
    
    // 2. Hi-Z Compact Pass
    static auto perInstanceDataCompact = std::vector<HiZCompactShader::PerInstanceData>();
    perInstanceDataCompact.resize(drawEvents_.size());

    for (u32t i = 0u, gid = 0u; i < drawEvents_.size(); ++i) {
        if (i >= instanceGroups_[gid + 1u]) {
            ++gid;
        }
        auto& e = drawEvents_[i];

        const auto stride = e.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT ? sizeof(u16t) : sizeof(u32t);
        perInstanceDataCompact[i] = HiZCompactShader::PerInstanceData{
            .instanceGroupId = gid,
            .idxCnt = static_cast<u32t>( e.subMesh->ibView.SizeInBytes / stride )
        };
    }

    pResources_->hiZPass.perInstanceDataCompact.stage(roomIdx_, perInstanceDataCompact);
    perInstanceDataCompact.clear();

    const auto pfdCompact = HiZCompactShader::PerFrameData{
        .objCnt = static_cast<u32t>( drawEvents_.size() )
    };
    pResources_->hiZPass.perFrameDataCompact.stage(roomIdx_, &pfdCompact, 1u);

    // 0. Hi-Z Clear Pass
    const auto pfdClear = HiZClearShader::PerFrameData{
        .groupCnt = static_cast<u32t>( instanceGroups_.size() - 1u )
    };
    pResources_->hiZPass.perFrameDataClear.stage(roomIdx_, &pfdClear, 1u);

    // 3. Hi-Z Command Pass
    const auto pfdCommand = HiZCommandShader::PerFrameData{
        .groupCnt = static_cast<u32t>( instanceGroups_.size() - 1u )
    };
    pResources_->hiZPass.perFrameDataCommand.stage(roomIdx_, &pfdCommand, 1u);
}

void Dispatcher::hiZPassCompute() {
    if (drawEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] PBRDeferredSkinnedPipeline::hiZPass: no command list.", false);
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

    // 0. Hi-Z Clear Pass — reset perGroupCnt and perGroupData.instCnt to 0
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(hiZClearShader_.Get()), false);
    pResources_->hiZPass.perFrameDataClear.bindCompute(cmdList, rootParamIdxPFD_, roomIdx_);
    pResources_->hiZPass.perGroupCnt.bindCompute(cmdList, rootParamIdxDestCnts0_, roomIdx_);
    pResources_->hiZPass.perGroupData.bindCompute(cmdList, rootParamIdxOutPerGroupData_, roomIdx_);

    const auto groupCntForClear = static_cast<u32t>( instanceGroups_.size() - 1u );
    static constexpr auto clearDispatchUnit = 64u;
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        (groupCntForClear + clearDispatchUnit - 1u) / clearDispatchUnit, 1u, 1u
    ), false );

    pResources_->hiZPass.perGroupCnt.uavBarrier(cmdList, roomIdx_);
    pResources_->hiZPass.perGroupData.uavBarrier(cmdList, roomIdx_);

    // 1. Hi-Z Cull Pass
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(hiZCullShader_.Get()), false);
    pResources_->hiZPass.perInstanceDataCull.bindCompute(cmdList, rootParamIdxPID_, roomIdx_);
    pResources_->hiZPass.perFrameDataCull.bindCompute(cmdList, rootParamIdxPFD_, roomIdx_);
    DISPLAY_ERROR_DX_VOID( cmdList->SetComputeRootDescriptorTable(
        rootParamIdxHiZMap_, SharedResources::HiZMap::hiZMaps[roomIdx_].srvHandle
    ), false );
    pResources_->hiZPass.perGroupCnt.bindCompute(cmdList, rootParamIdxDestCnts0_, roomIdx_);
    pResources_->hiZPass.visibleFlags.bindCompute(cmdList, rootParamIdxDestCnts1_, roomIdx_);

    static constexpr auto cullDispatchUnit = 64u;
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        static_cast<u32t>( (drawEvents_.size() + cullDispatchUnit - 1u) / cullDispatchUnit ), 1u, 1u
    ), false );

    pResources_->hiZPass.perGroupCnt.uavBarrier(cmdList, roomIdx_);
    pResources_->hiZPass.visibleFlags.uavBarrier(cmdList, roomIdx_);

    // 2. Prefix Sum Pass
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(prefixSumShader_.Get()), false);
    pResources_->hiZPass.perGroupCnt.bindComputeAsSRV(cmdList, rootParamIdxSrcCnts0_, roomIdx_);
    pResources_->hiZPass.groupOffsets.bindCompute(cmdList, rootParamIdxDestCnts0_, roomIdx_);

    const auto groupCnt = instanceGroups_.size() - 1u;
    static constexpr auto prefixSumDispatchUnit = 64u;
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        static_cast<u32t>( (groupCnt + prefixSumDispatchUnit - 1u) / prefixSumDispatchUnit ), 1u, 1u
    ), false );

    pResources_->hiZPass.groupOffsets.uavBarrier(cmdList, roomIdx_);

    // 3. Hi-Z Compact Pass
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(hiZCompactShader_.Get()), false);
    pResources_->hiZPass.perInstanceDataCompact.bindCompute(cmdList, rootParamIdxPID_, roomIdx_);
    pResources_->hiZPass.perFrameDataCompact.bindCompute(cmdList, rootParamIdxPFD_, roomIdx_);
    pResources_->hiZPass.groupOffsets.bindComputeAsSRV(cmdList, rootParamIdxSrcCnts0_, roomIdx_);
    pResources_->hiZPass.visibleFlags.bindComputeAsSRV(cmdList, rootParamIdxSrcCnts1_, roomIdx_);
    pResources_->hiZPass.visibleIndices.bindCompute(cmdList, rootParamIdxDestCnts0_, roomIdx_);
    pResources_->hiZPass.perGroupData.bindCompute(cmdList, rootParamIdxOutPerGroupData_, roomIdx_);

    static constexpr auto compactDispatchUnit = 64u;
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        static_cast<u32t>( (drawEvents_.size() + compactDispatchUnit - 1u) / compactDispatchUnit ), 1u, 1u
    ), false );

    pResources_->hiZPass.perGroupData.uavBarrier(cmdList, roomIdx_);
    pResources_->hiZPass.visibleIndices.uavBarrier(cmdList, roomIdx_);

    // 4. Hi-Z Command Pass
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(hiZCommandShader_.Get()), false);
    pResources_->hiZPass.perFrameDataCommand.bindCompute(cmdList, rootParamIdxPFD_, roomIdx_);
    pResources_->hiZPass.perGroupData.bindComputeAsSRV(cmdList, rootParamIdxPerGroupData_, roomIdx_);
    pResources_->hiZPass.indirectCmd.bindCompute(cmdList, rootParamIdxIndirectCmd_, roomIdx_);

    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        static_cast<u32t>( (groupCnt + prefixSumDispatchUnit - 1u) / prefixSumDispatchUnit ), 1u, 1u
    ), false );

    pResources_->hiZPass.indirectCmd.uavBarrier(cmdList, roomIdx_);

    transitionResourceState(cmdList,
        pResources_->hiZPass.visibleIndices.resource(roomIdx_),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transitionResourceState(cmdList,
        pResources_->hiZPass.indirectCmd.resource(roomIdx_),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

    // readback: visibleFlags → CPU-readable buffer (1-frame delay)
    // Compact Pass 이후 실행: visibleFlags는 이미 SRV로 소비됐으므로 COPY_SOURCE 전환 안전
    if (pResources_->hiZPass.visibilityReadback && !drawEvents_.empty()) {
        transitionResourceState(cmdList,
            pResources_->hiZPass.visibleFlags.resource(roomIdx_),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        cmdList->CopyBufferRegion(
            pResources_->hiZPass.visibilityReadback.Get(), 0,
            pResources_->hiZPass.visibleFlags.resource(roomIdx_), 0,
            drawEvents_.size() * sizeof(u32t));
        transitionResourceState(cmdList,
            pResources_->hiZPass.visibleFlags.resource(roomIdx_),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        pResources_->hiZPass.lastVisibilityObjCnt = static_cast<u32t>(drawEvents_.size());
    }

    // readback: perGroupCnt → CPU-readable buffer (1-frame delay)
    if (pResources_->hiZPass.visibleCountReadback) {
        transitionResourceState(cmdList,
            pResources_->hiZPass.perGroupCnt.resource(roomIdx_),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        cmdList->CopyBufferRegion(
            pResources_->hiZPass.visibleCountReadback.Get(), 0,
            pResources_->hiZPass.perGroupCnt.resource(roomIdx_), 0,
            groupCnt * sizeof(u32t));
        transitionResourceState(cmdList,
            pResources_->hiZPass.perGroupCnt.resource(roomIdx_),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        pResources_->hiZPass.lastGroupCnt = static_cast<u32t>(groupCnt);
        pResources_->hiZPass.lastObjCnt   = static_cast<u32t>(drawEvents_.size());
    }

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR(hrClose, false);
    if (hrClose < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    ID3D12CommandList* staged[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, staged), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

// ============================================================
// Shadow pass implementation (identical to PBRSkinnedPipeline, CSM variant)
// ============================================================

void Dispatcher::shadowUpdate() {
    if (drawEvents_.empty()) return;

    static auto perInstanceData = std::vector<ShadowMapSkinnedCSMShader::PerInstanceData>();
    perInstanceData.resize(drawEvents_.size());

    u32t boneUploadCnt = 0u;
    std::ranges::transform(drawEvents_, perInstanceData.begin(),
        [&boneUploadCnt](const DrawEvent& e) {
            const auto ret = ShadowMapSkinnedCSMShader::PerInstanceData{
                .world          = mu::transpose(e.world).getXmf(),
                .rootBoneOffset = boneUploadCnt
            };
            boneUploadCnt += static_cast<u32t>(e.boneXforms.size());
            return ret;
        });

    pResources_->shadowPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();

    static auto boneData = std::vector<ShadowMapSkinnedCSMShader::BoneData>();
    boneData.resize(boneUploadCnt);
    auto itBone = boneData.begin();
    std::ranges::for_each(drawEvents_, [&itBone](const DrawEvent& e) {
        for (auto& bx : e.boneXforms) {
            *itBone = ShadowMapSkinnedCSMShader::BoneData{ mu::transpose(bx).getXmf() };
            ++itBone;
        }
    });
    pResources_->shadowPass.boneData.stage(roomIdx_, boneData);
    boneData.clear();

    for (u32t ci = 0u; ci < mainDirectionalLightData_.cascadeCount; ++ci) {
        ShadowMapSkinnedCSMShader::PerFrameData pfd{};
        pfd.lightVP = mu::transpose(
            mainDirectionalLightData_.cascadeViews[ci] * mainDirectionalLightData_.cascadeProjs[ci]
        ).getXmf();
        pfd.cascadeIdx = ci;
        pResources_->shadowPass.perFrameData.cbuffers[ci].stage(roomIdx_, &pfd, 1u);
    }
}

void Dispatcher::shadowUpdateMT() {
    if (drawEvents_.empty()) return;

    static auto perInstanceData = std::vector<ShadowMapSkinnedCSMShader::PerInstanceData>();
    perInstanceData.resize(drawEvents_.size());

    auto latch = std::latch(drawEvents_.size() / jobSizeUpdate_
        + ((drawEvents_.size() % jobSizeUpdate_) != 0));

    std::size_t accEventCnt = 0u;
    while (accEventCnt + (jobSizeUpdate_ - 1) < drawEvents_.size()) {
        addJobShadowUpdate(drawEvents_.data() + accEventCnt,
            drawEvents_.data() + accEventCnt + jobSizeUpdate_,
            perInstanceData.data() + accEventCnt, latch);
        accEventCnt += jobSizeUpdate_;
    }
    if (accEventCnt != drawEvents_.size()) {
        addJobShadowUpdate(drawEvents_.data() + accEventCnt,
            drawEvents_.data() + drawEvents_.size(),
            perInstanceData.data() + accEventCnt, latch);
    }

    for (u32t ci = 0u; ci < mainDirectionalLightData_.cascadeCount; ++ci) {
        ShadowMapSkinnedCSMShader::PerFrameData pfd{};
        pfd.lightVP = mu::transpose(
            mainDirectionalLightData_.cascadeViews[ci] * mainDirectionalLightData_.cascadeProjs[ci]
        ).getXmf();
        pfd.cascadeIdx = ci;
        pResources_->shadowPass.perFrameData.cbuffers[ci].stage(roomIdx_, &pfd, 1u);
    }

    latch.wait();

    // rootBoneOffset must be computed sequentially — fix up after parallel world-xform pass
    for (std::size_t i = 1u; i < drawEvents_.size(); ++i) {
        perInstanceData[i].rootBoneOffset = static_cast<u32t>(
            drawEvents_[i-1].boneXforms.size() + perInstanceData[i-1].rootBoneOffset
        );
    }

    static auto boneData = std::vector<ShadowMapSkinnedCSMShader::BoneData>();
    boneData.resize(perInstanceData.back().rootBoneOffset
        + static_cast<u32t>(drawEvents_.back().boneXforms.size()));
    auto itBone = boneData.begin();
    std::ranges::for_each(drawEvents_, [&itBone](const DrawEvent& e) {
        for (auto& bx : e.boneXforms) {
            *itBone = ShadowMapSkinnedCSMShader::BoneData{ mu::transpose(bx).getXmf() };
            ++itBone;
        }
    });
    pResources_->shadowPass.boneData.stage(roomIdx_, boneData);
    boneData.clear();

    pResources_->shadowPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();
}

void Dispatcher::shadowDraw() {
    if (drawEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] PBRDeferredSkinnedPipeline::shadowDraw: no command list.", false);
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
    pResources_->shadowPass.boneData.bind(cmdList, rootParamIdxBoneData_, roomIdx_);

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
        auto gFirst = drawEvents_.begin();
        while (gFirst != drawEvents_.end()) {
            auto& de   = *gFirst;
            auto gLast = std::upper_bound(gFirst, drawEvents_.end(), de);

            pResources_->shadowPass.perDrawcallData.cbuffers[idxDC].bind(cmdList, rootParamIdxPDD_, roomIdx_);
            auto pdd = ShadowMapSkinnedCSMShader::PerDrawcallData{
                .firstInstanceOffset = static_cast<u32t>(gFirst - drawEvents_.begin())
            };
            pResources_->shadowPass.perDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);

            layoutMeshIfNeeded(*de.mesh);
            auto& vbViews = de.mesh->vbViewsByPipeline.at("PBRDeferredSkinnedPipeline_Shadow");
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
    if (drawEvents_.empty()) return;

    static auto instancingGroups = std::vector<decltype(drawEvents_)::const_iterator>();
    auto it = drawEvents_.cbegin();
    while (it != drawEvents_.cend()) {
        instancingGroups.push_back(it);
        it = std::upper_bound(it, drawEvents_.cend(), *it);
    }
    instancingGroups.push_back(drawEvents_.cend());

    const auto& csmDataMT   = SharedResources::ShadowMap::csmShadowMapData.at(
        std::string(SharedResources::ShadowMap::kDefaultKey))[roomIdx_];
    const u32t cascadeCountMT = csmDataMT.cascadeCount;

    const std::size_t jobCnt      = ((instancingGroups.size() - 1u) + (jobSizeDraw_ - 1u)) / jobSizeDraw_;
    const std::size_t totalJobCnt = static_cast<std::size_t>(cascadeCountMT) * jobCnt;
    auto latch = std::latch(totalJobCnt);

    std::list<CommandContext> cmdCtxs{};
    const auto allocCnt = cmdListPool_->alloc(totalJobCnt, CommandListUsage::RenderingSlave, cmdCtxs);
    DISPLAY_ERROR_STR(allocCnt == totalJobCnt,
        "[GFX Error] PBRDeferredSkinnedPipeline::shadowDrawMT: not enough command lists.", false);
    if (allocCnt != totalJobCnt) {
        cmdListPool_->free(CommandListUsage::RenderingSlave, std::move(cmdCtxs));
        instancingGroups.clear();
        return;
    }

    for (std::size_t k = 0u; k + 1u < instancingGroups.size(); ++k) {
        const auto& de = *instancingGroups[k];
        layoutMeshIfNeeded(*de.mesh);
        auto pdd = ShadowMapSkinnedCSMShader::PerDrawcallData{
            .firstInstanceOffset = static_cast<u32t>(instancingGroups[k] - drawEvents_.begin())
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

void Dispatcher::gBufferIndirectUpdate() {
    gBufferUpdate();
}

void Dispatcher::gBufferIndirectUpdateMT() {
    gBufferUpdateMT();
}

void Dispatcher::gBufferIndirectDraw() {
    if (drawEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] PBRDeferredSkinnedPipeline::gBufferIndirectDraw: no command list.", false);
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
    pResources_->gBufferPass.boneData.bind(cmdList, rootParamIdxBoneData_, roomIdx_);
    pResources_->gBufferPass.perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);
    pResources_->hiZPass.visibleIndices.bindGraphicsAsSRV(cmdList, rootParamIdxSrcCnts0_, roomIdx_);

    u32t idxDC = 0u;
    auto gFirst = drawEvents_.begin();
    while (gFirst != drawEvents_.end()) {
        auto& de   = *gFirst;
        auto gLast = std::upper_bound(gFirst, drawEvents_.end(), de);

        pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].bind(cmdList, rootParamIdxPDD_, roomIdx_);

        auto pdd = PBRDeferredSkinnedGBufferShader::PerDrawcallData{
            .material = PBRDeferredSkinnedGBufferShader::Material{
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
        };
        pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);

        layoutMeshIfNeeded(*de.mesh);
        auto& vbViews = de.mesh->vbViewsByPipeline.at("PBRDeferredSkinnedPipeline_GBuffer");
        DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);
        DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&de.subMesh->ibView), false);

        static constexpr u32t kIndirectCmdStride = sizeof(u32t) + sizeof(DrawIndexedInstancedArgs);
        DISPLAY_ERROR_DX_VOID( cmdList->ExecuteIndirect(
            cmdSig_->get(), 1u, pResources_->hiZPass.indirectCmd.resource(roomIdx_),
            idxDC * kIndirectCmdStride, nullptr, 0u
        ), false );

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

void Dispatcher::gBufferIndirectDrawMT() {
    if (drawEvents_.empty()) return;

    static auto instancingGroups = std::vector<decltype(drawEvents_)::const_iterator>();
    auto it = drawEvents_.cbegin();
    while (it != drawEvents_.cend()) {
        instancingGroups.push_back(it);
        it = std::upper_bound(it, drawEvents_.cend(), *it);
    }
    instancingGroups.push_back(drawEvents_.cend());

    const std::size_t jobCnt = ((instancingGroups.size() - 1u) + (jobSizeDraw_ - 1u)) / jobSizeDraw_;
    auto latch = std::latch(jobCnt);

    std::list<CommandContext> cmdCtxs{};
    const auto allocCnt = cmdListPool_->alloc(jobCnt, CommandListUsage::RenderingSlave, cmdCtxs);
    DISPLAY_ERROR_STR(allocCnt == jobCnt,
        "[GFX Error] PBRDeferredSkinnedPipeline::gBufferDrawMT: not enough command lists.", false);
    if (allocCnt != jobCnt) {
        cmdListPool_->free(CommandListUsage::RenderingSlave, std::move(cmdCtxs));
        instancingGroups.clear();
        return;
    }

    std::size_t accDC   = 0u;
    auto        currCtx = cmdCtxs.begin();

    while (accDC + (jobSizeDraw_ - 1) < instancingGroups.size() - 1u) {
        DISPLAY_ERROR_DX_HR(currCtx->cmdAlloc->Reset(), false);
        DISPLAY_ERROR_DX_HR(currCtx->cmdList->Reset(currCtx->cmdAlloc.Get(), nullptr), false);
        addJobGBufferIndirectDraw(currCtx->cmdList.Get(), instancingGroups.data() + accDC,
            instancingGroups.data() + accDC + jobSizeDraw_, accDC, latch);
        accDC += jobSizeDraw_;
        ++currCtx;
    }
    if (accDC != instancingGroups.size() - 1u) {
        const auto last = instancingGroups.size() - 1u - accDC;
        DISPLAY_ERROR_DX_HR(currCtx->cmdAlloc->Reset(), false);
        DISPLAY_ERROR_DX_HR(currCtx->cmdList->Reset(currCtx->cmdAlloc.Get(), nullptr), false);
        addJobGBufferIndirectDraw(currCtx->cmdList.Get(), instancingGroups.data() + accDC,
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

void Dispatcher::gBufferUpdate() {
    if (drawEvents_.empty()) return;

    const auto& lastFlags    = pResources_->hiZPass.lastVisibilityFlags;
    const bool  hasLastFlags = (lastFlags.size() == drawEvents_.size());

    static auto perInstanceData = std::vector<PBRDeferredSkinnedGBufferShader::PerInstanceData>();
    perInstanceData.resize(drawEvents_.size());

    const auto view     = cameraData_.view;
    const auto viewProj = cameraData_.view * cameraData_.proj;

    u32t boneUploadCnt = 0u;
    for (u32t i = 0u; i < static_cast<u32t>(drawEvents_.size()); ++i) {
        const auto& e        = drawEvents_[i];
        const bool  isCulled = hasLastFlags && (lastFlags[i] == 0u);

        perInstanceData[i].rootBoneOffset = boneUploadCnt;

        if (!isCulled) {
            perInstanceData[i].world       = mu::transpose(e.world).getXmf();
            perInstanceData[i].wvp         = mu::transpose(e.world * viewProj).getXmf();
            perInstanceData[i].wv          = mu::transpose(e.world * view).getXmf();
            perInstanceData[i].wvNormal    = mu::inverse(mu::Mat3x3(e.world * view)).getXmf();
            perInstanceData[i].worldNormal = mu::inverse(mu::Mat3x3(e.world)).getXmf();
        }

        boneUploadCnt += static_cast<u32t>(e.boneXforms.size());
    }

    pResources_->gBufferPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();

    // boneData: static 재사용. culled 인스턴스 슬롯은 이전 프레임 값 유지 (어차피 드로우 안 됨).
    static auto boneData = std::vector<PBRDeferredSkinnedGBufferShader::BoneData>();
    boneData.resize(boneUploadCnt);
    u32t boneIdx = 0u;
    for (u32t i = 0u; i < static_cast<u32t>(drawEvents_.size()); ++i) {
        const bool isCulled = hasLastFlags && (lastFlags[i] == 0u);
        if (!isCulled) {
            for (auto& bx : drawEvents_[i].boneXforms) {
                boneData[boneIdx] = PBRDeferredSkinnedGBufferShader::BoneData{
                    mu::transpose(bx).getXmf() };
                ++boneIdx;
            }
        } else {
            boneIdx += static_cast<u32t>(drawEvents_[i].boneXforms.size());
        }
    }
    pResources_->gBufferPass.boneData.stage(roomIdx_, boneData);
    boneData.clear();

    static auto lightData = std::vector<PBRDeferredSkinnedGBufferShader::Light>();
    lightData.resize(lightData_.size());
    std::ranges::transform(lightData_, lightData.begin(),
        [view](const LightData& ld) {
            return PBRDeferredSkinnedGBufferShader::Light{
                .color     = ld.color.getXmf(),
                .falloff   = ld.falloff,
                .posV      = mu::Vec3(mu::Vec4(ld.pos, 1.f) * view).getXmf(),
                .cosTheta  = ld.cosTheta,
                .dirV      = mu::NVec3(mu::Vec4(ld.dir, 0.f) * view).getXmf(),
                .cosPhi    = ld.cosPhi,
                .atten     = ld.atten.getXmf(),
                .intensity = ld.intensity,
                .type      = etoi(ld.type)
            };
        });

    const auto& csmData = SharedResources::ShadowMap::csmShadowMapData.at(
        std::string(SharedResources::ShadowMap::kDefaultKey))[roomIdx_];

    PBRDeferredSkinnedGBufferShader::PerFrameData pfd{};
    pfd.globalAmbient = frameData_.globalAmbient.getXmf();
    pfd.lightCnt      = static_cast<u32t>(lightData.size());
    pfd.cascadeCount  = mainDirectionalLightData_.cascadeCount;
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

void Dispatcher::gBufferUpdateMT() {
    if (drawEvents_.empty()) return;

    static auto perInstanceData = std::vector<PBRDeferredSkinnedGBufferShader::PerInstanceData>();
    perInstanceData.resize(drawEvents_.size());

    auto latch = std::latch(drawEvents_.size() / jobSizeUpdate_
        + ((drawEvents_.size() % jobSizeUpdate_) != 0));

    const auto viewProj = cameraData_.view * cameraData_.proj;

    const auto& lastFlags    = pResources_->hiZPass.lastVisibilityFlags;
    const bool  hasLastFlags = (lastFlags.size() == drawEvents_.size());

    std::size_t accEventCnt = 0u;
    while (accEventCnt + (jobSizeUpdate_ - 1) < drawEvents_.size()) {
        addJobGBufferUpdate(cameraData_.view, viewProj,
            drawEvents_.data() + accEventCnt,
            drawEvents_.data() + accEventCnt + jobSizeUpdate_,
            hasLastFlags ? lastFlags.data() + accEventCnt : nullptr,
            perInstanceData.data() + accEventCnt, latch);
        accEventCnt += jobSizeUpdate_;
    }
    if (accEventCnt != drawEvents_.size()) {
        addJobGBufferUpdate(cameraData_.view, viewProj,
            drawEvents_.data() + accEventCnt,
            drawEvents_.data() + drawEvents_.size(),
            hasLastFlags ? lastFlags.data() + accEventCnt : nullptr,
            perInstanceData.data() + accEventCnt, latch);
    }

    // lightData and pfd are small — no MT
    static auto lightData = std::vector<PBRDeferredSkinnedGBufferShader::Light>();
    lightData.resize(lightData_.size());
    const auto view = cameraData_.view;
    std::ranges::transform(lightData_, lightData.begin(),
        [view](const LightData& ld) {
            return PBRDeferredSkinnedGBufferShader::Light{
                .color     = ld.color.getXmf(),
                .falloff   = ld.falloff,
                .posV      = mu::Vec3(mu::Vec4(ld.pos, 1.f) * view).getXmf(),
                .cosTheta  = ld.cosTheta,
                .dirV      = mu::NVec3(mu::Vec4(ld.dir, 0.f) * view).getXmf(),
                .cosPhi    = ld.cosPhi,
                .atten     = ld.atten.getXmf(),
                .intensity = ld.intensity,
                .type      = etoi(ld.type)
            };
        });

    const auto& csmData = SharedResources::ShadowMap::csmShadowMapData.at(
        std::string(SharedResources::ShadowMap::kDefaultKey))[roomIdx_];

    PBRDeferredSkinnedGBufferShader::PerFrameData pfd{};
    pfd.globalAmbient = frameData_.globalAmbient.getXmf();
    pfd.lightCnt      = static_cast<u32t>(lightData.size());
    pfd.cascadeCount  = mainDirectionalLightData_.cascadeCount;
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

    // rootBoneOffset must be computed sequentially — fix up after parallel world-xform pass
    for (std::size_t i = 1u; i < drawEvents_.size(); ++i) {
        perInstanceData[i].rootBoneOffset = static_cast<u32t>(
            drawEvents_[i-1].boneXforms.size() + perInstanceData[i-1].rootBoneOffset
        );
    }

    static auto boneData = std::vector<PBRDeferredSkinnedGBufferShader::BoneData>();
    boneData.resize(perInstanceData.back().rootBoneOffset
        + static_cast<u32t>(drawEvents_.back().boneXforms.size()));
    for (u32t i = 0u; i < static_cast<u32t>(drawEvents_.size()); ++i) {
        const bool isCulled = hasLastFlags && (lastFlags[i] == 0u);
        const auto& e = drawEvents_[i];
        u32t boneIdx = perInstanceData[i].rootBoneOffset;
        if (!isCulled) {
            for (auto& bx : e.boneXforms) {
                boneData[boneIdx] = PBRDeferredSkinnedGBufferShader::BoneData{
                    mu::transpose(bx).getXmf() };
                ++boneIdx;
            }
        }
    }
    pResources_->gBufferPass.boneData.stage(roomIdx_, boneData);
    boneData.clear();

    pResources_->gBufferPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();
}

void Dispatcher::gBufferDraw() {
    if (drawEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] PBRDeferredSkinnedPipeline::gBufferDraw: no command list.", false);
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
    pResources_->gBufferPass.boneData.bind(cmdList, rootParamIdxBoneData_, roomIdx_);
    pResources_->gBufferPass.perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);

    u32t idxDC = 0u;
    auto gFirst = drawEvents_.begin();
    while (gFirst != drawEvents_.end()) {
        auto& de   = *gFirst;
        auto gLast = std::upper_bound(gFirst, drawEvents_.end(), de);

        cmdList->SetGraphicsRoot32BitConstant( rootParamIdxFirstInstOffset_,
            static_cast<u32t>(gFirst - drawEvents_.begin()), 0u
        );

        pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].bind(cmdList, rootParamIdxPDD_, roomIdx_);

        auto pdd = PBRDeferredSkinnedGBufferShader::PerDrawcallData{
            .material = PBRDeferredSkinnedGBufferShader::Material{
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
        };
        pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);

        layoutMeshIfNeeded(*de.mesh);
        auto& vbViews = de.mesh->vbViewsByPipeline.at("PBRDeferredSkinnedPipeline_GBuffer");
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
    if (drawEvents_.empty()) return;

    static auto instancingGroups = std::vector<decltype(drawEvents_)::const_iterator>();
    auto it = drawEvents_.cbegin();
    while (it != drawEvents_.cend()) {
        instancingGroups.push_back(it);
        it = std::upper_bound(it, drawEvents_.cend(), *it);
    }
    instancingGroups.push_back(drawEvents_.cend());

    const std::size_t jobCnt = ((instancingGroups.size() - 1u) + (jobSizeDraw_ - 1u)) / jobSizeDraw_;
    auto latch = std::latch(jobCnt);

    std::list<CommandContext> cmdCtxs{};
    const auto allocCnt = cmdListPool_->alloc(jobCnt, CommandListUsage::RenderingSlave, cmdCtxs);
    DISPLAY_ERROR_STR(allocCnt == jobCnt,
        "[GFX Error] PBRDeferredSkinnedPipeline::gBufferDrawMT: not enough command lists.", false);
    if (allocCnt != jobCnt) {
        cmdListPool_->free(CommandListUsage::RenderingSlave, std::move(cmdCtxs));
        instancingGroups.clear();
        return;
    }

    std::size_t accDC   = 0u;
    auto        currCtx = cmdCtxs.begin();

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
    const u32t* pVisFlags,
    PBRDeferredSkinnedGBufferShader::PerInstanceData* pOut,
    std::latch& latch
) {
    // NOTE: rootBoneOffset is set to 0 here; fixed up sequentially after latch.wait()
    threadPool_->addJob([=, &latch]() {
        const std::ptrdiff_t cnt = pLast - pFirst;
        for (std::ptrdiff_t i = 0; i < cnt; ++i) {
            const DrawEvent& e        = pFirst[i];
            const bool       isCulled = pVisFlags && (pVisFlags[i] == 0u);
            pOut[i].rootBoneOffset = 0u;  // fixed up sequentially after latch
            if (!isCulled) {
                pOut[i].world       = mu::transpose(e.world).getXmf();
                pOut[i].wvp         = mu::transpose(e.world * viewProj).getXmf();
                pOut[i].wv          = mu::transpose(e.world * view).getXmf();
                pOut[i].wvNormal    = mu::inverse(mu::Mat3x3(e.world * view)).getXmf();
                pOut[i].worldNormal = mu::inverse(mu::Mat3x3(e.world)).getXmf();
            }
        }
        latch.count_down();
    });
}

void Dispatcher::addJobGBufferIndirectDraw(
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
        pResources_->gBufferPass.boneData.bind(threadCmdList, rootParamIdxBoneData_, roomIdx_);
        pResources_->hiZPass.visibleIndices.bindGraphicsAsSRV(threadCmdList, rootParamIdxSrcCnts0_, roomIdx_);

        std::size_t idxDC = firstDrawcallIdx;
        auto pGroup = pItFirst;

        while (pGroup != pItLast) {
            auto gFirst = *pGroup;
            auto gLast  = *(pGroup + 1);
            const auto& de = *gFirst;

            pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].bind(
                threadCmdList, rootParamIdxPDD_, roomIdx_);

            auto pdd = PBRDeferredSkinnedGBufferShader::PerDrawcallData{
                .material = PBRDeferredSkinnedGBufferShader::Material{
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
                }
            };
            pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);

            layoutMeshIfNeeded(*de.mesh);
            auto& vbViews = de.mesh->vbViewsByPipeline.at("PBRDeferredSkinnedPipeline_GBuffer");
            DISPLAY_ERROR_DX_VOID(threadCmdList->IASetVertexBuffers(
                0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);
            DISPLAY_ERROR_DX_VOID(threadCmdList->IASetIndexBuffer(&de.subMesh->ibView), false);

            static constexpr u32t kIndirectCmdStride = sizeof(u32t) + sizeof(DrawIndexedInstancedArgs);
            DISPLAY_ERROR_DX_VOID( threadCmdList->ExecuteIndirect(
                cmdSig_->get(), 1u, pResources_->hiZPass.indirectCmd.resource(roomIdx_),
                static_cast<u32t>(idxDC) * kIndirectCmdStride, nullptr, 0u
            ), false );

            ++idxDC;
            ++pGroup;
        }

        transitionResourceState(threadCmdList,
            pResources_->hiZPass.visibleIndices.resource(roomIdx_),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transitionResourceState(threadCmdList,
            pResources_->hiZPass.indirectCmd.resource(roomIdx_),
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        DISPLAY_ERROR_DX_HR(threadCmdList->Close(), false);
        latch.count_down();
    });
}

void Dispatcher::addJobGBufferDraw( ID3D12GraphicsCommandList* threadCmdList,
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
        pResources_->gBufferPass.boneData.bind(threadCmdList, rootParamIdxBoneData_, roomIdx_);

        std::size_t idxDC = firstDrawcallIdx;
        auto pGroup = pItFirst;

        while (pGroup != pItLast) {
            auto gFirst = *pGroup;
            auto gLast  = *(pGroup + 1);
            const auto& de = *gFirst;
            
            threadCmdList->SetGraphicsRoot32BitConstant( rootParamIdxFirstInstOffset_,
                static_cast<u32t>(gFirst - drawEvents_.begin()), 0u
            );

            pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].bind(
                threadCmdList, rootParamIdxPDD_, roomIdx_);

            auto pdd = PBRDeferredSkinnedGBufferShader::PerDrawcallData{
                .material = PBRDeferredSkinnedGBufferShader::Material{
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
                }
            };
            pResources_->gBufferPass.perDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);

            layoutMeshIfNeeded(*de.mesh);
            auto& vbViews = de.mesh->vbViewsByPipeline.at("PBRDeferredSkinnedPipeline_GBuffer");
            DISPLAY_ERROR_DX_VOID(threadCmdList->IASetVertexBuffers(
                0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);
            DISPLAY_ERROR_DX_VOID(threadCmdList->IASetIndexBuffer(&de.subMesh->ibView), false);

            const auto stride = de.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT ? sizeof(u16t) : sizeof(u32t);
            DISPLAY_ERROR_DX_VOID( threadCmdList->DrawIndexedInstanced(
                static_cast<UINT>(de.subMesh->ibView.SizeInBytes / stride),
                static_cast<UINT>(gLast - gFirst), 0u, 0, 0u
            ), false );

            ++idxDC;
            ++pGroup;
        }

        DISPLAY_ERROR_DX_HR(threadCmdList->Close(), false);
        latch.count_down();
    });
}

void Dispatcher::addJobShadowUpdate(
    const DrawEvent* pFirst, const DrawEvent* pLast,
    ShadowMapSkinnedCSMShader::PerInstanceData* pOut, std::latch& latch
) {
    // NOTE: rootBoneOffset is set to 0 here; fixed up sequentially after latch.wait()
    threadPool_->addJob([=, &latch]() {
        std::transform(pFirst, pLast, pOut,
            [](const DrawEvent& e) {
                return ShadowMapSkinnedCSMShader::PerInstanceData{
                    .world          = mu::transpose(e.world).getXmf(),
                    .rootBoneOffset = 0u  // fixed up sequentially after latch
                };
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
        pResources_->shadowPass.boneData.bind(threadCmdList, rootParamIdxBoneData_, roomIdx_);
        pResources_->shadowPass.perFrameData.cbuffers[cascadeIdx].bind(threadCmdList, rootParamIdxPFD_, roomIdx_);

        std::size_t idxDC = firstDrawcallIdx;
        auto pGroup = pItFirst;

        while (pGroup != pItLast) {
            auto gFirst = *pGroup;
            auto gLast  = *(pGroup + 1);
            const auto& de = *gFirst;

            pResources_->shadowPass.perDrawcallData.cbuffers[idxDC].bind(
                threadCmdList, rootParamIdxPDD_, roomIdx_);

            auto pdd = ShadowMapSkinnedCSMShader::PerDrawcallData{
                .firstInstanceOffset = static_cast<u32t>(gFirst - drawEvents_.begin())
            };
            pResources_->shadowPass.perDrawcallData.cbuffers[idxDC].stage(roomIdx_, &pdd, 1u);

            auto& vbViews = de.mesh->vbViewsByPipeline.at("PBRDeferredSkinnedPipeline_Shadow");
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

}  // namespace PBRDeferredSkinnedPipeline
