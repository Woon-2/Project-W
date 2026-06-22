#ifndef __iblPrecomputePipeline_HPP
#define __iblPrecomputePipeline_HPP

#include "gfxUtil.hpp"
#include "shader.hpp"

// IBL runtime precompute (load-time pass).
//
// Records and executes, on a single ResourceLoading command context, the three
// compute dispatches that fill SharedResources::IBL::iblData:
//   1. diffuse irradiance convolution  -> iblData.irradiance      (32x32 cube)
//   2. specular GGX prefilter (5 mips) -> iblData.prefiltered     (128x128 cube)
//   3. environment BRDF integration    -> iblData.brdfLUT         (256x256 2D)
//
// Preconditions:
//   - SharedResources::IBL::addIBL() has already created the destination textures
//     (all in UNORDERED_ACCESS state) and their bindless SRVs + per-mip UAVs.
//   - The compute PSOs were created via createIBL*Shader().
//   - The source environment cube (envCubeRes / idxEnv) is currently in envCurState.
//     For a texture loaded with loadTexture(), envCurState should be
//     D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE.
//
// Postconditions (when this call returns, after the load fence has been waited on):
//   - iblData.irradiance / prefiltered / brdfLUT are in PIXEL_SHADER_RESOURCE.
//   - The environment cube is restored to PIXEL_SHADER_RESOURCE.
//
// iblParamsCBs must have at least 7 cbuffers (1 irradiance + 5 prefilter mips + 1 BRDF),
// room count 1. Create it with:
//   createConstantBufferArray(device, sizeof(IBLShader::IBLParams), 7, 1, "IBL_Params");
//
// descriptorHeaps must contain the GPU-visible CBV/SRV/UAV heap and the sampler heap
// (the same heaps bound during normal rendering), so the bindless pools resolve.
void precomputeIBL(
	ID3D12Device* device,
	const BindlessIndex& idxEnv,
	ID3D12Resource* envCubeRes,
	D3D12_RESOURCE_STATES envCurState,
	RootSig* rootSig,
	ID3D12PipelineState* irradiancePSO,
	ID3D12PipelineState* prefilterPSO,
	ID3D12PipelineState* brdfPSO,
	const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
	DescriptorPool& texPool,
	DescriptorPool& texArrayPool,
	DescriptorPool& texCubePool,
	DescriptorPool& samPool,
	DescriptorPool& cmpSamPool,
	ConstantBufferArray& iblParamsCBs,
	RenderSubmitter* submitter,
	CommandListPool& cmdListPool,
	Fence& loadFence,
	bool envIsLDR
);

#endif	// __iblPrecomputePipeline_HPP
