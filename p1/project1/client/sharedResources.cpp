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

std::unordered_map<std::string, ShadowMapData> shadowMapData;

// SharedResources::ShadowMap::shadowMapData에 특정 key로
// 주어진 인자로 생성된 그림자맵을 등록한다.
// 이미 해당 key가 등록되어 있다면 오류를 출력하고 아무 동작도 하지 않는다.
// (그림자맵 생성이 일어나지 않는다.)
// format 인자는 depth format이어야 한다. ex) DXGI_FORMAT_D32_FLOAT
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

	for (std::size_t i = 0u; i < roomCnt; ++i) {
		auto [pPair, _] = shadowMapData.try_emplace( key, createTexture( device, width, height, DXGI_FORMAT_D32_FLOAT,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_CLEAR_VALUE{
				.Format = format,
				.DepthStencil = D3D12_DEPTH_STENCIL_VALUE{ .Depth = 1.f, .Stencil = 0u }
			}
		) );
		auto& data = pPair->second;
		auto& tex = data.tex;

		data.format = format;
		data.width = width;
		data.height = height;
		data.curState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		setD3DName(data.tex.res.Get(), "ShadowMap");

		createDSV(device, tex, dsvPool);
		data.dsv = dsvPool.cpuHandle(tex.idxDsv);

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

		tex.idxSrv.idxSampler = calcIdxBindlessSampler(D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
			D3D12_TEXTURE_ADDRESS_MODE_BORDER, 1u
		);
	}
}

// SharedResources::ShadowMap::shadowMapData의 특정 key에 매핑된
// 그림자맵을 clear하는 명령을 기록한다.
// 먼저 addShadowMap 함수를 통해 해당 key의 그림자맵이 추가되어 있어야 한다.
void clearShadowMap(const std::string& key, ID3D12GraphicsCommandList* cmdList) {
	DISPLAY_ERROR_STR(shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::clearShadowMap: \""s
		+ key + "\" 키의 그림자 맵이 존재하지 않습니다.", false
	);

	if (!shadowMapData.contains(key)) {
		return;
	}

	auto& data = shadowMapData.at(key);

	// shadow map 텍스처를 depth=1.f로 clear한다.
	DISPLAY_ERROR_DX_VOID( cmdList->ClearDepthStencilView(
		data.dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0u, 0u, nullptr
	), false );
}

// 그림자맵의 상태가 현재 Depth Write가 아니라면 Depth Write로 변경한다.
void getReadyAsDepthWrite(const std::string& key, ID3D12GraphicsCommandList* cmdList) {
	DISPLAY_ERROR_STR(shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getReadyAsDepthWrite: \""s
		+ key + "\" 키의 그림자 맵이 존재하지 않습니다.", false
	);

	if (!shadowMapData.contains(key)) {
		return;
	}

	auto& data = shadowMapData.at(key);

	if (data.curState != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
		transitionResourceState( cmdList, data.tex.res.Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE	
		);
		data.curState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}
}

// 그림자맵의 상태가 현재 Depth Write가 아니라면 Depth Write로 변경한다.
// cmdListPool에서 자체적으로 Rendering Slave 타입 명령 컨텍스트 하나를 할당해 명령을 기록하고
// cmdQ를 실행한 뒤, 명령 컨텍스트를 fence에 연관시켜놓는다.
void getReadyAsDepthWrite(const std::string& key, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence) {
	DISPLAY_ERROR_STR(shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getReadyAsDepthWrite: \""s
		+ key + "\" 키의 그림자 맵이 존재하지 않습니다.", false
	);

	if (!shadowMapData.contains(key)) {
		return;
	}

	auto& data = shadowMapData.at(key);

	if (data.curState == D3D12_RESOURCE_STATE_DEPTH_WRITE) {
		return;
	}

	data.curState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

	// 그림자맵 상태 전환을 위한 명령 컨텍스트 할당
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

	// 클리어 명령 리스트 초기화
	DISPLAY_ERROR_DX_VOID( cmdAllocTransition->Reset(), false );
	DISPLAY_ERROR_DX_VOID( cmdListTransition->Reset(cmdAllocTransition, nullptr), false );

	transitionResourceState( cmdListTransition, data.tex.res.Get(),
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE	
	);

	// 상태 전환 명령 기록 끝
	DISPLAY_ERROR_DX_VOID( cmdListTransition->Close(), false );

	ID3D12CommandList* clearCmdLists[] = { cmdListTransition };

	// 상태 전환 명령 리스트 실행
	DISPLAY_ERROR_DX_VOID( cmdQ->ExecuteCommandLists(1u, clearCmdLists), false );

	fence.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtxTransition));
}

// 그림자맵의 상태가 현재 Shader Resource가 아니라면 Shader Resource로 변경한다.
void getReadyAsShaderResource(const std::string& key, ID3D12GraphicsCommandList* cmdList) {
	DISPLAY_ERROR_STR(shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getReadyAsShaderResource: \""s
		+ key + "\" 키의 그림자 맵이 존재하지 않습니다.", false
	);

	if (!shadowMapData.contains(key)) {
		return;
	}

	auto& data = shadowMapData.at(key);

	if (data.curState != D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) {
		transitionResourceState( cmdList, data.tex.res.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE	
		);
		data.curState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	}
}

// 그림자맵의 상태가 현재 Shader Resource가 아니라면 Shader Resource로 변경한다.
// cmdListPool에서 자체적으로 Rendering Slave 타입 명령 컨텍스트 하나를 할당해 명령을 기록하고
// cmdQ를 실행한 뒤, 명령 컨텍스트를 fence에 연관시켜놓는다.
void getReadyAsShaderResource(const std::string& key, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence) {
	DISPLAY_ERROR_STR(shadowMapData.contains(key), "[GFX Error] SharedResources::ShadowMap::getReadyAsShaderResource: \""s
		+ key + "\" 키의 그림자 맵이 존재하지 않습니다.", false
	);

	if (!shadowMapData.contains(key)) {
		return;
	}

	auto& data = shadowMapData.at(key);

	if (data.curState == D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) {
		return;
	}

	data.curState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

	// 그림자맵 상태 전환을 위한 명령 컨텍스트 할당
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

	// 클리어 명령 리스트 초기화
	DISPLAY_ERROR_DX_VOID( cmdAllocTransition->Reset(), false );
	DISPLAY_ERROR_DX_VOID( cmdListTransition->Reset(cmdAllocTransition, nullptr), false );

	transitionResourceState( cmdListTransition, data.tex.res.Get(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE	
	);

	// 상태 전환 명령 기록 끝
	DISPLAY_ERROR_DX_VOID( cmdListTransition->Close(), false );

	ID3D12CommandList* clearCmdLists[] = { cmdListTransition };

	// 상태 전환 명령 리스트 실행
	DISPLAY_ERROR_DX_VOID( cmdQ->ExecuteCommandLists(1u, clearCmdLists), false );

	fence.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtxTransition));
}

}	// namespace SharedResources::ShadowMap

}	// namespace SharedResources