#include "pch.hpp"
#include "sharedResources.hpp"
#include "errorHandling.hpp"

namespace SharedResources {

namespace ShadowMap {

DXGI_FORMAT convertDepthToColorFormat(DXGI_FORMAT depthFormat) {
	switch (depthFormat) {
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;

	default:
		DISPLAY_ERROR_STR(false, "[GFX Error] SharedResources::ShadowMap::convertDepthToColorFormat: "s
			+ "알 수 없는 depth format "s + std::to_string(depthFormat) + "을 받았습니다."s, false
		);
		return DXGI_FORMAT_UNKNOWN;
	}
}

std::unordered_map<std::string, std::vector<ShadowMapData>>    shadowMapData;
std::unordered_map<std::string, std::vector<CSMShadowMapData>> csmShadowMapData;

void addShadowMap( const std::string& key, ID3D12Device* device,
	DXGI_FORMAT format, u32t width, u32t height,
	std::size_t roomCnt, DescriptorPool& srvTexPool, DescriptorPool& dsvPool
) {
	DISPLAY_ERROR_STR(!shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::addShadowMap: \""s
		+ key + "\" 키로 이미 그림자맵이 등록되어 있습니다.", false
	);

	if (shadowMapData.contains(key)) {
		return;
	}

	auto& vec = shadowMapData[key];
	vec.reserve(roomCnt);

	for (std::size_t r = 0u; r < roomCnt; ++r) {
		Texture tex{};

		// Texture2D 리소스 생성
		{
			auto heapProperties = D3D12_HEAP_PROPERTIES{
				.Type = D3D12_HEAP_TYPE_DEFAULT,
				.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
				.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
				.CreationNodeMask = 0u,
				.VisibleNodeMask = 0u
			};
			auto clearVal = D3D12_CLEAR_VALUE{
				.Format = format,
				.DepthStencil = D3D12_DEPTH_STENCIL_VALUE{ .Depth = 1.f, .Stencil = 0u }
			};
			auto desc = D3D12_RESOURCE_DESC{
				.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
				.Alignment = 0u,
				.Width = width,
				.Height = height,
				.DepthOrArraySize = 1u,
				.MipLevels = 1u,
				.Format = format,
				.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
				.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
				.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
			};
			DISPLAY_ERROR_DX_VOID( device->CreateCommittedResource(
				&heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal,
				__uuidof(ID3D12Resource), &tex.res
			), false );
			auto resName = key + "[" + std::to_string(r) + "]";
			setD3DName(tex.res.Get(), resName.c_str());
		}

		// Texture2D DSV
		{
			auto dsvDesc = D3D12_DEPTH_STENCIL_VIEW_DESC{
				.Format = format,
				.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
				.Flags = D3D12_DSV_FLAG_NONE,
				.Texture2D = D3D12_TEX2D_DSV{ .MipSlice = 0u }
			};
			createDSV(device, tex, dsvDesc, dsvPool);
		}

		// Texture2D SRV (bindless IDX_RANGE_TEXTURE)
		{
			tex.idxSrv.idxRange = etoi(Texture::Type::Tex2D);
			createSRV( device, tex, D3D12_SHADER_RESOURCE_VIEW_DESC{
				.Format = convertDepthToColorFormat(format),
				.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Texture2D = D3D12_TEX2D_SRV{
					.MostDetailedMip = 0u,
					.MipLevels = 1u
				}
			}, srvTexPool );

			tex.idxSrv.idxInArray = 0;
			tex.idxSrv.idxSampler = calcIdxBindlessSampler(D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
				D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
				D3D12_TEXTURE_ADDRESS_MODE_BORDER, 1u
			);
		}

		ShadowMapData data{};
		data.dsv      = dsvPool.cpuHandle(tex.idxDsv);
		data.format   = format;
		data.width    = width;
		data.height   = height;
		data.curState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		data.tex      = std::move(tex);
		vec.push_back(std::move(data));
	}
}

void addCSMShadowMap( const std::string& key, ID3D12Device* device,
	DXGI_FORMAT format,
	const std::array<u32t, MAX_CSM_CASCADES>& cascadeResolutions,
	u32t cascadeCount,
	std::size_t roomCnt, DescriptorPool& srvTexPool, DescriptorPool& dsvPool
) {
	DISPLAY_ERROR_STR(!csmShadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::addCSMShadowMap: \""s
		+ key + "\" 키로 이미 CSM 그림자맵이 등록되어 있습니다.", false
	);

	if (csmShadowMapData.contains(key)) {
		return;
	}

	auto& vec = csmShadowMapData[key];
	vec.resize(roomCnt);

	for (std::size_t r = 0u; r < roomCnt; ++r) {
		vec[r].cascadeCount = cascadeCount;

		for (u32t ci = 0u; ci < cascadeCount; ++ci) {
			const u32t res = cascadeResolutions[ci];
			auto& slice = vec[r].cascades[ci];

			// Texture2D 리소스 생성 (cascade별 독립 텍스처)
			{
				auto heapProperties = D3D12_HEAP_PROPERTIES{
					.Type = D3D12_HEAP_TYPE_DEFAULT,
					.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
					.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
					.CreationNodeMask = 0u,
					.VisibleNodeMask = 0u
				};
				auto clearVal = D3D12_CLEAR_VALUE{
					.Format = format,
					.DepthStencil = D3D12_DEPTH_STENCIL_VALUE{ .Depth = 1.f, .Stencil = 0u }
				};
				auto desc = D3D12_RESOURCE_DESC{
					.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
					.Alignment = 0u,
					.Width = res,
					.Height = res,
					.DepthOrArraySize = 1u,
					.MipLevels = 1u,
					.Format = format,
					.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
					.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
					.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
				};
				DISPLAY_ERROR_DX_VOID( device->CreateCommittedResource(
					&heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
					D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal,
					__uuidof(ID3D12Resource), &slice.tex.res
				), false );
				auto resName = key + "_csm[" + std::to_string(r) + "][" + std::to_string(ci) + "]";
				setD3DName(slice.tex.res.Get(), resName.c_str());
			}

			// Texture2D DSV
			{
				auto dsvDesc = D3D12_DEPTH_STENCIL_VIEW_DESC{
					.Format = format,
					.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
					.Flags = D3D12_DSV_FLAG_NONE,
					.Texture2D = D3D12_TEX2D_DSV{ .MipSlice = 0u }
				};
				createDSV(device, slice.tex, dsvDesc, dsvPool);
			}

			// Texture2D SRV (bindless IDX_RANGE_TEXTURE)
			{
				slice.tex.idxSrv.idxRange = etoi(Texture::Type::Tex2D);
				createSRV( device, slice.tex, D3D12_SHADER_RESOURCE_VIEW_DESC{
					.Format = convertDepthToColorFormat(format),
					.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
					.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
					.Texture2D = D3D12_TEX2D_SRV{
						.MostDetailedMip = 0u,
						.MipLevels = 1u
					}
				}, srvTexPool );

				slice.tex.idxSrv.idxInArray = 0;
				slice.tex.idxSrv.idxSampler = calcIdxBindlessSampler(
					D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
					D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
					D3D12_TEXTURE_ADDRESS_MODE_BORDER, 1u
				);
			}

			slice.dsv      = dsvPool.cpuHandle(slice.tex.idxDsv);
			slice.format   = format;
			slice.width    = res;
			slice.height   = res;
			slice.curState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}
	}
}

void clearShadowMap(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	DISPLAY_ERROR_STR(shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::clearShadowMap: \""s
		+ key + "\" 키의 그림자 맵이 존재하지 않습니다.", false
	);

	if (!shadowMapData.contains(key)) {
		return;
	}

	auto& data = shadowMapData.at(key)[roomIdx];

	DISPLAY_ERROR_DX_VOID( cmdList->ClearDepthStencilView(
		data.dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0u, 0u, nullptr
	), false );
}

void clearCSMShadowMap(const std::string& key, std::size_t roomIdx, u32t cascadeIdx, ID3D12GraphicsCommandList* cmdList) {
	DISPLAY_ERROR_STR(csmShadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::clearCSMShadowMap: \""s
		+ key + "\" 키의 CSM 그림자 맵이 존재하지 않습니다.", false
	);

	if (!csmShadowMapData.contains(key)) {
		return;
	}

	auto& data = csmShadowMapData.at(key)[roomIdx].cascades[cascadeIdx];

	DISPLAY_ERROR_DX_VOID( cmdList->ClearDepthStencilView(
		data.dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0u, 0u, nullptr
	), false );
}

void getReadyAsDepthWrite(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	DISPLAY_ERROR_STR(shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getReadyAsDepthWrite: \""s
		+ key + "\" 키의 그림자 맵이 존재하지 않습니다.", false
	);

	if (!shadowMapData.contains(key)) {
		return;
	}

	auto& data = shadowMapData.at(key)[roomIdx];

	if (data.curState != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
		transitionResourceState( cmdList, data.tex.res.Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);
		data.curState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}
}

void getReadyAsDepthWrite(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence) {
	DISPLAY_ERROR_STR(shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getReadyAsDepthWrite: \""s
		+ key + "\" 키의 그림자 맵이 존재하지 않습니다.", false
	);

	if (!shadowMapData.contains(key)) {
		return;
	}

	auto& data = shadowMapData.at(key)[roomIdx];

	if (data.curState == D3D12_RESOURCE_STATE_DEPTH_WRITE) {
		return;
	}

	data.curState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

	CommandContext cmdCtxTransition{};
	DISPLAY_ERROR_STR(
		cmdListPool.allocOne(CommandListUsage::RenderingSlave, cmdCtxTransition),
		"[GFX Error] SharedResources::ShadowMap::getReadyAsDepthWrite: 사용 가능한 명령 리스트가 없습니다. "
		"CommandListPool::init 호출이 이루어지지 않았거나, 할당받은 명령 리스트가 반납되지 않았습니다.",
		false
	);
	auto cmdListTransition = cmdCtxTransition.cmdList.Get();
	auto cmdAllocTransition = cmdCtxTransition.cmdAlloc.Get();

	if (!cmdListTransition) {
		return;
	}

	DISPLAY_ERROR_DX_VOID( cmdAllocTransition->Reset(), false );
	DISPLAY_ERROR_DX_VOID( cmdListTransition->Reset(cmdAllocTransition, nullptr), false );

	transitionResourceState( cmdListTransition, data.tex.res.Get(),
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE
	);

	DISPLAY_ERROR_DX_VOID( cmdListTransition->Close(), false );

	ID3D12CommandList* clearCmdLists[] = { cmdListTransition };
	DISPLAY_ERROR_DX_VOID( cmdQ->ExecuteCommandLists(1u, clearCmdLists), false );

	fence.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtxTransition));
}

