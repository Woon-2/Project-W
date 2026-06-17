#include "pch.hpp"
#include "terrainDeferredPipeline.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"
#include "sharedResources.hpp"

namespace TerrainDeferredPipeline {

// Terrain Pipeline Occluder 패스의 input layout을 위한 Vertex Buffer View 배열이
// mesh에 존재하지 않는다면, 추가한다.
// 0: position
void __layoutMeshIfNeededOccluderPass(const Mesh& mesh) {
	if (mesh.vbViewsByPipeline.contains("TerrainPipeline_Occluder")) {
		return;
	}

	auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("TerrainPipeline_Occluder");
	auto& vbViews = pvbViews->second;
	vbViews.reserve(1u);	// position

	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_Position"),
		"[GFX Error] TerrainPipeline::__layoutMeshIfNeededShadowPass: " + mesh.name + "_VB_Position"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);

	auto& vbViewPos = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_Position") ];

	vbViews.push_back(vbViewPos);
}

// Terrain Pipeline 그림자 패스의 input layout을 위한 Vertex Buffer View 배열이
// mesh에 존재하지 않는다면, 추가한다.
// 0: position
void __layoutMeshIfNeededShadowPass(const Mesh& mesh) {
	if (mesh.vbViewsByPipeline.contains("TerrainPipeline_Shadow")) {
		return;
	}

	auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("TerrainPipeline_Shadow");
	auto& vbViews = pvbViews->second;
	vbViews.reserve(1u);	// position

	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_Position"),
		"[GFX Error] TerrainPipeline::__layoutMeshIfNeededShadowPass: " + mesh.name + "_VB_Position"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);

	auto& vbViewPos = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_Position") ];

	vbViews.push_back(vbViewPos);
}

void __layoutMeshIfNeededMainPass(const Mesh& mesh) {
    if (mesh.vbViewsByPipeline.contains("TerrainPipeline_Main")) {
        return;
    }

    auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("TerrainPipeline_Main");
    auto& vbViews = pvbViews->second;
    vbViews.reserve(5u);    // Position, Normal, Tangent, Bitangent, UV

    const auto checkVB = [&](const std::string& key) {
        DISPLAY_ERROR_STR(mesh.vbIdxMap.contains(key),
            "[GFX Error] TerrainPipeline::layoutMeshIfNeeded: VB key '" + key + "' not found.", false);
    };

    checkVB(mesh.name + "_VB_Position");
    checkVB(mesh.name + "_VB_Normal");
    checkVB(mesh.name + "_VB_Tangent");
    checkVB(mesh.name + "_VB_Bitangent");
    checkVB(mesh.name + "_VB_UV");

    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Position")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Normal")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Tangent")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_Bitangent")]);
    vbViews.push_back(mesh.vbViews[mesh.vbIdxMap.at(mesh.name + "_VB_UV")]);
}

void layoutMeshIfNeeded(const Mesh& mesh) {
    __layoutMeshIfNeededOccluderPass(mesh);
    __layoutMeshIfNeededShadowPass(mesh);
    __layoutMeshIfNeededMainPass(mesh);
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
    const ComPtr<ID3D12PipelineState>& occluderShader,
    const ComPtr<ID3D12PipelineState>& shadowShader,
    const ComPtr<ID3D12PipelineState>& gBufferShader,
    DescriptorPool* pDsvPool,
    const ComPtr<ID3D12CommandQueue>& cmdQ,
    const D3D12_VIEWPORT& viewport,
    const D3D12_RECT& scissorRect,
    Fence* pFence,
    Resources* pResources,
    ThreadPool* threadPool,
    CommandListPool* commandListPool,
    std::vector<DrawEvent>&& drawEvents,
    std::vector<OccluderInfo>&& occluderInfos,
    const LightData& mainDirectionalLightData,
    const CameraData& cameraData,
    const FrameData& frameData,
    std::size_t roomIdx
) : descriptorHeaps_(descriptorHeaps),
    pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
    pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
    rootSig_(rootSig), occluderShader_(occluderShader),
    shadowShader_(shadowShader), gBufferShader_(gBufferShader),
    pDsvPool_(pDsvPool),
    cmdQ_(cmdQ), viewport_(viewport), scissorRect_(scissorRect),
    pFence_(pFence), pResources_(pResources),
    threadPool_(threadPool), cmdListPool_(commandListPool),
    drawEvents_(std::move(drawEvents)), occluderInfos_(std::move(occluderInfos)),
    mainDirectionalLightData_(mainDirectionalLightData),
    cameraData_(cameraData), frameData_(frameData),
    roomIdx_(roomIdx),
    rootParamIdxPID_(rootSig->paramIdx("PerInstanceData")),
    rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
    rootParamIdxPFD_(rootSig->paramIdx("PerFrameData")),
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
    dsvGB_ = SharedResources::GBuffer::gBufferData[roomIdx].dsvHandle;
    SharedResources::ShadowMap::validateRequiredKeys({SharedResources::ShadowMap::kDefaultKey});
}

