#include "pch.hpp"
#include "TonemapPipeline.hpp"
#include "shader.hpp"
#include "errorHandling.hpp"

namespace TonemapPipeline {

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
	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv,
	Fence* pFence,
	Resources* pResources,
	CommandListPool* commandListPool,
	const BindlessIndex& idxSceneColor,
	std::size_t roomIdx,
	float exposure,
	float bloomIntensity,
	u32t debugMode,
	const BindlessIndex& idxBloom,
	DescriptorPool* pTexPool3D,
	const BindlessIndex& idxColorGradingLUT,
	const HeatDistortionShader::HeatParams& heat,
	const BindlessIndex& idxGB4
) : descriptorHeaps_(descriptorHeaps),
	pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
	pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
	pTexPool3D_(pTexPool3D),
	rootSig_(rootSig), shader_(shader), submitter_(submitter),
	viewport_(viewport), scissorRect_(scissorRect), backBufferRtv_(backBufferRtv),
	pFence_(pFence), pResources_(pResources), cmdListPool_(commandListPool),
	idxSceneColor_(idxSceneColor), roomIdx_(roomIdx),
	exposure_(exposure), bloomIntensity_(bloomIntensity), debugMode_(debugMode), idxBloom_(idxBloom),
	idxColorGradingLUT_(idxColorGradingLUT), heat_(heat), idxGB4_(idxGB4),
	rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
	rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
	rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
	rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
	rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
	rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool")),
	rootParamIdxTexPool3D_(rootSig->paramIdx("Texture3DPool")) {}

void Dispatcher::updateGPUDataSingleThreaded() {
	auto pdd = TonemapResolveShader::PerDrawcallData{
		.idxSceneColor       = idxSceneColor_,
		.idxBloom            = idxBloom_,
		.exposure            = exposure_,
		.bloomIntensity      = bloomIntensity_,
		.debugMode           = debugMode_,
		._pad                = 0.0f,
		.idxColorGradingLUT  = idxColorGradingLUT_,
		.heat                = heat_,
		.idxGB4              = idxGB4_
	};
	pResources_->perDrawcallData.cbuffers[0].stage(roomIdx_, &pdd, 1u);
}

void Dispatcher::drawSingleThreaded() {
	// Allocate a command context.
	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR( cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
		"[GFX Error] TonemapPipeline::drawSingleThreaded: no command list available.", false
	);
	if (!cmdCtx.cmdList) {
		return;
	}

	auto cmdList  = cmdCtx.cmdList.Get();
	auto cmdAlloc = cmdCtx.cmdAlloc.Get();
	auto hrCmdAllocReset = cmdAlloc->Reset();
	DISPLAY_ERROR_DX_HR( hrCmdAllocReset, false );
	if (hrCmdAllocReset < 0) {
		cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
		return;
	}
	auto hrCmdListReset = cmdList->Reset(cmdAlloc, nullptr);
	DISPLAY_ERROR_DX_HR( hrCmdListReset, false );
	if (hrCmdListReset < 0) {
		cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
		return;
	}

	DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
	DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(shader_.Get()), false);
	DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(1u, &backBufferRtv_, false, nullptr), false);
	DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &viewport_), false);
	DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &scissorRect_), false);

	// Bindless setup: bind descriptor heaps and pool descriptor tables.
	auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
	std::ranges::transform(descriptorHeaps_, descriptorHeapsRaw.begin(),
		[](ComPtr<ID3D12DescriptorHeap>& comPtrHeap) { return comPtrHeap.Get(); }
	);
	DISPLAY_ERROR_DX_VOID( cmdList->SetDescriptorHeaps(
		static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()
	), false );

	pTexPool_->bind(cmdList, rootParamIdxTexPool_);
	pTexArrayPool_->bind(cmdList, rootParamIdxTexArrayPool_);
	pTexCubePool_->bind(cmdList, rootParamIdxTexCubePool_);
	pSamPool_->bind(cmdList, rootParamIdxSamPool_);
	pCmpSamPool_->bind(cmdList, rootParamIdxCmpSamPool_);
	pTexPool3D_->bind(cmdList, rootParamIdxTexPool3D_);

	// PerDrawcallData (b0): scene-color bindless index.
	pResources_->perDrawcallData.cbuffers[0].bind(cmdList, rootParamIdxPDD_, roomIdx_);

	DISPLAY_ERROR_DX_VOID(cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);
	DISPLAY_ERROR_DX_VOID(cmdList->DrawInstanced(3u, 1u, 0u, 0u), false);   // fullscreen triangle

	auto hrClose = cmdList->Close();
	DISPLAY_ERROR_DX_HR( hrClose, false );
	if (hrClose < 0) {
		cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
		return;
	}

	ID3D12CommandList* stagedCmdLists[] = { cmdList };
	DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, stagedCmdLists), false);

	pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
		.push_back(std::move(cmdCtx));
}

}	// namespace TonemapPipeline