void getReadyAsShaderResource(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	DISPLAY_ERROR_STR(shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getReadyAsShaderResource: \""s
		+ key + "\" 키의 그림자 맵이 존재하지 않습니다.", false
	);

	if (!shadowMapData.contains(key)) {
		return;
	}

	auto& data = shadowMapData.at(key)[roomIdx];

	if (data.curState != D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) {
		transitionResourceState( cmdList, data.tex.res.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
		);
		data.curState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	}
}

void getReadyAsShaderResource(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence) {
	DISPLAY_ERROR_STR(shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getReadyAsShaderResource: \""s
		+ key + "\" 키의 그림자 맵이 존재하지 않습니다.", false
	);

	if (!shadowMapData.contains(key)) {
		return;
	}

	auto& data = shadowMapData.at(key)[roomIdx];

	if (data.curState == D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) {
		return;
	}

	data.curState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

	CommandContext cmdCtxTransition{};
	DISPLAY_ERROR_STR(
		cmdListPool.allocOne(CommandListUsage::RenderingSlave, cmdCtxTransition),
		"[GFX Error] SharedResources::ShadowMap::getReadyAsShaderResource: 사용 가능한 명령 리스트가 없습니다. "
		"CommandListPool::init 호출이 이루어지지 않았거나, 할당받은 명령 리스트가 반납되지 않았습니다.",
		false
	);
	auto cmdListTransition = cmdCtxTransition.cmdList.Get();
	auto cmdAllocTransition = cmdCtxTransition.cmdAlloc.Get();

	if (!cmdListTransition) {
		return;
	}

	DISPLAY_ERROR_DX_VOID( cmdAllocTransition->Reset(), false );
	DISPLAY_ERROR_DX_VOID( cmdListTransition->Reset(cmdAllocTransition, nullptr), false );

	transitionResourceState( cmdListTransition, data.tex.res.Get(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
	);

	DISPLAY_ERROR_DX_VOID( cmdListTransition->Close(), false );

	ID3D12CommandList* clearCmdLists[] = { cmdListTransition };
	DISPLAY_ERROR_DX_VOID( cmdQ->ExecuteCommandLists(1u, clearCmdLists), false );

	fence.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtxTransition));
}

