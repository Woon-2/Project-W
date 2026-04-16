#include "pch.hpp"
#include "blendCGMeshPipeline.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace BlendCGMeshPipeline {

Dispatcher::Dispatcher(
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
    ThreadPool* threadPool,
    CommandListPool* commandListPool,
    std::vector<DrawEvent>&& drawEvents,
    const CameraData& cameraData,
    const FrameData& frameData,
    std::size_t roomIdx
) : descriptorHeaps_(descriptorHeaps),
    pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
    pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
    rootSig_(rootSig), shader_(shader), cmdQ_(cmdQ),
    viewport_(viewport), scissorRect_(scissorRect), rtv_(rtv), dsv_(dsv),
    pFence_(pFence), pResources_(pResources), threadPool_(threadPool),
    cmdListPool_(commandListPool), drawEvents_(std::move(drawEvents)),
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

    std::sort(drawEvents_.begin(), drawEvents_.end());

    static auto perInstanceData = std::vector<BlendCGMeshShader::PerInstanceData>();
    perInstanceData.resize(drawEvents_.size());

    std::ranges::transform(drawEvents_, perInstanceData.begin(),
        [](const DrawEvent& e) {
            return BlendCGMeshShader::PerInstanceData{
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

    for (std::size_t i = 0; i < drawEvents_.size(); ++i) {
        const auto& e = drawEvents_[i];

        auto pdd = BlendCGMeshShader::PerDrawcallData{
            .idxMainTex = e.pMainTex ? e.pMainTex->idxSrv : BindlessIndex{},
            .idxNoiseTex = e.pNoiseTex ? e.pNoiseTex->idxSrv : BindlessIndex{},
            .idxFlowTex = e.pFlowTex ? e.pFlowTex->idxSrv : BindlessIndex{},
            .idxMaskTex = e.pMaskTex ? e.pMaskTex->idxSrv : BindlessIndex{},
            .idxCameraDepthTex = e.pCameraDepthTex ? e.pCameraDepthTex->idxSrv : BindlessIndex{},
            .firstInstanceOffset = static_cast<u32t>(i),
            .hasNoiseTex = e.pNoiseTex ? 1u : 0u,
            .hasFlowTex = e.pFlowTex ? 1u : 0u,
            .hasMaskTex = e.pMaskTex ? 1u : 0u,
            .hasCameraDepthTex = e.pCameraDepthTex ? 1u : 0u,
            .time = frameData_.time,
            .cameraNear = frameData_.cameraNear,
            .cameraFar = frameData_.cameraFar,
            .mainTexST = e.mainTexST.getXmf(),
            .noiseTexST = e.noiseTexST.getXmf(),
            .flowTexST = e.flowTexST.getXmf(),
            .maskTexST = e.maskTexST.getXmf(),
            .speedMainTexUVNoiseZW = e.speedMainTexUVNoiseZW.getXmf(),
            .distortionSpeedXYPowerZ = e.distortionSpeedXYPowerZ.getXmf(),
            .color = e.color.getXmf(),
            .uvRect = { 0.f, 0.f, 1.f, 1.f },
            .emission = e.emission,
            .opacity = e.opacity,
            .textureOpacity = e.textureOpacity,
            .multiplyTexture = e.multiplyTexture,
            .useOnlyColor = e.useOnlyColor,
            .useFresnel = e.useFresnel,
            .fresnelPower = e.fresnelPower,
            .fresnelScale = e.fresnelScale,
            .useCenterGlow = e.useCenterGlow,
            .useDepth = e.useDepth,
            .depthPower = e.depthPower,
            .pad0 = 0.f,
        };
        pResources_->perDrawcallData.cbuffers[i].stage(roomIdx_, &pdd, 1u);
    }

    const auto viewProj = cameraData_.view * cameraData_.proj;
    auto pfd = BlendCGMeshShader::PerFrameData{
        .matViewProj = mu::transpose(viewProj).getXmf(),
        .cameraPosW = cameraData_.pos.getXmf(),
        .pad1 = 0.f,
    };
    pResources_->perFrameData.stage(roomIdx_, &pfd, 1u);
}

void Dispatcher::drawSingleThreaded() {
    if (drawEvents_.empty()) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] BlendCGMeshPipeline::drawSingleThreaded: failed to alloc command list", false);
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
        if (!e.pMesh || !e.pSubMesh || !e.pMainTex) {
            ++idxDrawcall;
            continue;
        }

        if (e.pMesh->vbViewsByPipeline.find("BlendCGMeshPipeline") == e.pMesh->vbViewsByPipeline.end()) {
            auto& views = e.pMesh->vbViewsByPipeline["BlendCGMeshPipeline"];
            views.resize(3u);
            views[0] = e.pMesh->vbViews[e.pMesh->vbIdxMap.at(e.pMesh->name + "_VB_Position")];
            views[1] = e.pMesh->vbViews[e.pMesh->vbIdxMap.at(e.pMesh->name + "_VB_UV")];
            views[2] = e.pMesh->vbViews[e.pMesh->vbIdxMap.at(e.pMesh->name + "_VB_Color")];
        }
        const auto& vbViews = e.pMesh->vbViewsByPipeline.at("BlendCGMeshPipeline");

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

}  // namespace BlendCGMeshPipeline