void Dispatcher::occluderPass() {
    occluderUpdate();
    occluderDraw();
}

void Dispatcher::occluderUpdate() {
     if (occluderInfos_.empty()) return;

    static auto perInstanceData = std::vector<HiZOccluderShader::PerInstanceData>();
    perInstanceData.resize(occluderInfos_.size());

    std::ranges::transform(occluderInfos_, perInstanceData.begin(),
        [viewProj = cameraData_.view * cameraData_.proj](const OccluderInfo& e) {
            return HiZOccluderShader::PerInstanceData{
                .wvp = mu::transpose(e.world * viewProj).getXmf()
            };
        }
    );

    pResources_->occluderPass.perInstanceData.stage(roomIdx_, perInstanceData);
    perInstanceData.clear();
}

void Dispatcher::occluderDraw() {
    if (occluderInfos_.empty()) {
        return;
    }

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] TerrainPipeline::occluderPass: no command context available.", false);
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

    DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(occluderShader_.Get()), false);

    auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
    std::ranges::transform(descriptorHeaps_, heapsRaw.begin(),
        [](const ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
    DISPLAY_ERROR_DX_VOID(cmdList->SetDescriptorHeaps(
        static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

    DISPLAY_ERROR_DX_VOID(cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);

    DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(
        0u, nullptr, false, &SharedResources::HiZMap::hiZMaps[roomIdx_].dsvHandle), false
    );

    DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &viewport_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &scissorRect_), false);

    pResources_->occluderPass.perInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);

    u32t idxDrawcall = 0u;

    for (const auto& ev : occluderInfos_) {
        if (!ev.terrain) continue;
        const auto& mesh = ev.terrain->mesh;
        if (mesh.subMeshes.empty()) continue;

        auto pdd = HiZOccluderShader::PerDrawcallData{
            .firstInstanceOffset = idxDrawcall
        };
        pResources_->occluderPass.perDrawcallData.cbuffers[idxDrawcall].stage(roomIdx_, &pdd, 1u);
        pResources_->occluderPass.perDrawcallData.cbuffers[idxDrawcall].bind(cmdList, rootParamIdxPDD_, roomIdx_);

        layoutMeshIfNeeded(mesh);
        const auto& allVbViews = mesh.vbViewsByPipeline.at("TerrainPipeline_Occluder");
        DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(0u, 1u, &allVbViews[0]), false);

        const auto& subMesh = mesh.subMeshes[0];
        DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&subMesh.ibView), false);

        const UINT indexCount = subMesh.ibView.SizeInBytes / sizeof(u32t);
        DISPLAY_ERROR_DX_VOID(cmdList->DrawIndexedInstanced(indexCount, 1u, 0u, 0, 0u), false);

        ++idxDrawcall;
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

// ---------------------------------------------------------------------------
// shadowPass / shadowPassMT
// ---------------------------------------------------------------------------

void Dispatcher::shadowPass() {
    shadowUpdate();
    shadowDraw();
}

// Terrain has 1-4 draw events; no MT benefit. Delegates to single-threaded path.
void Dispatcher::shadowPassMT() {
    shadowPass();
}

// ---------------------------------------------------------------------------
// shadowUpdate / shadowDraw
// ---------------------------------------------------------------------------

void Dispatcher::shadowUpdate() {
    if (drawEvents_.empty()) return;

    const auto& light = mainDirectionalLightData_;
    for (u32t ci = 0u; ci < light.cascadeCount; ++ci) {
        TerrainShadowMapCSMShader::PerFrameData pfd{};
        pfd.lightVP = mu::transpose(
            light.cascadeViews[ci] * light.cascadeProjs[ci]
        ).getXmf();
        pfd.cascadeIdx = ci;
        pfd.camPos     = light.cascadeCameraPos.getXmf();  // camera-relative caster rebase
        pResources_->shadowPass.perFrameData.cbuffers[ci].stage(roomIdx_, &pfd, 1u);
    }
}

