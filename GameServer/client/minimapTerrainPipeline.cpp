#include "pch.hpp"
#include "minimapTerrainPipeline.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace MinimapTerrainPipeline {

void layoutMeshIfNeeded(const Mesh& mesh) {
    if (mesh.vbViewsByPipeline.contains("MinimapTerrainPipeline")) {
        return;
    }

    auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("MinimapTerrainPipeline");
    auto& vbViews = pvbViews->second;
    vbViews.reserve(2u);   // Position, UV

    const auto checkVB = [&](const std::string& key) {
        DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(key),
            "[GFX Error] MinimapTerrainPipeline::layoutMeshIfNeeded: VB key '" + key + "' not found.", false);
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
    RenderSubmitter* submitter,
    const D3D12_VIEWPORT& viewport,
    const D3D12_RECT& scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE rtv,
    Fence* pFence,
    Resources* pResources,
    CommandListPool* commandListPool,
    std::vector<DrawEvent>&& drawEvents,
    const CameraData& cameraData,
    std::size_t roomIdx
) : descriptorHeaps_(descriptorHeaps),
    pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
    pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
    rootSig_(rootSig), shader_(shader), submitter_(submitter),
    viewport_(viewport), scissorRect_(scissorRect), rtv_(rtv),
    pFence_(pFence), pResources_(pResources), cmdListPool_(commandListPool),
    drawEvents_(std::move(drawEvents)), cameraData_(cameraData), roomIdx_(roomIdx),
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
        "[GFX Error] MinimapTerrainPipeline::render: no command list available.", false
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

    const mu::Mat4x4 vp = cameraData_.view * cameraData_.proj;

    for (std::size_t i = 0u; i < drawEvents_.size() && i < kMaxDrawEvents; ++i) {
        const auto& ev = drawEvents_[i];
        if (!ev.terrain) continue;
        const TerrainData& terrain = *ev.terrain;

        auto pdd = MinimapTerrainShader::PerDrawcallData{
            .wvp = mu::transpose(ev.world * vp).getXmf()
        };
        pdd.idxSplatMap = terrain.splatMap.idxSrv;
        pdd.layerCount  = std::min(terrain.layerCount, MinimapTerrainShader::MAX_TERRAIN_LAYERS);
        for (int li = 0; li < pdd.layerCount; ++li) {
            pdd.idxDiffuse[li] = terrain.layers[li].diffuse.idxSrv;
            pdd.tiling[li] = XMFLOAT4{
                terrain.layers[li].tileSizeX, terrain.layers[li].tileSizeY,
                terrain.layers[li].tileOffsetX, terrain.layers[li].tileOffsetY
            };
        }

        pResources_->perDrawcallData.cbuffers[i].stage(roomIdx_, &pdd, 1u);
        pResources_->perDrawcallData.cbuffers[i].bind(cmdList, rootParamIdxPDD_, roomIdx_);

        layoutMeshIfNeeded(terrain.mesh);
        const auto& vbViews = terrain.mesh.vbViewsByPipeline.at("MinimapTerrainPipeline");
        DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(
            0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);

        const auto& subMesh = terrain.mesh.subMeshes[0];
        DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&subMesh.ibView), false);

        const UINT indexCount = subMesh.ibView.SizeInBytes / sizeof(u32t);
        DISPLAY_ERROR_DX_VOID(cmdList->DrawIndexedInstanced(indexCount, 1u, 0u, 0, 0u), false);
    }

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR( hrClose, false );
    if (hrClose < 0) {
        cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
        return;
    }

    ID3D12CommandList* staged[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, staged), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

}   // namespace MinimapTerrainPipeline
