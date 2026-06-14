#include "pch.hpp"
#include "BloomPipeline.hpp"
#include "shader.hpp"
#include "sharedResources.hpp"
#include "errorHandling.hpp"

namespace BloomPipeline {

Dispatcher::Dispatcher(
	const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
	DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
	DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
	DescriptorPool* pCmpSamPool,
	const std::shared_ptr<RootSig>& rootSig,
	const ComPtr<ID3D12PipelineState>& prefilterShader,
	const ComPtr<ID3D12PipelineState>& downsampleShader,
	const ComPtr<ID3D12PipelineState>& upsampleShader,
	const ComPtr<ID3D12CommandQueue>& cmdQ,
	Fence* pFence,
	Resources* pResources,
	CommandListPool* commandListPool,
	const BindlessIndex& sceneColorSrv,
	std::size_t roomIdx,
	float threshold
) : descriptorHeaps_(descriptorHeaps),
	pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
	pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
	rootSig_(rootSig),
	prefilterShader_(prefilterShader), downsampleShader_(downsampleShader), upsampleShader_(upsampleShader),
	cmdQ_(cmdQ), pFence_(pFence), pResources_(pResources), cmdListPool_(commandListPool),
	sceneColorSrv_(sceneColorSrv), roomIdx_(roomIdx), threshold_(threshold),
	rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
	rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
	rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
	rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
	rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
	rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool")) {}

void Dispatcher::render() {
	if (SharedResources::Bloom::bloomData.empty()) {
		return;
	}
	auto& bd = SharedResources::Bloom::bloomData[roomIdx_];
	const u32t N = bd.mipCount;
	if (N == 0u) {
		return;
	}

	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR( cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
		"[GFX Error] BloomPipeline::render: no command list available.", false
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

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	u32t pass = 0u;

	// Stages the cbuffer for this pass, sets the destination mip RTV/viewport and draws.
	auto doPass = [&](ID3D12PipelineState* pso, u32t destMip,
		const BindlessIndex& src, float srcW, float srcH, float threshold
	) {
		auto pdd = BloomShader::PerDrawcallData{
			.idxSrc       = src,
			.srcTexelSize = XMFLOAT2(1.0f / srcW, 1.0f / srcH),
			.threshold    = threshold,
			._pad         = 0.0f
		};
		pResources_->perDrawcallData.cbuffers[pass].stage(roomIdx_, &pdd, 1u);

		cmdList->SetPipelineState(pso);
		cmdList->OMSetRenderTargets(1u, &bd.rtv[destMip], false, nullptr);
		auto vp = D3D12_VIEWPORT{ 0.0f, 0.0f,
			static_cast<float>(bd.width[destMip]), static_cast<float>(bd.height[destMip]), 0.0f, 1.0f };
		auto sc = D3D12_RECT{ 0, 0,
			static_cast<LONG>(bd.width[destMip]), static_cast<LONG>(bd.height[destMip]) };
		cmdList->RSSetViewports(1u, &vp);
		cmdList->RSSetScissorRects(1u, &sc);
		pResources_->perDrawcallData.cbuffers[pass].bind(cmdList, rootParamIdxPDD_, roomIdx_);
		cmdList->DrawInstanced(3u, 1u, 0u, 0u);
		++pass;
	};

	// Prefilter + downsample: full-res scene color -> bloom mip 0 (half res).
	SharedResources::Bloom::transitionMip(roomIdx_, 0u, D3D12_RESOURCE_STATE_RENDER_TARGET, cmdList);
	doPass(prefilterShader_.Get(), 0u, sceneColorSrv_,
		static_cast<float>(bd.fullWidth), static_cast<float>(bd.fullHeight), threshold_);

	// Downsample chain: mip (i-1) -> mip i.
	for (u32t i = 1u; i < N; ++i) {
		SharedResources::Bloom::transitionMip(roomIdx_, i - 1u, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, cmdList);
		SharedResources::Bloom::transitionMip(roomIdx_, i,      D3D12_RESOURCE_STATE_RENDER_TARGET,         cmdList);
		doPass(downsampleShader_.Get(), i, bd.srv[i - 1u],
			static_cast<float>(bd.width[i - 1u]), static_cast<float>(bd.height[i - 1u]), 0.0f);
	}

	// Upsample chain (additive): mip (i+1) -> mip i, from the smallest used mip up to 0.
	for (int i = static_cast<int>(N) - 2; i >= 0; --i) {
		const u32t dst = static_cast<u32t>(i);
		const u32t src = static_cast<u32t>(i + 1);
		SharedResources::Bloom::transitionMip(roomIdx_, src, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, cmdList);
		SharedResources::Bloom::transitionMip(roomIdx_, dst, D3D12_RESOURCE_STATE_RENDER_TARGET,         cmdList);
		doPass(upsampleShader_.Get(), dst, bd.srv[src],
			static_cast<float>(bd.width[src]), static_cast<float>(bd.height[src]), 0.0f);
	}

	// Leave mip 0 readable for the tonemap-resolve composite.
	SharedResources::Bloom::transitionMip(roomIdx_, 0u, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, cmdList);

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

}	// namespace BloomPipeline
