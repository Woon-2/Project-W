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
		), format, width, height );
		auto& tex = pPair->second.tex;

		tex.idxSrv.idxRange = etoi(Texture::Type::Tex2D);

		createDSV(device, tex, dsvPool);
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

}	// namespace SharedResources::ShadowMap

}	// namespace SharedResources