void getCSMReadyAsDepthWrite(const std::string& key, std::size_t roomIdx, u32t cascadeIdx, ID3D12GraphicsCommandList* cmdList) {
	DISPLAY_ERROR_STR(csmShadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getCSMReadyAsDepthWrite: \""s
		+ key + "\" 키의 CSM 그림자 맵이 존재하지 않습니다.", false
	);

	if (!csmShadowMapData.contains(key)) {
		return;
	}

	auto& slice = csmShadowMapData.at(key)[roomIdx].cascades[cascadeIdx];

	if (slice.curState != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
		transitionResourceState( cmdList, slice.tex.res.Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);
		slice.curState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}
}

void getCSMReadyAsShaderResource(const std::string& key, std::size_t roomIdx, u32t cascadeIdx, ID3D12GraphicsCommandList* cmdList) {
	DISPLAY_ERROR_STR(csmShadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getCSMReadyAsShaderResource: \""s
		+ key + "\" 키의 CSM 그림자 맵이 존재하지 않습니다.", false
	);

	if (!csmShadowMapData.contains(key)) {
		return;
	}

	auto& slice = csmShadowMapData.at(key)[roomIdx].cascades[cascadeIdx];

	if (slice.curState != D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) {
		transitionResourceState( cmdList, slice.tex.res.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
		);
		slice.curState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	}
}