void Dispatcher::shadowDraw() {
    if (drawEvents_.empty()) return;

    const auto& csmData = SharedResources::ShadowMap::csmShadowMapData.at(
        std::string(SharedResources::ShadowMap::kDefaultKey))[roomIdx_];
    const u32t cascadeCount = csmData.cascadeCount;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] TerrainDeferredPipeline::shadowDraw: no command context available.", false);
    if (!cmdCtx.cmdList) return;

    auto* cmdList  = cmdCtx.cmdList.Get();
    auto* cmdAlloc = cmdCtx.cmdAlloc.Get();

    auto hrReset = cmdAlloc->Reset();
    DISPLAY_ERROR_DX_HR(hrReset, false);
    if (hrReset < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    auto hrListReset = cmdList->Reset(cmdAlloc, nullptr);
    DISPLAY_ERROR_DX_HR(hrListReset, false);
    if (hrListReset < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(shadowShader_.Get()), false);

    auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
    std::ranges::transform(descriptorHeaps_, heapsRaw.begin(),
        [](const ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
    DISPLAY_ERROR_DX_VOID(cmdList->SetDescriptorHeaps(
        static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

    DISPLAY_ERROR_DX_VOID(cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);

    u32t idxDrawcall = 0u;

    for (u32t ci = 0u; ci < cascadeCount; ++ci) {
        const auto& cascadeSlice = csmData.cascades[ci];

        DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(
            0u, nullptr, false, &cascadeSlice.dsv), false);

        auto shadowVP = D3D12_VIEWPORT{
            .TopLeftX = 0.f, .TopLeftY = 0.f,
            .Width    = static_cast<float>(cascadeSlice.width),
            .Height   = static_cast<float>(cascadeSlice.height),
            .MinDepth = 0.f, .MaxDepth = 1.f
        };
        auto shadowSR = D3D12_RECT{
            .left = 0, .top = 0,
            .right  = static_cast<LONG>(cascadeSlice.width),
            .bottom = static_cast<LONG>(cascadeSlice.height)
        };
        DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &shadowVP), false);
        DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &shadowSR), false);

        pResources_->shadowPass.perFrameData.cbuffers[ci].bind(cmdList, rootParamIdxPFD_, roomIdx_);

        for (const auto& ev : drawEvents_) {
            if (!ev.terrain) continue;
            const auto& mesh = ev.terrain->mesh;
            if (mesh.subMeshes.empty()) continue;

            auto pdd = TerrainShadowMapShader::PerDrawcallData{
                .world = mu::transpose(ev.world).getXmf()
            };
            pResources_->shadowPass.perDrawcallData.cbuffers[idxDrawcall].stage(roomIdx_, &pdd, 1u);
            pResources_->shadowPass.perDrawcallData.cbuffers[idxDrawcall].bind(cmdList, rootParamIdxPDD_, roomIdx_);

            layoutMeshIfNeeded(mesh);
            const auto& allVbViews = mesh.vbViewsByPipeline.at("TerrainPipeline_Shadow");
            DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(0u, 1u, &allVbViews[0]), false);

            const auto& subMesh = mesh.subMeshes[0];
            DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&subMesh.ibView), false);

            const UINT indexCount = subMesh.ibView.SizeInBytes / sizeof(u32t);
            DISPLAY_ERROR_DX_VOID(cmdList->DrawIndexedInstanced(indexCount, 1u, 0u, 0, 0u), false);

            ++idxDrawcall;
        }
    }

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR(hrClose, false);
    if (hrClose < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    ID3D12CommandList* lists[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, lists), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

// ---------------------------------------------------------------------------
// gBufferPass / gBufferPassMT
// ---------------------------------------------------------------------------

void Dispatcher::gBufferPass() {
    gBufferUpdate();
    gBufferDraw();
}

// Terrain has 1-4 draw events; no MT benefit. Delegates to single-threaded path.
void Dispatcher::gBufferPassMT() {
    gBufferPass();
}

// ---------------------------------------------------------------------------
// gBufferUpdate / gBufferDraw
// ---------------------------------------------------------------------------

void Dispatcher::gBufferUpdate() {
    if (drawEvents_.empty()) return;

    TerrainDeferredGBufferShader::PerFrameData pfd{};
    pfd.globalAmbient = frameData_.globalAmbient.getXmf();
    pResources_->gBufferPass.perFrameData.stage(roomIdx_, &pfd, 1u);
}

void Dispatcher::gBufferDraw() {
    if (drawEvents_.empty() || !gBufferShader_) return;

    CommandContext cmdCtx{};
    DISPLAY_ERROR_STR(cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
        "[GFX Error] TerrainDeferredPipeline::gBufferDraw: no command context available.", false);
    if (!cmdCtx.cmdList) return;

    auto* cmdList  = cmdCtx.cmdList.Get();
    auto* cmdAlloc = cmdCtx.cmdAlloc.Get();

    auto hrReset = cmdAlloc->Reset();
    DISPLAY_ERROR_DX_HR(hrReset, false);
    if (hrReset < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    auto hrListReset = cmdList->Reset(cmdAlloc, nullptr);
    DISPLAY_ERROR_DX_HR(hrListReset, false);
    if (hrListReset < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(gBufferShader_.Get()), false);
    DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(5u, rtvGB_, false, &dsvGB_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &viewport_), false);
    DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &scissorRect_), false);

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

    DISPLAY_ERROR_DX_VOID(cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);

    pResources_->gBufferPass.perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);

    u32t idxDrawcall = 0u;

    for (const auto& ev : drawEvents_) {
        if (!ev.terrain) continue;
        const auto& terrain = *ev.terrain;
        const auto& mesh    = terrain.mesh;
        if (mesh.subMeshes.empty()) continue;

        auto pdd = TerrainShader::PerDrawcallData{};

        const auto& worldMat = ev.world;
        auto view = cameraData_.view;
        auto proj = cameraData_.proj;
        auto wvp  = worldMat * view * proj;
        auto wv   = worldMat * view;
        pdd.wvp   = mu::transpose(wvp).getXmf();
        pdd.world = mu::transpose(worldMat).getXmf();
        pdd.wv    = mu::transpose(wv).getXmf();

        pdd.idxSplatMap = terrain.splatMap.idxSrv;
        pdd.layerCount  = terrain.layerCount;

        const int maxLayers = std::min(
            static_cast<int>(terrain.layers.size()),
            TerrainShader::MAX_TERRAIN_LAYERS
        );
        for (int i = 0; i < maxLayers; ++i) {
            const auto& layer = terrain.layers[i];
            pdd.idxDiffuse[i]        = layer.diffuse.idxSrv;
            pdd.idxNormal[i]         = layer.normalMap.idxSrv;
            pdd.tiling[i]            = XMFLOAT4(
                terrain.sizeX / layer.tileSizeX,
                terrain.sizeZ / layer.tileSizeY,
                layer.tileOffsetX,
                layer.tileOffsetY
            );
            pdd.metallicRoughness[i] = XMFLOAT4(layer.metallic, layer.roughness, 0.f, 0.f);
        }
        pdd.hasAnyNormal = 0;
        for (int i = 0; i < maxLayers; ++i) {
            if (terrain.layers[i].normalMap.idxSrv.idxRange >= 0) {
                pdd.hasAnyNormal = 1;
                break;
            }
        }
        for (int i = maxLayers; i < TerrainShader::MAX_TERRAIN_LAYERS; ++i) {
            pdd.idxDiffuse[i].idxRange    = -1;
            pdd.idxNormal[i].idxRange     = -1;
            pdd.tiling[i]                 = XMFLOAT4(1.f, 1.f, 0.f, 0.f);
            pdd.metallicRoughness[i]      = XMFLOAT4(0.f, 0.85f, 0.f, 0.f);
        }

        pResources_->gBufferPass.perDrawcallData.cbuffers[idxDrawcall].stage(roomIdx_, &pdd, 1u);
        pResources_->gBufferPass.perDrawcallData.cbuffers[idxDrawcall].bind(cmdList, rootParamIdxPDD_, roomIdx_);

        layoutMeshIfNeeded(mesh);
        const auto& vbViews = mesh.vbViewsByPipeline.at("TerrainPipeline_Main");
        DISPLAY_ERROR_DX_VOID(cmdList->IASetVertexBuffers(
            0u, static_cast<UINT>(vbViews.size()), vbViews.data()), false);

        const auto& subMesh = mesh.subMeshes[0];
        DISPLAY_ERROR_DX_VOID(cmdList->IASetIndexBuffer(&subMesh.ibView), false);

        const UINT indexCount = subMesh.ibView.SizeInBytes / sizeof(u32t);
        DISPLAY_ERROR_DX_VOID(cmdList->DrawIndexedInstanced(indexCount, 1u, 0u, 0, 0u), false);

        ++idxDrawcall;
    }

    auto hrClose = cmdList->Close();
    DISPLAY_ERROR_DX_HR(hrClose, false);
    if (hrClose < 0) { cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx)); return; }

    ID3D12CommandList* lists[] = { cmdList };
    DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, lists), false);
    pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

}   // namespace TerrainDeferredPipeline
