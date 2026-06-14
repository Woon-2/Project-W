#ifndef __BloomPipeline_HPP
#define __BloomPipeline_HPP

#include "gfxUtil.hpp"

class RootSig;

namespace BloomPipeline {

// Upper bound on bloom passes: prefilter + (N-1) downsample + (N-1) upsample = 2N-1,
// with N capped at 6 mips => 11. The per-drawcall constant buffer array is sized to this.
constexpr u32t kMaxBloomPasses = 11u;

// GPU-side resources for the bloom pass. One constant buffer element per pass (b0).
struct Resources {
	ConstantBufferArray perDrawcallData;   // kMaxBloomPasses elements (× roomCnt)
};

// Dispatcher for the pixel-based HDR bloom pass. Records the whole chain
// (prefilter -> downsample -> additive upsample) on a single command list:
// reads SceneColorHDR (PIXEL_SHADER_RESOURCE) and leaves bloom mip 0 in
// PIXEL_SHADER_RESOURCE for the tonemap-resolve composite. Mirrors the
// SkyboxPipeline/TonemapPipeline dispatcher idiom.
class Dispatcher {
public:
	Dispatcher() = default;
	Dispatcher(
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
	);

	// Stages all per-pass constant buffers, records every bloom pass, and submits.
	void render();

private:
	std::vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps_{};
	DescriptorPool* pTexPool_      = nullptr;
	DescriptorPool* pTexArrayPool_ = nullptr;
	DescriptorPool* pTexCubePool_  = nullptr;
	DescriptorPool* pSamPool_      = nullptr;
	DescriptorPool* pCmpSamPool_   = nullptr;
	std::shared_ptr<RootSig> rootSig_ = nullptr;
	ComPtr<ID3D12PipelineState> prefilterShader_  = nullptr;
	ComPtr<ID3D12PipelineState> downsampleShader_ = nullptr;
	ComPtr<ID3D12PipelineState> upsampleShader_   = nullptr;
	ComPtr<ID3D12CommandQueue>  cmdQ_ = nullptr;
	Fence* pFence_ = nullptr;
	Resources* pResources_ = nullptr;
	CommandListPool* cmdListPool_ = nullptr;
	BindlessIndex sceneColorSrv_{};
	std::size_t roomIdx_{};
	float threshold_ = 1.0f;

	UINT rootParamIdxPDD_{};
	UINT rootParamIdxTexPool_{};
	UINT rootParamIdxTexArrayPool_{};
	UINT rootParamIdxTexCubePool_{};
	UINT rootParamIdxSamPool_{};
	UINT rootParamIdxCmpSamPool_{};
};

}	// namespace BloomPipeline

#endif	// __BloomPipeline_HPP