void getCSMAllReadyAsDepthWrite(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence) {
	DISPLAY_ERROR_STR(csmShadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getCSMAllReadyAsDepthWrite: \""s
		+ key + "\" 키의 CSM 그림자 맵이 존재하지 않습니다.", false
	);
	if (!csmShadowMapData.contains(key)) {
		return;
	}

	// 이미 모든 cascade가 DEPTH_WRITE 상태이면 배리어 제출 불필요

	auto& csmData = csmShadowMapData.at(key)[roomIdx];
	bool anyNeedsTransition = false;
	for (u32t ci = 0u; ci < csmData.cascadeCount; ++ci) {
		if (csmData.cascades[ci].curState != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
			anyNeedsTransition = true;
			break;
		}
	}
	if (!anyNeedsTransition) {
		return;
	}

	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR( cmdListPool.allocOne(CommandListUsage::RenderingSlave, cmdCtx),
		"[GFX Error] SharedResources::ShadowMap::getCSMAllReadyAsDepthWrite: 사용 가능한 명령 리스트가 없습니다.", false
	);
	if (!cmdCtx.cmdList) {
		return;
	}

	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdAlloc->Reset(), false );
	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdList->Reset(cmdCtx.cmdAlloc.Get(), nullptr), false );

	for (u32t ci = 0u; ci < csmData.cascadeCount; ++ci) {
		getCSMReadyAsDepthWrite(key, roomIdx, ci, cmdCtx.cmdList.Get());
	}

	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdList->Close(), false );
	ID3D12CommandList* lists[] = { cmdCtx.cmdList.Get() };
	DISPLAY_ERROR_DX_VOID( cmdQ->ExecuteCommandLists(1u, lists), false );
	fence.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

