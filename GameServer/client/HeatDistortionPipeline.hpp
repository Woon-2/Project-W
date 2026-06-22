#ifndef __HeatDistortionPipeline_HPP
#define __HeatDistortionPipeline_HPP

#include "gfxUtil.hpp"
#include "shader.hpp"

class RootSig;

namespace HeatDistortionPipeline {

// GPU-side resources for the heat-haze glow pass. A single per-drawcall constant
// buffer (b0) carries the HeatSource array + the GB4 (linear view-Z) bindless
// index. Managed per-room via a single-element ConstantBufferArray, matching the
// other fullscreen pipelines (TonemapPipeline idiom).
struct Resources {
	ConstantBufferArray perDrawcallData;   // b0
};

// Dispatcher for the heat-haze glow pass.
// Draws a single fullscreen triangle that additively writes a tinted, depth-gated
// glow into the HDR scene-color RT (before bloom, so the tint glows). There is no
// DrawEvent vector and no mesh — the VS synthesizes the triangle from SV_VertexID,
// and the field is driven entirely by the b0 constant buffer.
//
// Mirrors the TonemapPipeline::Dispatcher idiom (updateGPUDataSingleThreaded +
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
		RenderSubmitter* submitter,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv,
		Fence* pFence,
		Resources* pResources,
		CommandListPool* commandListPool,
		DescriptorPool* pTexPool3D,
		const HeatDistortionShader::HeatParams& heat,
		const BindlessIndex& idxGB4,
		std::size_t roomIdx
	);

	// Stages the b0 constant buffer with the heat parameters + GB4 index.
	void updateGPUDataSingleThreaded();
	// Records and submits the fullscreen-triangle draw into the scene-color RT.
	void drawSingleThreaded();

private:
	std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
	DescriptorPool* pTexPool_      = nullptr;
	DescriptorPool* pTexArrayPool_ = nullptr;
	DescriptorPool* pTexCubePool_  = nullptr;
	DescriptorPool* pSamPool_      = nullptr;
	DescriptorPool* pCmpSamPool_   = nullptr;
	DescriptorPool* pTexPool3D_    = nullptr;
	std::shared_ptr<RootSig> rootSig_ = nullptr;
	ComPtr<ID3D12PipelineState> shader_ = nullptr;
	RenderSubmitter* submitter_   = nullptr;
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT     scissorRect_{};
	D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv_{};
	Fence* pFence_ = nullptr;
	Resources* pResources_ = nullptr;
	CommandListPool* cmdListPool_ = nullptr;
	HeatDistortionShader::HeatParams heat_{};
	BindlessIndex idxGB4_{ -1, -1, -1, -1 };
	std::size_t roomIdx_{};

	UINT rootParamIdxPDD_{};
	UINT rootParamIdxTexPool_{};
	UINT rootParamIdxTexArrayPool_{};
	UINT rootParamIdxTexCubePool_{};
	UINT rootParamIdxSamPool_{};
	UINT rootParamIdxCmpSamPool_{};
	UINT rootParamIdxTexPool3D_{};
};

}	// namespace HeatDistortionPipeline

#endif	// __HeatDistortionPipeline_HPP
