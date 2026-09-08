#include "pch.hpp"
#include "energyOrbPipeline.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace EnergyOrbPipeline {

Dispatcher::Dispatcher(
    const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
    DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
    DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
    DescriptorPool* pCmpSamPool,
    const std::shared_ptr<RootSig>& rootSig,
    const ComPtr<ID3D12PipelineState>& shader,
    RenderSubmitter* submitter,
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
    rootSig_(rootSig), shader_(shader), submitter_(submitter),
    viewport_(viewport), scissorRect_(scissorRect), rtv_(rtv), dsv_(dsv),
    pFence_(pFence), pResources_(pResources),
    threadPool_(threadPool), cmdListPool_(commandListPool),
    drawEvents_(std::move(drawEvents)),
    cameraData_(cameraData), frameData_(frameData), roomIdx_(roomIdx),
    rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
    rootParamIdxPID_(rootSig->paramIdx("PerInstanceData")),
    rootParamIdxPFD_(rootSig->paramIdx("PerFrameData")),
    rootParamIdxBoneData_(rootSig->paramIdx("BoneData")),
    rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
    rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
    rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
    rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
    rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool"))
{
    // Cap to the GPU buffer capacity so update (perInstanceData/boneData stage) and draw
    // (perDrawcallData.cbuffers[idx]) never overrun their fixed-size buffers. Dropping
    // excess orbs degrades gracefully instead of an out-of-bounds access violation.
    if (drawEvents_.size() > kMaxOrbDrawcalls) {
        gSharedLog << "[EnergyOrb] " << drawEvents_.size() << " orbs exceed capacity "
                   << kMaxOrbDrawcalls << "; dropping the surplus this frame.\n";
        drawEvents_.resize(kMaxOrbDrawcalls);
    }
}

void Dispatcher::updateGPUDataSingleThreaded() {
    if (drawEvents_.empty()) return;

    static auto perInstanceData = std::vector<EnergyOrbShader::PerInstanceData>();
    perInstanceData.resize(drawEvents_.size());

    u32t boneUploadCnt = 0u;
    std::ranges::transform(drawEvents_, perInstanceData.begin(),
        [&boneUploadCnt](const DrawEvent& e) {
            auto ret = EnergyOrbShader::PerInstanceData{
                .world              = mu::transpose(e.world).getXmf(),
                .colorAndSize       = XMFLOAT4{ e.colorHDR.x(), e.colorHDR.y(), e.colorHDR.z(), e.pointSize },
                .sphereCenterRadius = XMFLOAT4{ e.sphereCenter.x(), e.sphereCenter.y(), e.sphereCenter.z(), e.sphereRadius },
                .rootBoneOffset     = boneUploadCnt,
                .morphT             = e.morphT,
                .vertexCount        = e.vertexCount,
                .pad                = 0.f,
            };
            boneUploadCnt += static_cast<u32t>(e.boneXforms.size());
            return ret;
        }
    );

    pResources_->perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();

    // Death-pose bone palettes, concatenated and indexed by rootBoneOffset.
    static auto boneData = std::vector<EnergyOrbShader::BoneData>();
    boneData.resize(boneUploadCnt);
    auto itBoneOut = boneData.begin();
    for (const auto& e : drawEvents_) {
        for (const auto& boneXform : e.boneXforms) {
            *itBoneOut = EnergyOrbShader::BoneData{ mu::transpose(boneXform).getXmf() };
            ++itBoneOut;
        }
    }
    if (boneUploadCnt > 0u)
        pResources_->boneData.stage(roomIdx_, boneData);
    boneData.clear();

    const auto vp = cameraData_.view * cameraData_.proj;
    auto pfd = EnergyOrbShader::PerFrameData{
        .vp         = mu::transpose(vp).getXmf(),
        .cameraPosW = cameraData_.pos.getXmf(),
        .padding0   = 0.f,
    };
    pResources_->perFrameData.stage(roomIdx_, &pfd, 1u);
}

void Dispatcher::drawSingleThreaded() {
    if (drawEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] EnergyOrbPipeline::drawSingleThreaded: failed to alloc command list", false);
    if (!cmdCtx.cmdList) return;

    auto* cmdList = cmdCtx.cmdList.Get();
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

    DISPLAY_ERROR_DX_VOID(cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST), false);

    pResources_->perInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);
    pResources_->boneData.bind(cmdList, rootParamIdxBoneData_, roomIdx_);
    pResources_->perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);

    u32t idxDrawcall = 0u;
    const u32t drawcallCap = static_cast<u32t>(pResources_->perDrawcallData.cbuffers.size());

    for (const auto& e : drawEvents_) {
        // Defensive: never index past the allocated per-drawcall CBs (capacity is also
        // enforced by the constructor's truncation; this guards against any mismatch).
        if (idxDrawcall >= drawcallCap) break;
        // Lazily build the pipeline-specific VB view array: Position + skinning attrs.
        if (e.pMesh->vbViewsByPipeline.find("EnergyOrbPipeline") == e.pMesh->vbViewsByPipeline.end()) {
            auto& views = e.pMesh->vbViewsByPipeline["EnergyOrbPipeline"];
            views.resize(4u);
            views[0] = e.pMesh->vbViews[e.pMesh->vbIdxMap.at(e.pMesh->name + "_VB_Position")];
            views[1] = e.pMesh->vbViews[e.pMesh->vbIdxMap.at(e.pMesh->name + "_VB_BoneIndices")];
            views[2] = e.pMesh->vbViews[e.pMesh->vbIdxMap.at(e.pMesh->name + "_VB_BoneWeights")];
            views[3] = e.pMesh->vbViews[e.pMesh->vbIdxMap.at(e.pMesh->name + "_VB_UV")];
        }
        const auto& vbViews = e.pMesh->vbViewsByPipeline.at("EnergyOrbPipeline");

        pResources_->perDrawcallData.cbuffers[idxDrawcall].bind(cmdList, rootParamIdxPDD_, roomIdx_);

        auto pdd = EnergyOrbShader::PerDrawcallData{
            .idxAlbedo           = e.pAlbedo ? e.pAlbedo->idxSrv : BindlessIndex{ -1, -1, -1, -1 },
            .firstInstanceOffset = idxDrawcall,
            .pad0                = XMUINT3{ 0u, 0u, 0u },
        };
        pResources_->perDrawcallData.cbuffers[idxDrawcall].stage(roomIdx_, &pdd, 1u);

        DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);
        DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&e.pSubMesh->ibView), false);

        // One point per submesh index (shared vertices collapse to the same target).
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
    DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, stagedCmdLists), false);

    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

}  // namespace EnergyOrbPipeline