void getCSMAllReadyAsShaderResource(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence) {
	DISPLAY_ERROR_STR(csmShadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getCSMAllReadyAsShaderResource: \""s
		+ key + "\" 키의 CSM 그림자 맵이 존재하지 않습니다.", false
	);
	if (!csmShadowMapData.contains(key)) {
		return;
	}

	auto& csmData = csmShadowMapData.at(key)[roomIdx];
	bool anyNeedsTransition = false;
	for (u32t ci = 0u; ci < csmData.cascadeCount; ++ci) {
		if (csmData.cascades[ci].curState != D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) {
			anyNeedsTransition = true;
			break;
		}
	}
	if (!anyNeedsTransition) {
		return;
	}

	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR( cmdListPool.allocOne(CommandListUsage::RenderingSlave, cmdCtx),
		"[GFX Error] SharedResources::ShadowMap::getCSMAllReadyAsShaderResource: 사용 가능한 명령 리스트가 없습니다.", false
	);
	if (!cmdCtx.cmdList) {
		return;
	}

	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdAlloc->Reset(), false );
	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdList->Reset(cmdCtx.cmdAlloc.Get(), nullptr), false );

	for (u32t ci = 0u; ci < csmData.cascadeCount; ++ci) {
		getCSMReadyAsShaderResource(key, roomIdx, ci, cmdCtx.cmdList.Get());
	}

	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdList->Close(), false );
	ID3D12CommandList* lists[] = { cmdCtx.cmdList.Get() };
	DISPLAY_ERROR_DX_VOID( cmdQ->ExecuteCommandLists(1u, lists), false );
	fence.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

void clearCSMAllShadowMaps(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence) {
	DISPLAY_ERROR_STR(csmShadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::clearCSMAllShadowMaps: \""s
		+ key + "\" 키의 CSM 그림자 맵이 존재하지 않습니다.", false
	);
	if (!csmShadowMapData.contains(key)) {
		return;
	}

	const auto& csmData = csmShadowMapData.at(key)[roomIdx];

	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR( cmdListPool.allocOne(CommandListUsage::RenderingSlave, cmdCtx),
		"[GFX Error] SharedResources::ShadowMap::clearCSMAllShadowMaps: 사용 가능한 명령 리스트가 없습니다.", false
	);
	if (!cmdCtx.cmdList) {
		return;
	}

	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdAlloc->Reset(), false );
	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdList->Reset(cmdCtx.cmdAlloc.Get(), nullptr), false );

	for (u32t ci = 0u; ci < csmData.cascadeCount; ++ci) {
		DISPLAY_ERROR_DX_VOID( cmdCtx.cmdList->ClearDepthStencilView(
			csmData.cascades[ci].dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0u, 0u, nullptr
		), false );
	}

	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdList->Close(), false );
	ID3D12CommandList* lists[] = { cmdCtx.cmdList.Get() };
	DISPLAY_ERROR_DX_VOID( cmdQ->ExecuteCommandLists(1u, lists), false );
	fence.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

void validateRequiredKeys(std::initializer_list<std::string_view> keys) {
	for (auto k : keys) {
		const bool found = csmShadowMapData.contains(std::string(k)) || shadowMapData.contains(std::string(k));
		DISPLAY_ERROR_STR(found, "[GFX Error] SharedResources::ShadowMap::validateRequiredKeys: \""s
			+ std::string(k) + "\" 키의 그림자 맵이 등록되어 있지 않습니다. "
			"loadAssets()보다 Dispatcher를 먼저 생성했거나, 해당 키의 그림자 맵이 등록되지 않았습니다.", true
		);
	}
}

}	// namespace SharedResources::ShadowMap

