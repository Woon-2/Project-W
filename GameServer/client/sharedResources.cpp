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

void getCSMAllReadyAsDepthWrite(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
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

	for (u32t ci = 0u; ci < csmData.cascadeCount; ++ci) {
		getCSMReadyAsDepthWrite(key, roomIdx, ci, cmdList);
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

void getCSMAllReadyAsShaderResource(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
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

	for (u32t ci = 0u; ci < csmData.cascadeCount; ++ci) {
		getCSMReadyAsShaderResource(key, roomIdx, ci, cmdList);
	}
}

void clearCSMAllShadowMaps(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	DISPLAY_ERROR_STR(csmShadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::clearCSMAllShadowMaps: \""s
		+ key + "\" 키의 CSM 그림자 맵이 존재하지 않습니다.", false
	);
	if (!csmShadowMapData.contains(key)) {
		return;
	}

	const auto& csmData = csmShadowMapData.at(key)[roomIdx];

	for (u32t ci = 0u; ci < csmData.cascadeCount; ++ci) {
		DISPLAY_ERROR_DX_VOID( cmdList->ClearDepthStencilView(
			csmData.cascades[ci].dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0u, 0u, nullptr
		), false );
	}
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

	// Reversed-Z: far plane이 0.0에 매핑되므로 0.0으로 클리어한다.
	const auto clearVal = D3D12_CLEAR_VALUE{ .Format = formatD, .DepthStencil = { .Depth = 0.f } };
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
		gb.gb4   = createColorRT(device, width, height, DXGI_FORMAT_R32_FLOAT,         rtvPool, srvTexPool, ("GBuffer_GB4" + suffix).c_str());
		gb.depth = createDepthRT(device, width, height,                 dsvPool, srvTexPool, ("GBuffer_Depth" + suffix).c_str());

		gb.rtvHandles[0] = rtvPool.cpuHandle(gb.gb0.idxRtv);
		gb.rtvHandles[1] = rtvPool.cpuHandle(gb.gb1.idxRtv);
		gb.rtvHandles[2] = rtvPool.cpuHandle(gb.gb2.idxRtv);
		gb.rtvHandles[3] = rtvPool.cpuHandle(gb.gb3.idxRtv);
		gb.rtvHandles[4] = rtvPool.cpuHandle(gb.gb4.idxRtv);
		gb.dsvHandle     = dsvPool.cpuHandle(gb.depth.idxDsv);

		gb.curStateGB[0] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gb.curStateGB[1] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gb.curStateGB[2] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gb.curStateGB[3] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gb.curStateGB[4] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gb.curStateDepth = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		gBufferData.push_back(std::move(gb));
	}
}

void transitionToWrite(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	auto& gb = gBufferData[roomIdx];
	Texture* colorTex[5] = { &gb.gb0, &gb.gb1, &gb.gb2, &gb.gb3, &gb.gb4 };

	for (int i = 0; i < 5; ++i) {
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
	Texture* colorTex[5] = { &gb.gb0, &gb.gb1, &gb.gb2, &gb.gb3, &gb.gb4 };

	for (int i = 0; i < 5; ++i) {
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
	// GB4 (linear view-space Z): background pixels = 0 (overwritten by skybox in the forward pass)
	const float kZeroZ[4] = { 0.f, 0.f, 0.f, 0.f };

	cmdList->ClearRenderTargetView(gb.rtvHandles[0], kBlack,       0u, nullptr);
	cmdList->ClearRenderTargetView(gb.rtvHandles[1], kNormalClear, 0u, nullptr);
	cmdList->ClearRenderTargetView(gb.rtvHandles[2], kBlack,       0u, nullptr);
	cmdList->ClearRenderTargetView(gb.rtvHandles[3], kBlack,       0u, nullptr);
	cmdList->ClearRenderTargetView(gb.rtvHandles[4], kZeroZ,       0u, nullptr);
	// Reversed-Z: far plane이 0.0에 매핑되므로 0.0으로 클리어한다.
	cmdList->ClearDepthStencilView(gb.dsvHandle,     D3D12_CLEAR_FLAG_DEPTH, 0.f, 0u, 0u, nullptr);
}

void eraseGBuffer( DescriptorPool& rtvPool, DescriptorPool& dsvPool, DescriptorPool& srvTexPool ) {
	for (auto& gb : gBufferData) {
		// 색상 RT 5개: RTV + SRV 반납
		for (auto* colorTex : { &gb.gb0, &gb.gb1, &gb.gb2, &gb.gb3, &gb.gb4 }) {
			freeRTV(*colorTex, rtvPool);
			freeSRV(*colorTex, srvTexPool);
		}
		// 깊이 버퍼: DSV + SRV 반납
		freeDSV(gb.depth, dsvPool);
		freeSRV(gb.depth, srvTexPool);
	}
	gBufferData.clear();
}

}	// namespace GBuffer

namespace SceneColor {

std::vector<SceneColorData> sceneColorData;

namespace {

// HDR scene-color RT(R16G16B16A16_FLOAT)를 생성하고 RTV + bindless SRV를 등록한다.
// tonemap resolve 패스가 fullscreen으로 샘플하므로 BilinearClamp 샘플러를 쓴다.
Texture createSceneColorRT( ID3D12Device* device, u32t width, u32t height,
	DescriptorPool& rtvPool, DescriptorPool& srvTexPool, const char* name
) {
	constexpr DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const auto clearVal = D3D12_CLEAR_VALUE{ .Format = format, .Color = {0.f, 0.f, 0.f, 0.f} };
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
	tex.idxSrv.idxSampler  = etoi(Samplers::BilinearClamp);

	return tex;
}

}	// anonymous namespace

void addSceneColor( ID3D12Device* device, u32t width, u32t height,
	std::size_t roomCnt, DescriptorPool& rtvPool, DescriptorPool& srvTexPool
) {
	sceneColorData.reserve(roomCnt);

	for (std::size_t r = 0u; r < roomCnt; ++r) {
		SceneColorData sc{};
		sc.width  = width;
		sc.height = height;

		const auto suffix = "[" + std::to_string(r) + "]";
		sc.color = createSceneColorRT(device, width, height, rtvPool, srvTexPool,
			("SceneColor" + suffix).c_str());

		sc.rtv      = rtvPool.cpuHandle(sc.color.idxRtv);
		sc.curState = D3D12_RESOURCE_STATE_RENDER_TARGET;

		sceneColorData.push_back(std::move(sc));
	}
}

void transitionToWrite(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	auto& sc = sceneColorData[roomIdx];
	if (sc.curState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		transitionResourceState(cmdList, sc.color.res.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);
		sc.curState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
}

void transitionToRead(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	auto& sc = sceneColorData[roomIdx];
	if (sc.curState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
		transitionResourceState(cmdList, sc.color.res.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
		sc.curState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
}

void eraseSceneColor( DescriptorPool& rtvPool, DescriptorPool& srvTexPool ) {
	for (auto& sc : sceneColorData) {
		freeRTV(sc.color, rtvPool);
		freeSRV(sc.color, srvTexPool);
	}
	sceneColorData.clear();
}

}	// namespace SceneColor

namespace Bloom {

std::vector<BloomData> bloomData;

void addBloom( ID3D12Device* device, u32t fullWidth, u32t fullHeight,
	std::size_t roomCnt, DescriptorPool& rtvPool, DescriptorPool& srvTexPool
) {
	constexpr DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr u32t kMaxBloomMips = 6u;

	bloomData.reserve(roomCnt);

	for (std::size_t r = 0u; r < roomCnt; ++r) {
		BloomData b{};
		b.fullWidth  = fullWidth;
		b.fullHeight = fullHeight;

		const u32t baseW = std::max(1u, fullWidth  / 2u);
		const u32t baseH = std::max(1u, fullHeight / 2u);

		// createTextureWithMips builds the full chain; the bloom pass uses the first N.
		b.mipCount = std::min(kMaxBloomMips, calcMipCount(baseW, baseH));

		b.mips = createTextureWithMips(device, baseW, baseH, format,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		setD3DName(b.mips.res.Get(), ("Bloom_Mips[" + std::to_string(r) + "]").c_str());

		b.mipState.assign(b.mipCount, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		for (u32t m = 0u; m < b.mipCount; ++m) {
			b.width.push_back(std::max(1u, baseW >> m));
			b.height.push_back(std::max(1u, baseH >> m));

			createRTV(device, b.mips, D3D12_RENDER_TARGET_VIEW_DESC{
				.Format        = format,
				.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
				.Texture2D     = D3D12_TEX2D_RTV{ .MipSlice = m, .PlaneSlice = 0u }
			}, rtvPool);
			b.rtv.push_back(rtvPool.cpuHandle(b.mips.idxRtv));
			b.rtvPoolIndices.push_back(b.mips.idxRtv);

			createSRV(device, b.mips, D3D12_SHADER_RESOURCE_VIEW_DESC{
				.Format                  = format,
				.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Texture2D               = D3D12_TEX2D_SRV{ .MostDetailedMip = m, .MipLevels = 1u }
			}, srvTexPool);
			BindlessIndex si{};
			si.idxRange    = etoi(Texture::Type::Tex2D);
			si.idxResource = b.mips.idxSrv.idxResource;
			si.idxInArray  = 0;
			si.idxSampler  = etoi(Samplers::BilinearClamp);
			b.srv.push_back(si);
			b.srvPoolIndices.push_back(b.mips.idxSrv.idxResource);
		}

		bloomData.push_back(std::move(b));
	}
}

void transitionMip( std::size_t roomIdx, u32t mip,
	D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList
) {
	auto& b = bloomData[roomIdx];
	if (b.mipState[mip] == newState) return;
	auto barrier = D3D12_RESOURCE_BARRIER{
		.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
			.pResource   = b.mips.res.Get(),
			.Subresource = mip,
			.StateBefore = b.mipState[mip],
			.StateAfter  = newState
		}
	};
	cmdList->ResourceBarrier(1u, &barrier);
	b.mipState[mip] = newState;
}

BindlessIndex mip0Srv( std::size_t roomIdx ) {
	if (bloomData.empty() || bloomData[roomIdx].srv.empty()) {
		return BindlessIndex{ -1, -1, -1, -1 };
	}
	return bloomData[roomIdx].srv[0];
}

void eraseBloom( DescriptorPool& rtvPool, DescriptorPool& srvTexPool ) {
	for (auto& b : bloomData) {
		for (int idx : b.rtvPoolIndices) rtvPool.free(idx);
		for (int idx : b.srvPoolIndices) srvTexPool.free(idx);
	}
	bloomData.clear();
}

}	// namespace Bloom

namespace Portrait {

std::vector<PortraitRTData> portraitData;

namespace {

// 포트레이트 color RT(R8G8B8A8_UNORM)를 생성하고 RTV + bindless SRV를 등록한다.
Texture createPortraitColor( ID3D12Device* device, u32t width, u32t height,
	DescriptorPool& rtvPool, DescriptorPool& srvTexPool, const char* name
) {
	constexpr DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	const auto clearVal = D3D12_CLEAR_VALUE{ .Format = format, .Color = {0.f, 0.f, 0.f, 0.f} };
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
	tex.idxSrv.idxSampler  = etoi(Samplers::BilinearClamp);

	return tex;
}

// 포트레이트 depth RT(D32_FLOAT)를 생성하고 DSV만 등록한다. (SRV 없음 — depth 미샘플)
Texture createPortraitDepth( ID3D12Device* device, u32t width, u32t height,
	DescriptorPool& dsvPool, const char* name
) {
	constexpr DXGI_FORMAT format = DXGI_FORMAT_R32_TYPELESS;
	constexpr DXGI_FORMAT formatD = DXGI_FORMAT_D32_FLOAT;

	// Reversed-Z: 포트레이트 카메라도 setPerspective를 쓰므로 far plane이 0.0에 매핑된다.
	const auto clearVal = D3D12_CLEAR_VALUE{ .Format = formatD, .DepthStencil = { .Depth = 0.f } };
	Texture tex = createTexture( device, width, height, format,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		clearVal
	);
	setD3DName(tex.res.Get(), name);

	createDSV(device, tex, D3D12_DEPTH_STENCIL_VIEW_DESC{
		.Format        = formatD,
		.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
		.Flags         = D3D12_DSV_FLAG_NONE,
		.Texture2D     = D3D12_TEX2D_DSV{ .MipSlice = 0u }
	}, dsvPool);

	return tex;
}

}	// anonymous namespace

void addPortraitRT( ID3D12Device* device, u32t cellW, u32t cellH, u32t cellCount,
	std::size_t roomCnt, DescriptorPool& rtvPool, DescriptorPool& dsvPool, DescriptorPool& srvTexPool
) {
	const u32t width = cellW * cellCount;
	const u32t height = cellH;

	portraitData.reserve(roomCnt);

	for (std::size_t r = 0u; r < roomCnt; ++r) {
		PortraitRTData p{};
		p.width     = width;
		p.height    = height;
		p.cellCount = cellCount;

		const auto suffix = "[" + std::to_string(r) + "]";
		p.color = createPortraitColor(device, width, height, rtvPool, srvTexPool, ("Portrait_Color" + suffix).c_str());
		p.depth = createPortraitDepth(device, width, height, dsvPool, ("Portrait_Depth" + suffix).c_str());

		p.rtv = rtvPool.cpuHandle(p.color.idxRtv);
		p.dsv = dsvPool.cpuHandle(p.depth.idxDsv);
		p.curStateColor = D3D12_RESOURCE_STATE_RENDER_TARGET;

		portraitData.push_back(std::move(p));
	}
}

void transitionToWrite(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	auto& p = portraitData[roomIdx];
	if (p.curStateColor != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		transitionResourceState(cmdList, p.color.res.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);
		p.curStateColor = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
}

void transitionToRead(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	auto& p = portraitData[roomIdx];
	if (p.curStateColor != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
		transitionResourceState(cmdList, p.color.res.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
		p.curStateColor = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
}

void clearPortraitRT(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	auto& p = portraitData[roomIdx];
	const float kTransparent[4] = { 0.f, 0.f, 0.f, 0.f };
	cmdList->ClearRenderTargetView(p.rtv, kTransparent, 0u, nullptr);
	cmdList->ClearDepthStencilView(p.dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0u, 0u, nullptr);
}

}	// namespace Portrait

namespace HiZMap {

std::vector<HiZMapData> hiZMaps;

// hiZMap은 화면(뷰포트) 해상도가 바뀌면 다시 만들어야 한다.
// 해상도가 바뀔 경우 eraseHiZMaps를 호출 후 다시 변경된 해상도에 맞게 addHiZMaps를 호출해야 한다.
void addHiZMaps( ID3D12Device* device, u32t width, u32t height,
	std::size_t roomCnt, DescriptorPool& srvTexPool, DescriptorPool& uavPool,
	DescriptorPool& dsvPool
) {
	hiZMaps.reserve(roomCnt);

	constexpr DXGI_FORMAT formatF = DXGI_FORMAT_R32_FLOAT;
	constexpr DXGI_FORMAT formatD = DXGI_FORMAT_D32_FLOAT;

	// Reversed-Z: far plane이 0.0에 매핑되므로 0.0으로 클리어한다.
	// (실제 매 프레임 클리어는 clearHiZMap()에서 clearDepth 상수를 사용한다.)
	const auto clearVal = D3D12_CLEAR_VALUE{ .Format = formatD, .DepthStencil = { .Depth = 0.f } };

	for (std::size_t r = 0u; r < roomCnt; ++r) {
		auto& mapData = hiZMaps.emplace_back();

		mapData.srcWidth = width;
		mapData.srcHeight = height;

		mapData.mipLevelCnt = calcMipCount(width, height);

		// level 0 -> dsv로 연결해 occluder 직접 렌더링
		mapData.srcTex = createTexture( device, width, height, formatD,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, clearVal
		);

		setD3DName(mapData.srcTex.res.Get(), "HiZMap_Src");

		createDSV( device, mapData.srcTex, D3D12_DEPTH_STENCIL_VIEW_DESC{
			.Format        = formatD,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags         = D3D12_DSV_FLAG_NONE,
			.Texture2D     = D3D12_TEX2D_DSV{ .MipSlice = 0u }
		}, dsvPool);
		mapData.dsvHandle = dsvPool.cpuHandle(mapData.srcTex.idxDsv);


		mapData.mips = createTextureWithMips( device, width, height,
			formatF, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);

		setD3DName(mapData.mips.res.Get(), "HiZMap_Mips");

		// 읽을 때는 hlsl load/sample 함수에서 mip level을 지정할 수 있기 때문에
		// 하나의 srv를 사용해야 하고,
		// 쓸 때는 반드시 하나의 텍스처를 지정해야 하기 때문에,
		// mip별 uav가 있어야 한다.
		createSRV( device, mapData.mips, D3D12_SHADER_RESOURCE_VIEW_DESC{
			.Format = formatF,
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2D = D3D12_TEX2D_SRV{ .MostDetailedMip = 0u, .MipLevels = mapData.mipLevelCnt }
		}, srvTexPool );
		mapData.srvHandle = srvTexPool.gpuHandle(mapData.mips.idxSrv.idxResource);

		mapData.uavHandles.reserve(mapData.mipLevelCnt);
		mapData.uavPoolIndices.reserve(mapData.mipLevelCnt);
		for (auto i = 0u; i < mapData.mipLevelCnt; ++i) {
			createUAV( device, mapData.mips, D3D12_UNORDERED_ACCESS_VIEW_DESC{
				.Format = formatF,
				.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
				.Texture2D = D3D12_TEX2D_UAV{ .MipSlice = i },
			}, uavPool );
			// createUAV는 mips.idxUav.idxResource에 이번 mip의 풀 슬롯을 기록한다(매번 덮어씀).
			// 해제 시 전부 반납하려면 mip마다 인덱스를 따로 보관해야 한다.
			mapData.uavPoolIndices.push_back( mapData.mips.idxUav.idxResource );
			mapData.uavHandles.push_back( uavPool.gpuHandle(mapData.mips.idxUav.idxResource) );
		}
	}
}

void eraseHiZMaps( DescriptorPool& srvTexPool,
	DescriptorPool& uavPool, DescriptorPool& dsvPool
) {
	for (auto& mapData : hiZMaps) {
		dsvPool.free(mapData.srcTex.idxDsv);

		srvTexPool.free(mapData.mips.idxSrv.idxResource);

		// mip별 uav 슬롯을 전부 반납한다(idxUav.idxResource는 마지막 mip만 보관).
		for (int uavIdx : mapData.uavPoolIndices) {
			uavPool.free(uavIdx);
		}
	}

	hiZMaps.clear();
}

void clearHiZMap(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList) {
	cmdList->ClearDepthStencilView( hiZMaps[roomIdx].dsvHandle,
		D3D12_CLEAR_FLAG_DEPTH, clearDepth, 0u, 0u, nullptr
	);
}

void clearHiZMap(std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence) {
	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR( cmdListPool.allocOne(CommandListUsage::RenderingSlave, cmdCtx),
		"[GFX Error] SharedResources::ShadowMap::clearCSMAllShadowMaps: 사용 가능한 명령 리스트가 없습니다.", false
	);
	if (!cmdCtx.cmdList) {
		return;
	}

	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdAlloc->Reset(), false );
	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdList->Reset(cmdCtx.cmdAlloc.Get(), nullptr), false );

	cmdCtx.cmdList->ClearDepthStencilView( hiZMaps[roomIdx].dsvHandle,
		D3D12_CLEAR_FLAG_DEPTH, clearDepth, 0u, 0u, nullptr
	);

	DISPLAY_ERROR_DX_VOID( cmdCtx.cmdList->Close(), false );
	ID3D12CommandList* lists[] = { cmdCtx.cmdList.Get() };
	DISPLAY_ERROR_DX_VOID( cmdQ->ExecuteCommandLists(1u, lists), false );
	fence.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtx));
}

}	// namespace SharedResources::HiZMap

namespace IBL {

IBLData iblData;

namespace {

// Creates a cubemap-capable Texture2D (DepthOrArraySize = 6) with the requested mip
// count, in the UNORDERED_ACCESS state. createTexture() in gfxUtil only makes
// single-slice Tex2D resources, so the committed resource is built directly here.
Texture createCubeTextureUAV( ID3D12Device* device, u32t res, u32t mips, DXGI_FORMAT format ) {
	Texture tex{};

	const auto desc = D3D12_RESOURCE_DESC{
		.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment        = 0u,
		.Width            = res,
		.Height           = res,
		.DepthOrArraySize = 6u,
		.MipLevels        = static_cast<UINT16>(mips),
		.Format           = format,
		.SampleDesc       = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
	};

	const auto heapProps = D3D12_HEAP_PROPERTIES{ .Type = D3D12_HEAP_TYPE_DEFAULT };

	DISPLAY_ERROR_DX_HR(
		device->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
			__uuidof(ID3D12Resource), &tex.res
		), false
	);

	return tex;
}

}	// anonymous namespace

void addIBL( ID3D12Device* device,
	DescriptorPool& uavPool, DescriptorPool& srvTexCubePool, DescriptorPool& srvTexPool
) {
	if (iblData.created) return;

	constexpr DXGI_FORMAT cubeFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr DXGI_FORMAT lutFormat  = DXGI_FORMAT_R16G16_FLOAT;

	constexpr u32t irradianceRes  = 32u;
	constexpr u32t prefilteredRes = 128u;
	constexpr u32t prefilteredMip = 5u;   // 128, 64, 32, 16, 8
	constexpr u32t brdfRes        = 256u;

	// --- Irradiance cube (single mip) ---
	iblData.irradiance = createCubeTextureUAV( device, irradianceRes, 1u, cubeFormat );
	setD3DName(iblData.irradiance.res.Get(), "IBL_Irradiance");

	createSRV( device, iblData.irradiance, D3D12_SHADER_RESOURCE_VIEW_DESC{
		.Format = cubeFormat,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.TextureCube = D3D12_TEXCUBE_SRV{ .MostDetailedMip = 0u, .MipLevels = 1u }
	}, srvTexCubePool );
	iblData.irradiance.idxSrv.idxRange   = etoi(Texture::Type::TexCube);
	iblData.irradiance.idxSrv.idxSampler = etoi(Samplers::TrilinearClamp);

	createUAV( device, iblData.irradiance, D3D12_UNORDERED_ACCESS_VIEW_DESC{
		.Format = cubeFormat,
		.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY,
		.Texture2DArray = D3D12_TEX2D_ARRAY_UAV{
			.MipSlice = 0u, .FirstArraySlice = 0u, .ArraySize = 6u
		}
	}, uavPool );
	iblData.irradianceUavIdx    = iblData.irradiance.idxUav.idxResource;
	iblData.irradianceUavHandle = uavPool.gpuHandle(iblData.irradianceUavIdx);

	// --- Prefiltered specular cube (per-mip UAVs) ---
	iblData.prefiltered = createCubeTextureUAV( device, prefilteredRes, prefilteredMip, cubeFormat );
	setD3DName(iblData.prefiltered.res.Get(), "IBL_Prefiltered");

	createSRV( device, iblData.prefiltered, D3D12_SHADER_RESOURCE_VIEW_DESC{
		.Format = cubeFormat,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.TextureCube = D3D12_TEXCUBE_SRV{ .MostDetailedMip = 0u, .MipLevels = prefilteredMip }
	}, srvTexCubePool );
	iblData.prefiltered.idxSrv.idxRange   = etoi(Texture::Type::TexCube);
	iblData.prefiltered.idxSrv.idxSampler = etoi(Samplers::TrilinearClamp);

	iblData.prefilteredMipCount = prefilteredMip;
	iblData.prefilteredMipUavIdx.reserve(prefilteredMip);
	iblData.prefilteredMipUavHandles.reserve(prefilteredMip);
	for (u32t m = 0u; m < prefilteredMip; ++m) {
		createUAV( device, iblData.prefiltered, D3D12_UNORDERED_ACCESS_VIEW_DESC{
			.Format = cubeFormat,
			.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY,
			.Texture2DArray = D3D12_TEX2D_ARRAY_UAV{
				.MipSlice = m, .FirstArraySlice = 0u, .ArraySize = 6u
			}
		}, uavPool );
		// createUAV overwrites idxUav.idxResource each call; record each mip's slot.
		iblData.prefilteredMipUavIdx.push_back( iblData.prefiltered.idxUav.idxResource );
		iblData.prefilteredMipUavHandles.push_back( uavPool.gpuHandle(iblData.prefiltered.idxUav.idxResource) );
	}

	// --- BRDF integration LUT (Texture2D) ---
	iblData.brdfLUT = createTexture( device, brdfRes, brdfRes, lutFormat,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	setD3DName(iblData.brdfLUT.res.Get(), "IBL_BRDFLUT");

	createSRV( device, iblData.brdfLUT, D3D12_SHADER_RESOURCE_VIEW_DESC{
		.Format = lutFormat,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D = D3D12_TEX2D_SRV{ .MostDetailedMip = 0u, .MipLevels = 1u }
	}, srvTexPool );
	iblData.brdfLUT.idxSrv.idxRange   = etoi(Texture::Type::Tex2D);
	iblData.brdfLUT.idxSrv.idxSampler = etoi(Samplers::BilinearClamp);

	createUAV( device, iblData.brdfLUT, D3D12_UNORDERED_ACCESS_VIEW_DESC{
		.Format = lutFormat,
		.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
		.Texture2D = D3D12_TEX2D_UAV{ .MipSlice = 0u }
	}, uavPool );
	iblData.brdfUavIdx    = iblData.brdfLUT.idxUav.idxResource;
	iblData.brdfUavHandle = uavPool.gpuHandle(iblData.brdfUavIdx);

	iblData.created = true;
}

void eraseIBL( DescriptorPool& uavPool, DescriptorPool& srvTexCubePool, DescriptorPool& srvTexPool ) {
	if (!iblData.created) return;

	// SRVs
	srvTexCubePool.free(iblData.irradiance.idxSrv.idxResource);
	srvTexCubePool.free(iblData.prefiltered.idxSrv.idxResource);
	srvTexPool.free(iblData.brdfLUT.idxSrv.idxResource);

	// UAVs
	uavPool.free(iblData.irradianceUavIdx);
	for (int uavIdx : iblData.prefilteredMipUavIdx) {
		uavPool.free(uavIdx);
	}
	uavPool.free(iblData.brdfUavIdx);

	iblData = IBLData{};
}

}	// namespace SharedResources::IBL

namespace ColorGrading {

ColorGradingData lutData;

namespace {

// 한 줄에서 공백으로 구분된 RGB float 3개를 읽는다.
bool parseRgbLine(const std::string& line, float& r, float& g, float& b) {
	std::istringstream iss(line);
	return static_cast<bool>(iss >> r >> g >> b);
}

// .cube 파일(텍스트, "LUT_3D_SIZE N" 헤더 + N^3개의 "r g b" 라인)을 파싱해
// R8G8B8A8_UNORM 바이트 버퍼로 변환한다. 표준 .cube 순서(R이 가장 빠르게, G가 중간,
// B가 가장 느리게 변화)가 그대로 Texture3D의 (x=R, y=G, z=B) 축과 일치한다.
bool parseCubeLUT(const std::filesystem::path& path, u32t& outSize, std::vector<u8t>& outData) {
	std::ifstream file(path);
	if (!file.is_open()) {
		DISPLAY_ERROR_STR(false, "[GFX Error] SharedResources::ColorGrading::parseCubeLUT: \""s
			+ path.string() + "\" 파일을 열 수 없습니다.", false
		);
		return false;
	}

	u32t size = 0u;
	std::string line;
	while (std::getline(file, line)) {
		std::istringstream iss(line);
		std::string token;
		if (!(iss >> token)) continue;
		if (token == "LUT_3D_SIZE") {
			iss >> size;
			break;
		}
		// TITLE / DOMAIN_MIN / DOMAIN_MAX / # 주석 등의 다른 헤더 라인은 무시한다.
	}

	if (size == 0u) {
		DISPLAY_ERROR_STR(false, "[GFX Error] SharedResources::ColorGrading::parseCubeLUT: \""s
			+ path.string() + "\"에서 LUT_3D_SIZE를 찾지 못했습니다.", false
		);
		return false;
	}

	const auto texelCnt = static_cast<std::size_t>(size) * size * size;
	outData.assign(texelCnt * 4u, 0u);

	std::size_t idx = 0u;
	float r = 0.f, g = 0.f, b = 0.f;
	while (idx < texelCnt && std::getline(file, line)) {
		if (!parseRgbLine(line, r, g, b)) continue;
		outData[idx * 4u + 0u] = static_cast<u8t>(std::clamp(r, 0.f, 1.f) * 255.f + 0.5f);
		outData[idx * 4u + 1u] = static_cast<u8t>(std::clamp(g, 0.f, 1.f) * 255.f + 0.5f);
		outData[idx * 4u + 2u] = static_cast<u8t>(std::clamp(b, 0.f, 1.f) * 255.f + 0.5f);
		outData[idx * 4u + 3u] = 255u;
		++idx;
	}

	if (idx != texelCnt) {
		DISPLAY_ERROR_STR(false, "[GFX Error] SharedResources::ColorGrading::parseCubeLUT: \""s
			+ path.string() + "\"의 데이터 라인 수가 LUT_3D_SIZE^3과 일치하지 않습니다.", false
		);
		return false;
	}

	outSize = size;
	return true;
}

}	// anonymous namespace

void addColorGradingLUT( ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	Fence& fenceToAssociate, DescriptorPool& srvTex3DPool, const std::filesystem::path& lutPath
) {
	if (lutData.created) return;

	u32t size = 0u;
	std::vector<u8t> rgba8{};
	if (!parseCubeLUT(lutPath, size, rgba8)) return;

	constexpr DXGI_FORMAT lutFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	lutData.size = size;
	lutData.lut  = createTexture3D( device, size, lutFormat,
		D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST
	);
	setD3DName(lutData.lut.res.Get(), "ColorGradingLUT_"s + lutPath.stem().string());

	const UINT rowPitch   = size * 4u;
	const UINT slicePitch = rowPitch * size;

	const D3D12_SUBRESOURCE_DATA subresource{
		.pData      = rgba8.data(),
		.RowPitch   = rowPitch,
		.SlicePitch = slicePitch
	};

	const auto requiredBytes = GetRequiredIntermediateSize(lutData.lut.res.Get(), 0u, 1u);
	auto uploadBuffer = createBufferResource(device, nullptr, requiredBytes, BufferCreationType::UploadBuffer);

	DISPLAY_ERROR_DX_VOID(
		UpdateSubresources(cmdList, lutData.lut.res.Get(), uploadBuffer.Get(), 0, 0, 1u, &subresource), false
	);
	fenceToAssociate.associatedResources_.push_back(std::move(uploadBuffer));

	transitionResourceState(cmdList, lutData.lut.res.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);

	createSRV( device, lutData.lut, D3D12_SHADER_RESOURCE_VIEW_DESC{
		.Format = lutFormat,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture3D = D3D12_TEX3D_SRV{ .MostDetailedMip = 0u, .MipLevels = 1u, .ResourceMinLODClamp = 0.f }
	}, srvTex3DPool );
	lutData.lut.idxSrv.idxRange   = etoi(Texture::Type::Tex3D);
	lutData.lut.idxSrv.idxSampler = etoi(Samplers::TrilinearClamp);
	// Tex3D는 array slice가 없어 idxInArray가 비어 있으므로, half-texel 보정에 필요한
	// LUT 한 축의 해상도(N)를 여기 실어 셰이더(sampleBindless3D)로 전달한다.
	lutData.lut.idxSrv.idxInArray = static_cast<i32t>(size);

	lutData.created = true;
}

void eraseColorGradingLUT( DescriptorPool& srvTex3DPool ) {
	if (!lutData.created) return;

	srvTex3DPool.free(lutData.lut.idxSrv.idxResource);
	lutData = ColorGradingData{};
}

}	// namespace SharedResources::ColorGrading

}	// namespace SharedResources
