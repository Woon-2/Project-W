#include "pch.hpp"
#include "terrainPipeline.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace TerrainPipeline {

// ---------------------------------------------------------------------------
// layoutMeshIfNeeded
// ---------------------------------------------------------------------------

void layoutMeshIfNeeded(const Mesh& mesh) {
    if (mesh.vbViewsByPipeline.contains("TerrainPipeline")) {
        return;
    }

    auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("TerrainPipeline");
    auto& vbViews = pvbViews->second;
    vbViews.reserve(3u);    // Position, Normal, UV

    const auto checkVB = [&](const std::string& key) {
        DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(key),
            "[GFX Error] TerrainPipeline::layoutMeshIfNeeded: VB key '" + key + "' not found.", false);
    };

    checkVB(mesh.name + "_VB_Position");
    checkVB(mesh.name + "_VB_Normal");
    checkVB(mesh.name + "_VB_UV");

    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Position")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Normal")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_UV")]);
}

// ---------------------------------------------------------------------------
// Dispatcher constructor
// ---------------------------------------------------------------------------

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
    const LightData& lightData, const CameraData& cameraData, const FrameData& frameData,
    std::size_t roomIdx
) : descriptorHeaps_(descriptorHeaps),
    pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
    pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
    rootSig_(rootSig), shader_(shader), cmdQ_(cmdQ),
    viewport_(viewport), scissorRect_(scissorRect), rtv_(rtv), dsv_(dsv),
    pFence_(pFence), pResources_(pResources), threadPool_(threadPool), cmdListPool_(commandListPool),
    drawEvents_(std::move(drawEvents)),
    lightData_(lightData), cameraData_(cameraData), frameData_(frameData),
    roomIdx_(roomIdx),
    rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
    rootParamIdxPFD_(rootSig->paramIdx("PerFrameData")),
    rootParamIdxLightData_(rootSig->paramIdx("LightData")),
    rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
    rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
    rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
    rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
    rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool"))
{}

// ---------------------------------------------------------------------------
// mainPass / mainPassMT
// ---------------------------------------------------------------------------

void Dispatcher::mainPass() {
    mainUpdate();
    mainDraw();
}

void Dispatcher::mainPassMT() {
    mainUpdateMT();
    mainDrawMT();
}

// ---------------------------------------------------------------------------
// mainUpdate / mainUpdateMT
// ---------------------------------------------------------------------------

void Dispatcher::mainUpdate() {
    if (drawEvents_.empty()) {
        return;
    }

    auto pfd = TerrainShader::PerFrameData{};
    pfd.globalAmbient = frameData_.globalAmbient.getXmf();
    pfd.lightCnt      = frameData_.lightCount;
    pfd.idxShadowMap  = frameData_.idxShadowMap;

    auto lightVP = lightData_.view * lightData_.proj;
    pfd.lightVP  = mu::transpose(lightVP).getXmf();

    pResources_->mainPass.perFrameData.stage(roomIdx_, &pfd, 1u);
}

// Terrain pipeline typically has only 1~4 draw events.
// Parallelizing PerFrameData staging over such a small count yields no
// measurable benefit and introduces unnecessary thread-pool overhead
// (job enqueue + latch sync).
// This function is provided for API symmetry with PBRPipeline::mainPassMT()
// only; it delegates directly to the single-threaded implementation.
void Dispatcher::mainUpdateMT() {
    mainUpdate();
}

// ---------------------------------------------------------------------------
// mainDraw / mainDrawMT
// ---------------------------------------------------------------------------