namespace GBuffer {

std::vector<GBufferData> gBufferData;

namespace {

// 색상 렌더 타겟 텍스처를 생성하고 RTV + SRV를 등록한다.
Texture createColorRT( ID3D12Device* device, u32t width, u32t height,
	DXGI_FORMAT format, DescriptorPool& rtvPool, DescriptorPool& srvTexPool,
	const char* name
) {
	const auto clearVal = D3D12_CLEAR_VALUE{ .Format = format, .Color= {0.f, 0.f, 0.f, 0.f} };
	Texture tex = createTexture( device, width, height, format,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
		clearVal
	);
	setD3DName(tex.res.Get(), name);

	// RTV
	createRTV(device, tex, D3D12_RENDER_TARGET_VIEW_DESC{
		.Format        = format,
		.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
		.Texture2D     = D3D12_TEX2D_RTV{ .MipSlice = 0u, .PlaneSlice = 0u }
	}, rtvPool);

	// SRV (bindless)
	tex.idxSrv.idxRange = etoi(Texture::Type::Tex2D);
	createSRV(device, tex, D3D12_SHADER_RESOURCE_VIEW_DESC{
		.Format                  = format,
		.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D               = D3D12_TEX2D_SRV{ .MostDetailedMip = 0u, .MipLevels = 1u }
	}, srvTexPool);
	tex.idxSrv.idxInArray  = 0;
	tex.idxSrv.idxSampler  = etoi(Samplers::NearestClamp);

	return tex;
}

// GB1 (R16G16_FLOAT) 전용 — 클리어 색상을 (0.5, 0.5, 0, 0)으로 설정하여
// octDecode 시 forward normal (0, 0, 1)이 되도록 한다.
Texture createGB1RT( ID3D12Device* device, u32t width, u32t height,
	DescriptorPool& rtvPool, DescriptorPool& srvTexPool, const char* name
) {
	constexpr DXGI_FORMAT format = DXGI_FORMAT_R16G16_FLOAT;
	const auto clearVal = D3D12_CLEAR_VALUE{ .Format = format, .Color= {0.5f, 0.5f, 0.f, 0.f} };
	Texture tex = createTexture( device, width, height, format,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
		clearVal
	);
	setD3DName(tex.res.Get(), name);

	createRTV(device, tex, D3D12_RENDER_TARGET_VIEW_DESC{
		.Format        = format,
		.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
		.Texture2D     = D3D12_TEX2D_RTV{ .MipSlice = 0u, .PlaneSlice = 0u }
	}, rtvPool);

	tex.idxSrv.idxRange = etoi(Texture::Type::Tex2D);
	createSRV(device, tex, D3D12_SHADER_RESOURCE_VIEW_DESC{
		.Format                  = format,
		.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D               = D3D12_TEX2D_SRV{ .MostDetailedMip = 0u, .MipLevels = 1u }
	}, srvTexPool);
	tex.idxSrv.idxInArray  = 0;
	tex.idxSrv.idxSampler  = etoi(Samplers::NearestClamp);

	return tex;
}

// 깊이 버퍼 텍스처를 생성하고 DSV + SRV(R32_FLOAT)를 등록한다.
Texture createDepthRT( ID3D12Device* device, u32t width, u32t height,
	DescriptorPool& dsvPool, DescriptorPool& srvTexPool, const char* name
) {
	constexpr DXGI_FORMAT format = DXGI_FORMAT_R32_TYPELESS;
	constexpr DXGI_FORMAT formatD = DXGI_FORMAT_D32_FLOAT;

	const auto clearVal = D3D12_CLEAR_VALUE{ .Format = formatD, .DepthStencil = { .Depth = 1.f } };
	Texture tex = createTexture( device, width, height, format,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		clearVal
	);
	setD3DName(tex.res.Get(), name);

	// DSV (D32_FLOAT)
	createDSV(device, tex, D3D12_DEPTH_STENCIL_VIEW_DESC{
		.Format        = DXGI_FORMAT_D32_FLOAT,
		.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
		.Flags         = D3D12_DSV_FLAG_NONE,
		.Texture2D     = D3D12_TEX2D_DSV{ .MipSlice = 0u }
	}, dsvPool);

	// SRV (R32_FLOAT, bindless)
	tex.idxSrv.idxRange = etoi(Texture::Type::Tex2D);
	createSRV(device, tex, D3D12_SHADER_RESOURCE_VIEW_DESC{
		.Format                  = DXGI_FORMAT_R32_FLOAT,
		.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D               = D3D12_TEX2D_SRV{ .MostDetailedMip = 0u, .MipLevels = 1u }
	}, srvTexPool);
	tex.idxSrv.idxInArray  = 0;
	tex.idxSrv.idxSampler  = etoi(Samplers::NearestClamp);

	return tex;
}

}	// anonymous namespace

void addGBuffer( ID3D12Device* device, u32t width, u32t height,
	std::size_t roomCnt, DescriptorPool& rtvPool,
	DescriptorPool& dsvPool, DescriptorPool& srvTexPool
) {
	gBufferData.reserve(roomCnt);

	for (std::size_t r = 0u; r < roomCnt; ++r) {
		GBufferData gb{};
		gb.width  = width;
		gb.height = height;

		auto suffix = "[" + std::to_string(r) + "]";
		gb.gb0   = createColorRT(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,   rtvPool, srvTexPool, ("GBuffer_GB0" + suffix).c_str());
		gb.gb1   = createGB1RT  (device, width, height,                               rtvPool, srvTexPool, ("GBuffer_GB1" + suffix).c_str());
		gb.gb2   = createColorRT(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,   rtvPool, srvTexPool, ("GBuffer_GB2" + suffix).c_str());
		gb.gb3   = createColorRT(device, width, height, DXGI_FORMAT_R8_UNORM,          rtvPool, srvTexPool, ("GBuffer_GB3" + suffix).c_str());
		gb.depth = createDepthRT(device, width, height,                 dsvPool, srvTexPool, ("GBuffer_Depth" + suffix).c_str());

		gb.rtvHandles[0] = rtvPool.cpuHandle(gb.gb0.idxRtv);
		gb.rtvHandles[1] = rtvPool.cpuHandle(gb.gb1.idxRtv);
		gb.rtvHandles[2] = rtvPool.cpuHandle(gb.gb2.idxRtv);
		gb.rtvHandles[3] = rtvPool.cpuHandle(gb.gb3.idxRtv);
		gb.dsvHandle     = dsvPool.cpuHandle(gb.depth.idxDsv);

		gb.curStateGB[0] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gb.curStateGB[1] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gb.curStateGB[2] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gb.curStateGB[3] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gb.curStateDepth = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		gBufferData.push_back(std::move(gb));
	}
}

void transitionToWrite(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	auto& gb = gBufferData[roomIdx];
	Texture* colorTex[4] = { &gb.gb0, &gb.gb1, &gb.gb2, &gb.gb3 };

	for (int i = 0; i < 4; ++i) {
		if (gb.curStateGB[i] != D3D12_RESOURCE_STATE_RENDER_TARGET) {
			transitionResourceState(cmdList, colorTex[i]->res.Get(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			gb.curStateGB[i] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
	}

	if (gb.curStateDepth != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
		transitionResourceState(cmdList, gb.depth.res.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);
		gb.curStateDepth = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}
}

void transitionToRead(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	auto& gb = gBufferData[roomIdx];
	Texture* colorTex[4] = { &gb.gb0, &gb.gb1, &gb.gb2, &gb.gb3 };

	for (int i = 0; i < 4; ++i) {
		if (gb.curStateGB[i] != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
			transitionResourceState(cmdList, colorTex[i]->res.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			);
			gb.curStateGB[i] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
	}

	if (gb.curStateDepth != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
		transitionResourceState(cmdList, gb.depth.res.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
		gb.curStateDepth = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
}

void clearGBuffer(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	auto& gb = gBufferData[roomIdx];

	// GB0, GB2, GB3: clear to (0, 0, 0, 0)
	const float kBlack[4] = { 0.f, 0.f, 0.f, 0.f };
	// GB1: clear to (0.5, 0.5, 0, 0) — octDecode((0.5,0.5)) = (0,0,1) forward normal
	const float kNormalClear[4] = { 0.5f, 0.5f, 0.f, 0.f };

	cmdList->ClearRenderTargetView(gb.rtvHandles[0], kBlack,       0u, nullptr);
	cmdList->ClearRenderTargetView(gb.rtvHandles[1], kNormalClear, 0u, nullptr);
	cmdList->ClearRenderTargetView(gb.rtvHandles[2], kBlack,       0u, nullptr);
	cmdList->ClearRenderTargetView(gb.rtvHandles[3], kBlack,       0u, nullptr);
	cmdList->ClearDepthStencilView(gb.dsvHandle,     D3D12_CLEAR_FLAG_DEPTH, 1.f, 0u, 0u, nullptr);
}

}	// namespace GBuffer

}	// namespace SharedResources
