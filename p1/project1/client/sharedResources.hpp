#ifndef __sharedResources_HPP
#define __sharedResources_HPP

#include "gfxUtil.hpp"

// 그림자맵 텍스처와 그와 관련된 자주 쓰이는 데이터들을 담는 구조체
// format은 그림자맵의 SRV와 DSV를 만들 때 활용하고,
// width와 height는 그림자맵을 렌더링할 때 사용할 viewport와 scissor rectangle을
// 만들 때 활용한다.
// dsv는 그림자맵을 클리어할 때 사용한다.
struct ShadowMapData {
	Texture tex;
	DXGI_FORMAT format;
	u32t width;
	u32t height;
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
};

// 둘 이상의 파이프라인이 공유하는 리소스들과 그 초기화 함수들을 두는 네임스페이스
namespace SharedResources {

namespace ShadowMap {

// SharedResources::ShadowMap::shadowMapData에 특정 key로
// 주어진 인자로 생성된 그림자맵을 등록한다.
// 이미 해당 key가 등록되어 있다면 오류를 출력하고 아무 동작도 하지 않는다.
// (그림자맵 생성이 일어나지 않는다.)
// format 인자는 depth format이어야 한다. ex) DXGI_FORMAT_D32_FLOAT
void addShadowMap( const std::string& key, ID3D12Device* device,
	DXGI_FORMAT format, u32t width, u32t height,
	std::size_t roomCnt, DescriptorPool& srvTexPool, DescriptorPool& dsvPool	
);

void clearShadowMap(const std::string& key, ID3D12GraphicsCommandList* cmdList);

extern std::unordered_map<std::string, ShadowMapData> shadowMapData;

}	// namespace SharedResources::ShadowMap

}	// namespace SharedResources

#endif	// __sharedResources_HPP