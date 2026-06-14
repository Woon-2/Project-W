#include "pch.hpp"
#include "iblPrecomputePipeline.hpp"
#include "sharedResources.hpp"
#include "errorHandling.hpp"

namespace {

// numthreads(8,8,1) in every IBL compute shader.
constexpr u32t kThreadGroupSize = 8u;

inline u32t dispatchDim(u32t extent) {
	return (extent + kThreadGroupSize - 1u) / kThreadGroupSize;
}

}	// anonymous namespace

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
	ID3D12CommandQueue* cmdQ,
	CommandListPool& cmdListPool,
	Fence& loadFence,
	bool envIsLDR
) {
	using namespace SharedResources::IBL;

	if (!iblData.created) {
		DISPLAY_ERROR_STR(false,
			"[GFX Error] precomputeIBL: SharedResources::IBL::addIBL must be called first.", false);
		return;
	}

	// IBL texture parameters (must match addIBL()).
	constexpr u32t irradianceRes  = 32u;
	constexpr u32t prefilteredRes = 128u;
	constexpr u32t brdfRes        = 256u;

	const u32t prefilteredMipCount = iblData.prefilteredMipCount;

	// --- Acquire and reset a load-time command context ---
	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR(cmdListPool.allocOne(CommandListUsage::ResourceLoading, cmdCtx),
		"[GFX Error] precomputeIBL: no available ResourceLoading command list.", false);
	if (!cmdCtx.cmdList) return;

	auto cmdList  = cmdCtx.cmdList.Get();
	auto cmdAlloc = cmdCtx.cmdAlloc.Get();
	DISPLAY_ERROR_DX_VOID(cmdAlloc->Reset(), false);
	DISPLAY_ERROR_DX_VOID(cmdList->Reset(cmdAlloc, nullptr), false);

	// --- Bind compute root signature, heaps, and bindless pools ---
	DISPLAY_ERROR_DX_VOID(cmdList->SetComputeRootSignature(rootSig->get()), false);

	auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps.size());
	std::ranges::transform(descriptorHeaps, heapsRaw.begin(),
		[](const ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
	DISPLAY_ERROR_DX_VOID(cmdList->SetDescriptorHeaps(
		static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

	const UINT paramTexPool      = rootSig->paramIdx("TexturePool");
	const UINT paramTexArrayPool = rootSig->paramIdx("TextureArrayPool");
	const UINT paramTexCubePool  = rootSig->paramIdx("TextureCubePool");
	const UINT paramSamPool      = rootSig->paramIdx("SamplerPool");
	const UINT paramCmpSamPool   = rootSig->paramIdx("ComparisonSamplerPool");
	const UINT paramDestTex      = rootSig->paramIdx("DestTex");
	const UINT paramPDD          = rootSig->paramIdx("PerDrawcallData");

	// A descriptor pool's GPU table start is gpuHandle(0).
	DISPLAY_ERROR_DX_VOID(cmdList->SetComputeRootDescriptorTable(paramTexPool,      texPool.gpuHandle(0)),      false);
	DISPLAY_ERROR_DX_VOID(cmdList->SetComputeRootDescriptorTable(paramTexArrayPool, texArrayPool.gpuHandle(0)), false);
	DISPLAY_ERROR_DX_VOID(cmdList->SetComputeRootDescriptorTable(paramTexCubePool,  texCubePool.gpuHandle(0)),  false);
	DISPLAY_ERROR_DX_VOID(cmdList->SetComputeRootDescriptorTable(paramSamPool,      samPool.gpuHandle(0)),      false);
	DISPLAY_ERROR_DX_VOID(cmdList->SetComputeRootDescriptorTable(paramCmpSamPool,   cmpSamPool.gpuHandle(0)),   false);

	// --- Transition the source environment cube to a compute-readable SRV state ---
	if (envCurState != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) {
		transitionResourceState(cmdList, envCubeRes,
			envCurState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	const u32t envIsLDRu = envIsLDR ? 1u : 0u;

	// =========================================================================
	// 1. Diffuse irradiance convolution -> iblData.irradiance (mip 0).
	// =========================================================================
	{
		const auto params = IBLShader::IBLParams{
			.idxEnv    = idxEnv,
			.faceRes   = irradianceRes,
			.mipLevel  = 0u,
			.mipCount  = 1u,
			.envIsLDR  = envIsLDRu,
			.roughness = 0.0f,
			._pad      = XMFLOAT3{ 0.0f, 0.0f, 0.0f }
		};
		iblParamsCBs.cbuffers[0].stage(0u, &params, 1u);

		DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(irradiancePSO), false);
		iblParamsCBs.cbuffers[0].bindCompute(cmdList, paramPDD, 0u);
		DISPLAY_ERROR_DX_VOID(cmdList->SetComputeRootDescriptorTable(
			paramDestTex, iblData.irradianceUavHandle), false);

		DISPLAY_ERROR_DX_VOID(cmdList->Dispatch(
			dispatchDim(irradianceRes), dispatchDim(irradianceRes), 6u), false);

		uavBarrier(cmdList, iblData.irradiance.res.Get());
	}

	// =========================================================================
	// 2. Specular GGX prefilter -> iblData.prefiltered, one dispatch per mip.
	// =========================================================================
	for (u32t m = 0u; m < prefilteredMipCount; ++m) {
		const u32t mipRes   = prefilteredRes >> m;
		const float rough   = (prefilteredMipCount > 1u)
			? static_cast<float>(m) / static_cast<float>(prefilteredMipCount - 1u)
			: 0.0f;

		const auto params = IBLShader::IBLParams{
			.idxEnv    = idxEnv,
			.faceRes   = mipRes,
			.mipLevel  = m,
			.mipCount  = prefilteredMipCount,
			.envIsLDR  = envIsLDRu,
			.roughness = rough,
			._pad      = XMFLOAT3{ 0.0f, 0.0f, 0.0f }
		};
		const std::size_t cbIdx = 1u + m;
		iblParamsCBs.cbuffers[cbIdx].stage(0u, &params, 1u);

		DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(prefilterPSO), false);
		iblParamsCBs.cbuffers[cbIdx].bindCompute(cmdList, paramPDD, 0u);
		DISPLAY_ERROR_DX_VOID(cmdList->SetComputeRootDescriptorTable(
			paramDestTex, iblData.prefilteredMipUavHandles[m]), false);

		DISPLAY_ERROR_DX_VOID(cmdList->Dispatch(
			dispatchDim(mipRes), dispatchDim(mipRes), 6u), false);

		uavBarrier(cmdList, iblData.prefiltered.res.Get());
	}

	// =========================================================================
	// 3. Environment BRDF integration LUT -> iblData.brdfLUT.
	// =========================================================================
	{
		const std::size_t cbIdx = 1u + static_cast<std::size_t>(prefilteredMipCount);
		const auto params = IBLShader::IBLParams{
			.idxEnv    = idxEnv,
			.faceRes   = brdfRes,
			.mipLevel  = 0u,
			.mipCount  = 1u,
			.envIsLDR  = envIsLDRu,
			.roughness = 0.0f,
			._pad      = XMFLOAT3{ 0.0f, 0.0f, 0.0f }
		};
		iblParamsCBs.cbuffers[cbIdx].stage(0u, &params, 1u);

		DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(brdfPSO), false);
		iblParamsCBs.cbuffers[cbIdx].bindCompute(cmdList, paramPDD, 0u);
		DISPLAY_ERROR_DX_VOID(cmdList->SetComputeRootDescriptorTable(
			paramDestTex, iblData.brdfUavHandle), false);

		DISPLAY_ERROR_DX_VOID(cmdList->Dispatch(
			dispatchDim(brdfRes), dispatchDim(brdfRes), 1u), false);

		uavBarrier(cmdList, iblData.brdfLUT.res.Get());
	}

	// --- Transition outputs UNORDERED_ACCESS -> PIXEL_SHADER_RESOURCE ---
	transitionResourceState(cmdList, iblData.irradiance.res.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	transitionResourceState(cmdList, iblData.prefiltered.res.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	transitionResourceState(cmdList, iblData.brdfLUT.res.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// --- Restore the environment cube to PIXEL_SHADER_RESOURCE ---
	transitionResourceState(cmdList, envCubeRes,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// --- Submit and synchronize via the load fence (one-shot, blocking) ---
	auto hrClose = cmdList->Close();
	DISPLAY_ERROR_DX_HR(hrClose, false);
	if (hrClose < 0) {
		cmdListPool.freeOne(CommandListUsage::ResourceLoading, std::move(cmdCtx));
		return;
	}

	ID3D12CommandList* staged[] = { cmdList };
	DISPLAY_ERROR_DX_VOID(cmdQ->ExecuteCommandLists(1u, staged), false);

	++loadFence.desiredValue;
	DISPLAY_ERROR_DX_HR(cmdQ->Signal(loadFence.fence.Get(), loadFence.desiredValue), false);
	if (loadFence.fence->GetCompletedValue() != loadFence.desiredValue) {
		loadFence.fence->SetEventOnCompletion(loadFence.desiredValue, nullptr);
	}

	// GPU finished with the command context; return it to the pool.
	cmdListPool.freeOne(CommandListUsage::ResourceLoading, std::move(cmdCtx));
}
