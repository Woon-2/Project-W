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
    RenderSubmitter* submitter,
    const D3D12_VIEWPORT& viewport,
    const D3D12_RECT& scissorRect,
    Fence* pFence,
    Resources* pResources, ThreadPool* threadPool,
    CommandListPool* commandListPool,
    std::vector<DrawEvent>&& drawEvents,
    std::vector<LightData>&& lightData,
    const LightData& mainDirectionalLightData,
    const CameraData& cameraData, const FrameData& frameData,
    std::size_t roomIdx, u32t visibilitySlot
) : descriptorHeaps_(descriptorHeaps),
    pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
    pTexCubePool_(pTexCubePool), pSamPool_(pSamPool),
    pCmpSamPool_(pCmpSamPool), pDsvPool_(pDsvPool),
    rootSig_(rootSig), cmdSig_(cmdSig),
    hiZClearShader_(hiZClearShader),
    hiZCullShader_(hiZCullShader), prefixSumShader_(prefixSumShader),
    hiZCompactShader_(hiZCompactShader), hiZCommandShader_(hiZCommandShader),
    gBufferShader_(gBufferShader), shadowShader_(shadowShader),
    submitter_(submitter), viewport_(viewport), scissorRect_(scissorRect),
    rtvGB_{}, dsvGB_(SharedResources::GBuffer::gBufferData[roomIdx].dsvHandle),
    pFence_(pFence), pResources_(pResources),
    threadPool_(threadPool), cmdListPool_(commandListPool),
    drawEvents_(std::move(drawEvents)),
    lightData_(std::move(lightData)),
    mainDirectionalLightData_(mainDirectionalLightData),
    cameraData_(cameraData), frameData_(frameData),
    roomIdx_(roomIdx), visibilitySlot_(visibilitySlot),
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

    gBufferEvents_.clear();
    shadowEvents_.clear();
    for (const auto& e : drawEvents_) {
        if (!e.viewFrustumCulled) gBufferEvents_.push_back(e);
        if (!e.shadowCulled)      shadowEvents_.push_back(e);
    }

    instanceGroups_.clear();
    auto it = gBufferEvents_.cbegin();
    while (it != gBufferEvents_.cend()) {
        instanceGroups_.push_back(static_cast<u32t>( std::distance(gBufferEvents_.cbegin(), it) ));
        it = std::upper_bound(it, gBufferEvents_.cend(), *it);
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
    // visibility feedback populate:
    // 직전 프레임들이 써둔 2-slot ring을 읽어 objectVisibility를 재구성한다.
    // same-parity 슬롯(= 이번에 덮어쓸 슬롯 = N-2)은 전역 프레임 펜스(N-2 대기)로
    // 완성·coherent 가 보장되고, 나머지 슬롯은 in-flight라 torn read 가능하나
    // OR-only 합산 + objId bounds-check 덕분에 최악이 보수적(과다 visible)으로 안전하다.
    // 슬롯 엔트리는 objId가 패킹돼 self-describing 하므로 프레임 간 instance 순서와 무관.
    {
        auto& hiZ = pResources_->hiZPass;
        const u64t slotBytes = static_cast<u64t>(Resources::HiZPass::MAX_HIZ_INSTANCES) * sizeof(u32t);
        if (hiZ.visibilityFeedback.hasReadback() && !hiZ.objectVisibility.empty()) {
            if (hiZ.slotEntryCount[0] == 0u && hiZ.slotEntryCount[1] == 0u) {
                // 아직 readback 데이터가 없음(시작 프레임 등) → 무엇이 가려졌는지 모르므로
                // 보수적으로 전부 visible 처리(컬링 안 함).
                std::fill(hiZ.objectVisibility.begin(), hiZ.objectVisibility.end(), true);
                hiZ.lastVisibleCount = 0u;
                hiZ.lastTotalCount   = 0u;
            } else {
                std::fill(hiZ.objectVisibility.begin(), hiZ.objectVisibility.end(), false);
                for (u32t s = 0u; s < 2u; ++s) {
                    const u32t cnt = hiZ.slotEntryCount[s];
                    const auto* p = hiZ.visibilityFeedback.readbackPtr<u32t>(0u, s * slotBytes);
                    for (u32t i = 0u; i < cnt; ++i) {
                        const u32t v = p[i];
                        const u32t oid = v >> 1u;
                        if ((v & 1u) && oid < static_cast<u32t>(hiZ.objectVisibility.size()))
                            hiZ.objectVisibility[oid] = true;
                    }
                }
                // getHiZStats()용 per-instance 카운트: 완성이 보장된 same-parity 슬롯 기준.
                const u32t total = hiZ.slotEntryCount[visibilitySlot_];
                const auto* pc = hiZ.visibilityFeedback.readbackPtr<u32t>(0u, visibilitySlot_ * slotBytes);
                u32t vis = 0u;
                for (u32t i = 0u; i < total; ++i) vis += (pc[i] & 1u);
                hiZ.lastVisibleCount = vis;
                hiZ.lastTotalCount   = total;
            }
        }
    }

    if (gBufferEvents_.empty()) return;

    // 1. Hi-Z Cull Pass 입력 스테이징 (objId 패킹용 instanceObjId 포함)
    static auto perInstanceDataCull = std::vector<HiZCullShader::PerInstanceData>();
    perInstanceDataCull.resize(gBufferEvents_.size());

    for (u32t i = 0u, gid = 0u; i < gBufferEvents_.size(); ++i) {
        if (i >= instanceGroups_[gid + 1u]) {
            ++gid;
        }
        auto& e = gBufferEvents_[i];

        // 스킨/랙돌 포즈를 따라가는 월드 공간 cull AABB가 있으면 identity world로 투영한다.
        // 없으면 rest-pose mesh->bounds × world로 폴백(정적/리지드 메시).
        if (e.hasWorldCullBounds) {
            perInstanceDataCull[i] = HiZCullShader::PerInstanceData{
                .world = mu::transpose(mu::Mat4x4{}).getXmf(),   // identity
                .aabbMin = e.worldCullMin.getXmf(),
                .aabbMax = e.worldCullMax.getXmf(),
                .instanceGroupId = gid,
                .instanceObjId = e.renderObjectId
            };
        }
        else {
            perInstanceDataCull[i] = HiZCullShader::PerInstanceData{
                .world = mu::transpose(e.world).getXmf(),
                .aabbMin = (e.mesh->bounds.center - e.mesh->bounds.size * 0.5f).getXmf(),
                .aabbMax = (e.mesh->bounds.center + e.mesh->bounds.size * 0.5f).getXmf(),
                .instanceGroupId = gid,
                .instanceObjId = e.renderObjectId
            };
        }
    }

    pResources_->hiZPass.perInstanceDataCull.stage(roomIdx_, perInstanceDataCull);
    perInstanceDataCull.clear();

    const auto pfdCull = HiZCullShader::PerFrameData{
        .viewProj = mu::transpose(cameraData_.view * cameraData_.proj).getXmf(),
        .screenSize = XMFLOAT2(viewport_.Width, viewport_.Height),
        .objCnt = static_cast<u32t>(gBufferEvents_.size()),
    };
    pResources_->hiZPass.perFrameDataCull.stage(roomIdx_, &pfdCull, 1u);

    // 2. Hi-Z Compact Pass
    static auto perInstanceDataCompact = std::vector<HiZCompactShader::PerInstanceData>();
    perInstanceDataCompact.resize(gBufferEvents_.size());

    for (u32t i = 0u, gid = 0u; i < gBufferEvents_.size(); ++i) {
        if (i >= instanceGroups_[gid + 1u]) {
            ++gid;
        }
        auto& e = gBufferEvents_[i];

        const auto stride = e.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT ? sizeof(u16t) : sizeof(u32t);
        perInstanceDataCompact[i] = HiZCompactShader::PerInstanceData{
            .instanceGroupId = gid,
            .idxCnt = static_cast<u32t>( e.subMesh->ibView.SizeInBytes / stride )
        };
    }

    pResources_->hiZPass.perInstanceDataCompact.stage(roomIdx_, perInstanceDataCompact);
    perInstanceDataCompact.clear();

    const auto pfdCompact = HiZCompactShader::PerFrameData{
        .objCnt = static_cast<u32t>( gBufferEvents_.size() )
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
    if (gBufferEvents_.empty()) {
        // 이번 프레임 슬롯엔 아무것도 기록하지 않으므로, 다음 read 시 stale 데이터를
        // 읽지 않도록 CPU측 엔트리 수를 0으로 갱신한다.
        pResources_->hiZPass.slotEntryCount[visibilitySlot_] = 0u;
        return;
    }

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
    // visibility feedback: 단일 리소스 내 이번 프레임 슬롯을 offset으로 u3(OutPerGroupData)에 바인딩.
    // cull 패스에서 OutPerGroupData(perGroupData)는 사용되지 않으므로 슬롯 재사용에 충돌 없음.
    {
        const u64t slotBytes = static_cast<u64t>(Resources::HiZPass::MAX_HIZ_INSTANCES) * sizeof(u32t);
        pResources_->hiZPass.visibilityFeedback.bindCompute(
            cmdList, rootParamIdxOutPerGroupData_, 0u, visibilitySlot_ * slotBytes);
    }

    static constexpr auto cullDispatchUnit = 64u;
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        static_cast<u32t>( (gBufferEvents_.size() + cullDispatchUnit - 1u) / cullDispatchUnit ), 1u, 1u
    ), false );

    pResources_->hiZPass.perGroupCnt.uavBarrier(cmdList, roomIdx_);
    pResources_->hiZPass.visibleFlags.uavBarrier(cmdList, roomIdx_);

    // 2. Prefix Sum Pass
    // 단일 스레드그룹 내부 chunked scan으로 임의의 group 수를 처리한다(셰이더가 groupCnt를
    // 읽어 chunk 순회). groupCnt(b1)는 clear 패스용 PerFrameData와 레이아웃·값이 같아 재사용.
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(prefixSumShader_.Get()), false);
    pResources_->hiZPass.perFrameDataClear.bindCompute(cmdList, rootParamIdxPFD_, roomIdx_);
    pResources_->hiZPass.perGroupCnt.bindComputeAsSRV(cmdList, rootParamIdxSrcCnts0_, roomIdx_);
    pResources_->hiZPass.groupOffsets.bindCompute(cmdList, rootParamIdxDestCnts0_, roomIdx_);

    const auto groupCnt = instanceGroups_.size() - 1u;
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(1u, 1u, 1u), false );

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
        static_cast<u32t>( (gBufferEvents_.size() + compactDispatchUnit - 1u) / compactDispatchUnit ), 1u, 1u
    ), false );

    pResources_->hiZPass.perGroupData.uavBarrier(cmdList, roomIdx_);
    pResources_->hiZPass.visibleIndices.uavBarrier(cmdList, roomIdx_);

    // 4. Hi-Z Command Pass
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(hiZCommandShader_.Get()), false);
    pResources_->hiZPass.perFrameDataCommand.bindCompute(cmdList, rootParamIdxPFD_, roomIdx_);
    pResources_->hiZPass.perGroupData.bindComputeAsSRV(cmdList, rootParamIdxPerGroupData_, roomIdx_);
    pResources_->hiZPass.indirectCmd.bindCompute(cmdList, rootParamIdxIndirectCmd_, roomIdx_);

    // Command 패스는 그룹당 1엔트리 기록(스캔 아님)이라 multi-threadgroup이 정상.
    static constexpr auto commandDispatchUnit = 64u;
    DISPLAY_ERROR_DX_VOID( cmdList->Dispatch(
        static_cast<u32t>( (groupCnt + commandDispatchUnit - 1u) / commandDispatchUnit ), 1u, 1u
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

    // visibility feedback readback: cull이 기록한 이번 프레임 슬롯을 단일 readback 리소스의
    // 같은 슬롯 offset으로 복사한다. cull(u3) 쓰기 이후 어떤 패스도 visibilityFeedback을
    // 건드리지 않으므로(여전히 UAV 상태), copy의 UAV→COPY_SOURCE 전환이 곧 쓰기 완료 동기화점이다.
    // 전용 fence는 걸지 않는다 — coherency는 전역 프레임 펜스(N-2 대기)가 제공한다.
    {
        const u64t slotBytes  = static_cast<u64t>(Resources::HiZPass::MAX_HIZ_INSTANCES) * sizeof(u32t);
        const u64t slotOffset = visibilitySlot_ * slotBytes;
        pResources_->hiZPass.visibilityFeedback.copyToReadback(
            cmdList, 0u, gBufferEvents_.size() * sizeof(u32t),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, slotOffset, slotOffset
        );
        pResources_->hiZPass.slotEntryCount[visibilitySlot_] = static_cast<u32t>(gBufferEvents_.size());
    }

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR(hrClose, false);
    if (hrClose < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    ID3D12CommandList* staged[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, staged), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

// ============================================================
// Shadow pass implementation (identical to PBRSkinnedPipeline, CSM variant)
// ============================================================

void Dispatcher::shadowUpdate() {
    if (shadowEvents_.empty()) return;

    static auto perInstanceData = std::vector<ShadowMapSkinnedCSMShader::PerInstanceData>();
    perInstanceData.resize(shadowEvents_.size());

    u32t boneUploadCnt = 0u;
    std::ranges::transform(shadowEvents_, perInstanceData.begin(),
        [&boneUploadCnt](const DrawEvent& e) -> ShadowMapSkinnedCSMShader::PerInstanceData {
            const auto ret = ShadowMapSkinnedCSMShader::PerInstanceData{
                .world          = mu::transpose(e.world).getXmf(),
                .rootBoneOffset = boneUploadCnt,
                .bakedClipId    = e.bakedClipId,
                .bakedClipFrame = e.bakedClipFrame
            };
            if (e.bakedClipId == -1) {
                boneUploadCnt += static_cast<u32t>(e.boneXforms.size());
            }
            return ret;
        });

    pResources_->shadowPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();

    static auto boneData = std::vector<ShadowMapSkinnedCSMShader::BoneData>();
    boneData.resize(boneUploadCnt);
    auto itBone = boneData.begin();
    std::ranges::for_each(shadowEvents_, [&itBone](const DrawEvent& e) {
        if (e.bakedClipId == -1) {
            for (auto& bx : e.boneXforms) {
                *itBone = ShadowMapSkinnedCSMShader::BoneData{ mu::transpose(bx).getXmf() };
                ++itBone;
            }
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
        pfd.camPos     = mainDirectionalLightData_.cascadeCameraPos.getXmf();  // camera-relative caster rebase
        pResources_->shadowPass.perFrameData.cbuffers[ci].stage(roomIdx_, &pfd, 1u);
    }
}

void Dispatcher::shadowUpdateMT() {
    if (shadowEvents_.empty()) return;

    static auto perInstanceData = std::vector<ShadowMapSkinnedCSMShader::PerInstanceData>();
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
        ShadowMapSkinnedCSMShader::PerFrameData pfd{};
        pfd.lightVP = mu::transpose(
            mainDirectionalLightData_.cascadeViews[ci] * mainDirectionalLightData_.cascadeProjs[ci]
        ).getXmf();
        pfd.cascadeIdx = ci;
        pfd.camPos     = mainDirectionalLightData_.cascadeCameraPos.getXmf();  // camera-relative caster rebase
        pResources_->shadowPass.perFrameData.cbuffers[ci].stage(roomIdx_, &pfd, 1u);
    }

    latch.wait();

    // rootBoneOffset prefix-sum (sequential); all events in shadowEvents_ contribute bones
    for (std::size_t i = 1u; i < shadowEvents_.size(); ++i) {
        perInstanceData[i].rootBoneOffset = perInstanceData[i-1].rootBoneOffset;
        if (shadowEvents_[i-1].bakedClipId == -1) {
            perInstanceData[i].rootBoneOffset += static_cast<u32t>( shadowEvents_[i-1].boneXforms.size() );
        }
    }

    u32t boneUploadCnt = perInstanceData.back().rootBoneOffset;
    if (shadowEvents_.back().bakedClipId == -1) {
        boneUploadCnt += static_cast<u32t>( shadowEvents_.back().boneXforms.size() );
    }

    static auto boneData = std::vector<ShadowMapSkinnedCSMShader::BoneData>();
    boneData.resize(boneUploadCnt);

    auto itBone = boneData.begin();
    std::ranges::for_each(shadowEvents_, [&itBone](const DrawEvent& e) {
        if (e.bakedClipId == -1) {
            for (auto& bx : e.boneXforms) {
                *itBone = ShadowMapSkinnedCSMShader::BoneData{ mu::transpose(bx).getXmf() };
                ++itBone;
            }
        }
    });
    pResources_->shadowPass.boneData.stage(roomIdx_, boneData);
    boneData.clear();

    pResources_->shadowPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();
}

void Dispatcher::shadowDraw() {
    if (shadowEvents_.empty()) return;

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
        auto gFirst = shadowEvents_.begin();
        while (gFirst != shadowEvents_.end()) {
            auto& de   = *gFirst;
            auto gLast = std::upper_bound(gFirst, shadowEvents_.end(), de);

            pResources_->shadowPass.perDrawcallData.cbuffers[idxDC].bind(cmdList, rootParamIdxPDD_, roomIdx_);
            auto pdd = ShadowMapSkinnedCSMShader::PerDrawcallData{
                .firstInstanceOffset = static_cast<u32t>(gFirst - shadowEvents_.begin())
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
    DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, staged), false);
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
    DISPLAY_ERROR_DX_VOID(submitter_->submit(static_cast<UINT>(staged.size()), staged.data()), false);
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
    if (gBufferEvents_.empty()) return;

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
    pResources_->gBufferPass.boneData.bind(cmdList, rootParamIdxBoneData_, roomIdx_);
    pResources_->gBufferPass.perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);
    pResources_->hiZPass.visibleIndices.bindGraphicsAsSRV(cmdList, rootParamIdxSrcCnts0_, roomIdx_);

    u32t idxDC = 0u;
    auto gFirst = gBufferEvents_.begin();
    while (gFirst != gBufferEvents_.end()) {
        auto& de   = *gFirst;
        auto gLast = std::upper_bound(gFirst, gBufferEvents_.end(), de);

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
                .cAlphaCutoff          = de.material->constantAlphaCutoff,
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
    DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, staged), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

void Dispatcher::gBufferIndirectDrawMT() {
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
    DISPLAY_ERROR_DX_VOID(submitter_->submit(static_cast<UINT>(staged.size()), staged.data()), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
        .splice(pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].end(), std::move(cmdCtxs));
}

void Dispatcher::gBufferUpdate() {
    if (gBufferEvents_.empty()) return;

    static auto perInstanceData = std::vector<PBRDeferredSkinnedGBufferShader::PerInstanceData>();
    perInstanceData.resize(gBufferEvents_.size());

    const auto view     = cameraData_.view;
    const auto viewProj = cameraData_.view * cameraData_.proj;

    u32t boneUploadCnt = 0u;
    for (u32t i = 0u; i < static_cast<u32t>(gBufferEvents_.size()); ++i) {
        const auto& e              = gBufferEvents_[i];
        perInstanceData[i].rootBoneOffset = boneUploadCnt;
        perInstanceData[i].world          = mu::transpose(e.world).getXmf();
        perInstanceData[i].wvp            = mu::transpose(e.world * viewProj).getXmf();
        perInstanceData[i].wv             = mu::transpose(e.world * view).getXmf();
        perInstanceData[i].wvNormal       = mu::inverse(mu::Mat3x3(e.world * view)).getXmf();
        perInstanceData[i].worldNormal    = mu::inverse(mu::Mat3x3(e.world)).getXmf();
        perInstanceData[i].bakedClipId    = e.bakedClipId;
        perInstanceData[i].bakedClipFrame = e.bakedClipFrame;
        perInstanceData[i].rippleCount    = e.rippleCount;
        for (int r = 0; r < DrawEvent::kMaxRipples; ++r) {
            perInstanceData[i].ripplePosAge[r]         = e.ripplePosAge[r].getXmf();
            perInstanceData[i].rippleColorIntensity[r] = e.rippleColorIntensity[r].getXmf();
        }
        if (e.bakedClipId == -1) {
            boneUploadCnt += static_cast<u32t>(e.boneXforms.size());
        }
    }

    pResources_->gBufferPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();

    static auto boneData = std::vector<PBRDeferredSkinnedGBufferShader::BoneData>();
    boneData.resize(boneUploadCnt);
    u32t boneIdx = 0u;
    for (const auto& e : gBufferEvents_) {
        if (e.bakedClipId == -1) {
            for (auto& bx : e.boneXforms) {
                boneData[boneIdx++] = PBRDeferredSkinnedGBufferShader::BoneData{ mu::transpose(bx).getXmf() };
            }
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
    if (gBufferEvents_.empty()) return;

    static auto perInstanceData = std::vector<PBRDeferredSkinnedGBufferShader::PerInstanceData>();
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

    // rootBoneOffset prefix-sum (sequential); all gBufferEvents_ contribute bones
    for (std::size_t i = 1u; i < gBufferEvents_.size(); ++i) {
        perInstanceData[i].rootBoneOffset = perInstanceData[i-1].rootBoneOffset;
        if (gBufferEvents_[i-1].bakedClipId == -1) {
            perInstanceData[i].rootBoneOffset += static_cast<u32t>( gBufferEvents_[i-1].boneXforms.size() );
        }
    }

    u32t boneUploadCnt = perInstanceData.back().rootBoneOffset;
    if (gBufferEvents_.back().bakedClipId == -1) {
        boneUploadCnt += static_cast<u32t>( gBufferEvents_.back().boneXforms.size() );
    }

    static auto boneData = std::vector<PBRDeferredSkinnedGBufferShader::BoneData>();
    boneData.resize(boneUploadCnt);
    for (u32t i = 0u; i < static_cast<u32t>(gBufferEvents_.size()); ++i) {
        const auto& e    = gBufferEvents_[i];
        u32t         boneIdx = perInstanceData[i].rootBoneOffset;
        if (e.bakedClipId == -1) {
            for (auto& bx : e.boneXforms) {
                boneData[boneIdx++] = PBRDeferredSkinnedGBufferShader::BoneData{ mu::transpose(bx).getXmf() };
            }
        }
    }
    pResources_->gBufferPass.boneData.stage(roomIdx_, boneData);
    boneData.clear();

    pResources_->gBufferPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();
}

void Dispatcher::gBufferDraw() {
    if (gBufferEvents_.empty()) return;

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
    pResources_->gBufferPass.boneData.bind(cmdList, rootParamIdxBoneData_, roomIdx_);
    pResources_->gBufferPass.perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);

    u32t idxDC = 0u;
    auto gFirst = gBufferEvents_.begin();
    while (gFirst != gBufferEvents_.end()) {
        auto& de   = *gFirst;
        auto gLast = std::upper_bound(gFirst, gBufferEvents_.end(), de);

        cmdList->SetGraphicsRoot32BitConstant( rootParamIdxFirstInstOffset_,
            static_cast<u32t>(gFirst - gBufferEvents_.begin()), 0u
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
                .cAlphaCutoff          = de.material->constantAlphaCutoff,
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
    DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, staged), false);
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
    DISPLAY_ERROR_DX_VOID(submitter_->submit(static_cast<UINT>(staged.size()), staged.data()), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
        .splice(pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].end(), std::move(cmdCtxs));
}

// ============================================================
// Thread job helpers
// ============================================================

void MU_CALLCONV Dispatcher::addJobGBufferUpdate(
    mu::Mat4x4 view, const mu::Mat4x4& viewProj,
    const DrawEvent* pFirst, const DrawEvent* pLast,
    PBRDeferredSkinnedGBufferShader::PerInstanceData* pOut,
    std::latch& latch
) {
    // NOTE: rootBoneOffset is set to 0 here; fixed up sequentially after latch.wait()
    threadPool_->addJob([=, &latch]() {
        const std::ptrdiff_t cnt = pLast - pFirst;
        for (std::ptrdiff_t i = 0; i < cnt; ++i) {
            const DrawEvent& e     = pFirst[i];
            pOut[i].rootBoneOffset = 0u;  // fixed up sequentially after latch
            pOut[i].world          = mu::transpose(e.world).getXmf();
            pOut[i].wvp            = mu::transpose(e.world * viewProj).getXmf();
            pOut[i].wv             = mu::transpose(e.world * view).getXmf();
            pOut[i].wvNormal       = mu::inverse(mu::Mat3x3(e.world * view)).getXmf();
            pOut[i].worldNormal    = mu::inverse(mu::Mat3x3(e.world)).getXmf();
            pOut[i].bakedClipId    = e.bakedClipId;
            pOut[i].bakedClipFrame = e.bakedClipFrame;
            pOut[i].rippleCount    = e.rippleCount;
            for (int r = 0; r < DrawEvent::kMaxRipples; ++r) {
                pOut[i].ripplePosAge[r]         = e.ripplePosAge[r].getXmf();
                pOut[i].rippleColorIntensity[r] = e.rippleColorIntensity[r].getXmf();
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
                    .cAlphaCutoff          = de.material->constantAlphaCutoff,
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
        pResources_->gBufferPass.boneData.bind(threadCmdList, rootParamIdxBoneData_, roomIdx_);

        std::size_t idxDC = firstDrawcallIdx;
        auto pGroup = pItFirst;

        while (pGroup != pItLast) {
            auto gFirst = *pGroup;
            auto gLast  = *(pGroup + 1);
            const auto& de = *gFirst;

            threadCmdList->SetGraphicsRoot32BitConstant( rootParamIdxFirstInstOffset_,
                static_cast<u32t>(gFirst - gBufferEvents_.begin()), 0u
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
                    .cAlphaCutoff          = de.material->constantAlphaCutoff,
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
    // rootBoneOffset is set to 0 here; fixed up sequentially after latch.wait()
    threadPool_->addJob([=, &latch]() {
        std::transform(pFirst, pLast, pOut,
            [](const DrawEvent& e) -> ShadowMapSkinnedCSMShader::PerInstanceData {
                return ShadowMapSkinnedCSMShader::PerInstanceData{
                    .world          = mu::transpose(e.world).getXmf(),
                    .rootBoneOffset = 0u,
                    .bakedClipId    = e.bakedClipId,
                    .bakedClipFrame = e.bakedClipFrame
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