void Dispatcher::mainDraw() {
    if (drawEvents_.empty()) {
        return;
    }

    // Allocate a command context.
    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] TerrainPipeline::mainDraw: no command context available.", false);
    if (!cmdCtx.cmdList) {
        return;
    }

    auto* cmdList  = cmdCtx.cmdList.Get();
    auto* cmdAlloc = cmdCtx.cmdAlloc.Get();

    auto hrReset = cmdAlloc->Reset();
    DISPLAY_ERROR_DX_HR(hrReset, false);
    if (hrReset < 0) {
        cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
        return;
    }
    auto hrListReset = cmdList->Reset(cmdAlloc, nullptr);
    DISPLAY_ERROR_DX_HR(hrListReset, false);
    if (hrListReset < 0) {
        cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
        return;
    }

    // Pipeline state
    DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(shader_.Get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(1u, &rtv_, false, &dsv_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &viewport_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &scissorRect_), false);

    // Bind descriptor heaps
    auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
    std::ranges::transform(descriptorHeaps_, heapsRaw.begin(),
        [](const ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
    DISPLAY_ERROR_DX_VOID(cmdList->SetDescriptorHeaps(
        static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

    pTexPool_->bind(cmdList, rootParamIdxTexPool_);
    pTexArrayPool_->bind(cmdList, rootParamIdxTexArrayPool_);
    pTexCubePool_->bind(cmdList, rootParamIdxTexCubePool_);
    pSamPool_->bind(cmdList, rootParamIdxSamPool_);
    pCmpSamPool_->bind(cmdList, rootParamIdxCmpSamPool_);
    pResources_->mainPass.lightData.bind(cmdList, rootParamIdxLightData_, roomIdx_);

    DISPLAY_ERROR_DX_VOID(cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);

    // Bind PerFrameData (staged in mainUpdate).
    pResources_->mainPass.perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);

    // Iterate draw events (usually just one terrain)
    for (const auto& ev : drawEvents_) {
        if (!ev.terrain) continue;
        const auto& terrain = *ev.terrain;
        const auto& mesh    = terrain.mesh;
        if (mesh.subMeshes.empty()) continue;

        // Build PerDrawcallData.
        // perDrawcallData uses a single ConstantBuffer slot, so it must be
        // staged and bound immediately before each draw call.
        auto pdd = TerrainShader::PerDrawcallData{};

        const auto& worldMat = ev.world;
        auto view = cameraData_.view;
        auto proj = cameraData_.proj;
        auto wvp  = worldMat * view * proj;
        auto wv   = worldMat * view;
        pdd.wvp   = mu::transpose(wvp).getXmf();
        pdd.world = mu::transpose(worldMat).getXmf();
        pdd.wv    = mu::transpose(wv).getXmf();

        // Splat map
        pdd.idxSplatMap = terrain.splatMap.idxSrv;

        // Per-layer texture indices and tiling
        pdd.layerCount = terrain.layerCount;
        const int maxLayers = std::min(static_cast<int>(terrain.layers.size()), TerrainShader::MAX_TERRAIN_LAYERS);
        for (int i = 0; i < maxLayers; ++i) {
            const auto& layer  = terrain.layers[i];
            pdd.idxDiffuse[i]  = layer.diffuse.idxSrv;
            pdd.idxNormal[i]   = layer.normalMap.idxSrv;
            pdd.tiling[i]      = XMFLOAT4(
                terrain.sizeX / layer.tileSizeX,
                terrain.sizeZ / layer.tileSizeY,
                layer.tileOffsetX,
                layer.tileOffsetY
            );
        }
        // Fill remaining slots with invalid indices
        for (int i = maxLayers; i < TerrainShader::MAX_TERRAIN_LAYERS; ++i) {
            pdd.idxDiffuse[i].idxRange = -1;
            pdd.idxNormal[i].idxRange  = -1;
            pdd.tiling[i]              = XMFLOAT4(1.f, 1.f, 0.f, 0.f);
        }

        pResources_->mainPass.perDrawcallData.stage(roomIdx_, &pdd, 1u);
        pResources_->mainPass.perDrawcallData.bind(cmdList, rootParamIdxPDD_, roomIdx_);

        // Bind vertex and index buffers
        layoutMeshIfNeeded(mesh);
        const auto& vbViews = mesh.vbViewsByPipeline.at("TerrainPipeline");
        DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(
            0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);

        const auto& subMesh = mesh.subMeshes[0];
        DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&subMesh.ibView), false);

        // 32-bit index buffer: SizeInBytes / 4
        const UINT indexCount = subMesh.ibView.SizeInBytes / sizeof(u32t);
        DISPLAY_ERROR_DX_VOID(cmdList->DrawIndexedInstanced(indexCount, 1u, 0u, 0, 0u), false);
    }

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR(hrClose, false);
    if (hrClose < 0) {
        cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
        return;
    }

    ID3D12CommandList* lists[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, lists), false);

    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
        .push_back(std::move(cmdCtx));
}

// Terrain pipeline typically has only 1~4 draw events.
// Recording draw calls for such a small count on a worker thread yields no
// measurable benefit and introduces unnecessary thread-pool overhead
// (job enqueue + latch sync + multi-list submission).
// This function is provided for API symmetry with PBRPipeline::mainPassMT()
// only; it delegates directly to the single-threaded implementation.
void Dispatcher::mainDrawMT() {
    mainDraw();
}

}   // namespace TerrainPipeline
