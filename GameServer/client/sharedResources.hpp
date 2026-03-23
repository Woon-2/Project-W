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
	D3D12_RESOURCE_STATES curState;
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

// SharedResources::ShadowMap::shadowMapData의 특정 key에 매핑된
// 그림자맵을 clear하는 명령을 기록한다.
// 먼저 addShadowMap 함수를 통해 해당 key의 그림자맵이 추가되어 있어야 한다.
void clearShadowMap(const std::string& key, ID3D12GraphicsCommandList* cmdList);

// 그림자맵의 상태가 현재 Depth Write가 아니라면 Depth Write로 변경한다.
void getReadyAsDepthWrite(const std::string& key, ID3D12GraphicsCommandList* cmdList);
// 그림자맵의 상태가 현재 Depth Write가 아니라면 Depth Write로 변경한다.
// cmdListPool에서 자체적으로 Rendering Slave 타입 명령 컨텍스트 하나를 할당해 명령을 기록하고
// cmdQ를 실행한 뒤, 명령 컨텍스트를 fence에 연관시켜놓는다.
void getReadyAsDepthWrite(const std::string& key, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence);
// 그림자맵의 상태가 현재 Shader Resource가 아니라면 Shader Resource로 변경한다.
void getReadyAsShaderResource(const std::string& key, ID3D12GraphicsCommandList* cmdList);
// 그림자맵의 상태가 현재 Shader Resource가 아니라면 Shader Resource로 변경한다.
// cmdListPool에서 자체적으로 Rendering Slave 타입 명령 컨텍스트 하나를 할당해 명령을 기록하고
// cmdQ를 실행한 뒤, 명령 컨텍스트를 fence에 연관시켜놓는다.
void getReadyAsShaderResource(const std::string& key, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence);

extern std::unordered_map<std::string, ShadowMapData> shadowMapData;

}	// namespace SharedResources::ShadowMap

}	// namespace SharedResources

#endif	// __sharedResources_HPP