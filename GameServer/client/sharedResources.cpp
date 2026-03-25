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

std::unordered_map<std::string, std::vector<ShadowMapData>> shadowMapData;

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
	DXGI_FORMAT format, u32t width, u32t height, u32t sliceCount,
	std::size_t roomCnt, DescriptorPool& srvTexArrayPool, DescriptorPool& dsvPool
) {
	DISPLAY_ERROR_STR(!shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::addCSMShadowMap: \""s
		+ key + "\" 키로 이미 그림자맵이 등록되어 있습니다.", false
	);

	if (shadowMapData.contains(key)) {
		return;
	}

	auto& vec = shadowMapData[key];
	vec.reserve(roomCnt);

	for (std::size_t r = 0u; r < roomCnt; ++r) {
		Texture tex{};

		// Texture2DArray 리소스 생성 (cascade 개수 = sliceCount)
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
				.DepthOrArraySize = static_cast<UINT16>(sliceCount),
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

		// Full-array DSV (모든 cascade slice를 한 번에 커버)
		{
			auto dsvDesc = D3D12_DEPTH_STENCIL_VIEW_DESC{
				.Format = format,
				.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY,
				.Flags = D3D12_DSV_FLAG_NONE,
				.Texture2DArray = D3D12_TEX2D_ARRAY_DSV{
					.MipSlice = 0u,
					.FirstArraySlice = 0u,
					.ArraySize = sliceCount
				}
			};
			createDSV(device, tex, dsvDesc, dsvPool);
		}

		// Texture2DArray SRV (bindless IDX_RANGE_TEXTUREARRAY)
		{
			tex.idxSrv.idxRange = etoi(Texture::Type::Tex2DArray);
			createSRV( device, tex, D3D12_SHADER_RESOURCE_VIEW_DESC{
				.Format = convertDepthToColorFormat(format),
				.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Texture2DArray = D3D12_TEX2D_ARRAY_SRV{
					.MostDetailedMip = 0u,
					.MipLevels = 1u,
					.FirstArraySlice = 0u,
					.ArraySize = sliceCount
				}
			}, srvTexArrayPool );

			tex.idxSrv.idxInArray = 0;  // PS uses idx.z = cascadeIdx at sampling time
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

}	// namespace SharedResources::ShadowMap

}	// namespace SharedResources
