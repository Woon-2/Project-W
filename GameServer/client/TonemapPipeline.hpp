#ifndef __TonemapPipeline_HPP
#define __TonemapPipeline_HPP

#include "gfxUtil.hpp"

class RootSig;

namespace TonemapPipeline {

// GPU-side resources for the tonemap resolve pass.
// Only a single per-drawcall constant buffer (b0) is needed: it carries the
// bindless index of the HDR scene-color SRV to sample. Managed per-room via a
// ConstantBufferArray with a single element so the Resources idiom matches the
// other fullscreen pipelines.
struct Resources {
	ConstantBufferArray perDrawcallData;   // b0
};

// Dispatcher for the tonemap resolve pass.
// Draws a single fullscreen triangle that reads the HDR scene-color RT (bindless)
// and writes the LDR backbuffer. There is no DrawEvent vector and no mesh — the
// vertex shader synthesizes the triangle from SV_VertexID.
//
// Mirrors the SkyboxPipeline::Dispatcher idiom (updateGPUDataSingleThreaded +
// drawSingleThreaded) so GFX can instantiate and drive it the same way.
class Dispatcher {
public:
	Dispatcher() = default;
	Dispatcher(
		const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
		DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
		DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
		DescriptorPool* pCmpSamPool,
		const std::shared_ptr<RootSig>& rootSig,
		const ComPtr<ID3D12PipelineState>& shader,
		const ComPtr<ID3D12CommandQueue>& cmdQ,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv,
		Fence* pFence,
		Resources* pResources,
		CommandListPool* commandListPool,
		const BindlessIndex& idxSceneColor,
		std::size_t roomIdx
	);

	// Stages the b0 constant buffer with the scene-color bindless index.
	void updateGPUDataSingleThreaded();
	// Records and submits the fullscreen-triangle draw to the backbuffer.
	void drawSingleThreaded();

private:
	std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
	DescriptorPool* pTexPool_      = nullptr;
	DescriptorPool* pTexArrayPool_ = nullptr;
	DescriptorPool* pTexCubePool_  = nullptr;
	DescriptorPool* pSamPool_      = nullptr;
	DescriptorPool* pCmpSamPool_   = nullptr;
	std::shared_ptr<RootSig> rootSig_ = nullptr;
	ComPtr<ID3D12PipelineState> shader_ = nullptr;
	ComPtr<ID3D12CommandQueue>  cmdQ_   = nullptr;
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT     scissorRect_{};
	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv_{};
	Fence* pFence_ = nullptr;
	Resources* pResources_ = nullptr;
	CommandListPool* cmdListPool_ = nullptr;
	BindlessIndex idxSceneColor_{};
	std::size_t roomIdx_{};

	UINT rootParamIdxPDD_{};
	UINT rootParamIdxTexPool_{};
	UINT rootParamIdxTexArrayPool_{};
	UINT rootParamIdxTexCubePool_{};
	UINT rootParamIdxSamPool_{};
	UINT rootParamIdxCmpSamPool_{};
};

}	// namespace TonemapPipeline

#endif	// __TonemapPipeline_HPP
