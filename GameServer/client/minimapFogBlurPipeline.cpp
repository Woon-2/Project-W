#include "pch.hpp"
#include "minimapFogBlurPipeline.hpp"
#include "shader.hpp"
#include "sharedResources.hpp"
#include "errorHandling.hpp"

namespace MinimapFogBlurPipeline {

Dispatcher::Dispatcher(
    const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
    DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
    DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
    DescriptorPool* pCmpSamPool,
    const std::shared_ptr<RootSig>& rootSig,
    const ComPtr<ID3D12PipelineState>& blurShader,
    const ComPtr<ID3D12CommandQueue>& cmdQ,
    Fence* pFence,
    Resources* pResources,
    CommandListPool* commandListPool,
    std::size_t roomIdx,
    float blurRadiusTexels
) : descriptorHeaps_(descriptorHeaps),
    pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
    pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
    rootSig_(rootSig), blurShader_(blurShader), cmdQ_(cmdQ),
    pFence_(pFence), pResources_(pResources), cmdListPool_(commandListPool),
    roomIdx_(roomIdx), blurRadiusTexels_(blurRadiusTexels),
    rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
    rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
    rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
    rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
    rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
    rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool")) {}

void Dispatcher::render() {
    if (SharedResources::Minimap::minimapData.empty()) {
        return;
    }
    auto& m = SharedResources::Minimap::minimapData[roomIdx_];

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR( cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] MinimapFogBlurPipeline::render: no command list available.", false
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

    cmdList->SetPipelineState(blurShader_.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const float texelSize = 1.f / static_cast<float>(m.size);
    auto vp = D3D12_VIEWPORT{ 0.f, 0.f, static_cast<float>(m.size), static_cast<float>(m.size), 0.f, 1.f };
    auto sc = D3D12_RECT{ 0, 0, static_cast<LONG>(m.size), static_cast<LONG>(m.size) };
    cmdList->RSSetViewports(1u, &vp);
    cmdList->RSSetScissorRects(1u, &sc);

    auto doPass = [&](u32t pass, const BindlessIndex& src, D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, bool horizontal) {
        auto pdd = MinimapFogBlurShader::PerDrawcallData{
            .idxSrc            = src,
            .srcTexelSize      = XMFLOAT2(texelSize, texelSize),
            .blurRadiusTexels  = blurRadiusTexels_,
            .horizontal        = horizontal ? 1u : 0u
        };
        pResources_->perDrawcallData.cbuffers[pass].stage(roomIdx_, &pdd, 1u);
        cmdList->OMSetRenderTargets(1u, &dstRtv, false, nullptr);
        pResources_->perDrawcallData.cbuffers[pass].bind(cmdList, rootParamIdxPDD_, roomIdx_);
        cmdList->DrawInstanced(3u, 1u, 0u, 0u);
    };

    // Pass 1 (horizontal): texA (terrain cache result, SRV) -> texB (RTV).
    SharedResources::Minimap::transitionToRead(roomIdx_, /*useA=*/true, cmdList);
    SharedResources::Minimap::transitionToWrite(roomIdx_, /*useA=*/false, cmdList);
    doPass(0u, m.texA.idxSrv, m.rtvB, /*horizontal=*/true);

    // Pass 2 (vertical + composite): texB (SRV) -> texA (RTV). Final result lands in texA.
    SharedResources::Minimap::transitionToRead(roomIdx_, /*useA=*/false, cmdList);
    SharedResources::Minimap::transitionToWrite(roomIdx_, /*useA=*/true, cmdList);
    doPass(1u, m.texB.idxSrv, m.rtvA, /*horizontal=*/false);

    // Leave texA readable for the minimap UI quad to sample.
    SharedResources::Minimap::transitionToRead(roomIdx_, /*useA=*/true, cmdList);

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

}   // namespace MinimapFogBlurPipeline
