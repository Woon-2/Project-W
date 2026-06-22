#include "pch.hpp"
#include "minimapPropPipeline.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace MinimapPropPipeline {

void layoutMeshIfNeeded(const Mesh& mesh) {
    if (mesh.vbViewsByPipeline.contains("MinimapPropPipeline")) {
        return;
    }

    auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("MinimapPropPipeline");
    auto& vbViews = pvbViews->second;
    vbViews.reserve(2u);   // Position, UV

    const auto checkVB = [&](const std::string& key) {
        DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(key),
            "[GFX Error] MinimapPropPipeline::layoutMeshIfNeeded: VB key '" + key + "' not found.", false);
    };

    checkVB(mesh.name + "_VB_Position");
    checkVB(mesh.name + "_VB_UV");

    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Position")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_UV")]);
}

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
    Fence* pFence,
    Resources* pResources,
    CommandListPool* commandListPool,
    std::vector<DrawEvent>&& drawEvents,
    const mu::Mat4x4& viewProj,
    std::size_t roomIdx
) : descriptorHeaps_(descriptorHeaps),
    pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
    pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
    rootSig_(rootSig), shader_(shader), cmdQ_(cmdQ),
    viewport_(viewport), scissorRect_(scissorRect), rtv_(rtv),
    pFence_(pFence), pResources_(pResources), cmdListPool_(commandListPool),
    drawEvents_(std::move(drawEvents)), viewProj_(viewProj), roomIdx_(roomIdx),
    rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
    rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
    rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
    rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
    rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
    rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool")) {}

void Dispatcher::render() {
    if (drawEvents_.empty()) {
        return;
    }

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR( cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] MinimapPropPipeline::render: no command list available.", false
    );
    if (!cmdCtx.cmdList) {
        return;
    }
    auto cmdList  = cmdCtx.cmdList.Get();
    auto cmdAlloc = cmdCtx.cmdAlloc.Get();
    if (cmdAlloc->Reset() < 0 || cmdList->Reset(cmdAlloc, nullptr) < 0) {
        cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
        return;
    }

    DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);

    auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
    std::ranges::transform(descriptorHeaps_, descriptorHeapsRaw.begin(),
        [](ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
    DISPLAY_ERROR_DX_VOID( cmdList->SetDescriptorHeaps(
        static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()
    ), false );

    pTexPool_->bind(cmdList, rootParamIdxTexPool_);
    pTexArrayPool_->bind(cmdList, rootParamIdxTexArrayPool_);
    pTexCubePool_->bind(cmdList, rootParamIdxTexCubePool_);
    pSamPool_->bind(cmdList, rootParamIdxSamPool_);
    pCmpSamPool_->bind(cmdList, rootParamIdxCmpSamPool_);

    cmdList->SetPipelineState(shader_.Get());
    cmdList->OMSetRenderTargets(1u, &rtv_, false, nullptr);
    cmdList->RSSetViewports(1u, &viewport_);
    cmdList->RSSetScissorRects(1u, &scissorRect_);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const std::size_t count = std::min<std::size_t>(drawEvents_.size(), kMaxDrawEvents);
    for (std::size_t i = 0u; i < count; ++i) {
        const auto& ev = drawEvents_[i];
        if (!ev.mesh || !ev.subMesh || !ev.material) continue;

        auto pdd = MinimapPropShader::PerDrawcallData{
            .wvp = mu::transpose(ev.world * viewProj_).getXmf()
        };
        pdd.idxAlbedo   = ev.material->mapAlbedo.idxSrv;
        pdd.tint        = ev.material->constantAlbedo;
        pdd.alphaCutoff = ev.material->constantAlphaCutoff;

        pResources_->perDrawcallData.cbuffers[i].stage(roomIdx_, &pdd, 1u);
        pResources_->perDrawcallData.cbuffers[i].bind(cmdList, rootParamIdxPDD_, roomIdx_);

        layoutMeshIfNeeded(*ev.mesh);
        const auto& vbViews = ev.mesh->vbViewsByPipeline.at("MinimapPropPipeline");
        DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(
            0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);

        DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&ev.subMesh->ibView), false);

        const UINT indexCount = ev.subMesh->ibView.SizeInBytes / sizeof(u32t);
        DISPLAY_ERROR_DX_VOID(cmdList->DrawIndexedInstanced(indexCount, 1u, 0u, 0, 0u), false);
    }

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR( hrClose, false );
    if (hrClose < 0) {
        cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
        return;
    }

    ID3D12CommandList* staged[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, staged), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

}   // namespace MinimapPropPipeline
