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

// CSM 전용 그림자맵 데이터. cascade별로 독립된 Texture2D를 가진다.
// (GS + Texture2DArray 방식 대신 4-pass + separate Texture2D 방식 사용)
struct CSMShadowMapData {
	struct CascadeSlice {
		Texture                     tex;      // Texture2D SRV (bindless, srvTexPool)
		DXGI_FORMAT                 format;
		u32t                        width;
		u32t                        height;
		D3D12_CPU_DESCRIPTOR_HANDLE dsv;
		D3D12_RESOURCE_STATES       curState;
	};
	std::array<CascadeSlice, MAX_CSM_CASCADES> cascades;
	u32t cascadeCount = MAX_CSM_CASCADES;
};

// 둘 이상의 파이프라인이 공유하는 리소스들과 그 초기화 함수들을 두는 네임스페이스
namespace SharedResources {

namespace ShadowMap {

// shadowMapData / csmShadowMapData에서 사용하는 기본 키 이름.
inline constexpr std::string_view kDefaultKey = "ShadowMap";

// SharedResources::ShadowMap::shadowMapData에 특정 key로
// Texture2D 그림자맵을 roomCnt개 생성하여 등록한다. (non-CSM 경로)
// 이미 해당 key가 등록되어 있다면 오류를 출력하고 아무 동작도 하지 않는다.
// format 인자는 depth format이어야 한다. ex) DXGI_FORMAT_D32_FLOAT
void addShadowMap( const std::string& key, ID3D12Device* device,
	DXGI_FORMAT format, u32t width, u32t height,
	std::size_t roomCnt, DescriptorPool& srvTexPool, DescriptorPool& dsvPool
);

// SharedResources::ShadowMap::csmShadowMapData에 특정 key로
// cascade별 독립 Texture2D 그림자맵을 roomCnt개 생성하여 등록한다. (CSM 경로)
// cascadeResolutions[i]는 cascade i의 텍스처 해상도(정사각형)이다.
// cascadeCount는 실제로 사용할 cascade 개수 (최대 MAX_CSM_CASCADES).
// 이미 해당 key가 등록되어 있다면 오류를 출력하고 아무 동작도 하지 않는다.
// format 인자는 depth format이어야 한다. ex) DXGI_FORMAT_D32_FLOAT
void addCSMShadowMap( const std::string& key, ID3D12Device* device,
	DXGI_FORMAT format,
	const std::array<u32t, MAX_CSM_CASCADES>& cascadeResolutions,
	u32t cascadeCount,
	std::size_t roomCnt, DescriptorPool& srvTexPool, DescriptorPool& dsvPool
);

// SharedResources::ShadowMap::shadowMapData의 특정 key에 매핑된
// roomIdx번째 그림자맵을 clear하는 명령을 기록한다.
// 먼저 addShadowMap 혹은 addCSMShadowMap 함수를 통해 해당 key의 그림자맵이 추가되어 있어야 한다.
void clearShadowMap(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);
// CSM cascade ci의 그림자맵을 clear하는 명령을 기록한다.
void clearCSMShadowMap(const std::string& key, std::size_t roomIdx, u32t cascadeIdx, ID3D12GraphicsCommandList* cmdList);

// 그림자맵의 상태가 현재 Depth Write가 아니라면 Depth Write로 변경한다.
void getReadyAsDepthWrite(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);
// 그림자맵의 상태가 현재 Depth Write가 아니라면 Depth Write로 변경한다.
// cmdListPool에서 자체적으로 Rendering Slave 타입 명령 컨텍스트 하나를 할당해 명령을 기록하고
// cmdQ를 실행한 뒤, 명령 컨텍스트를 fence에 연관시켜놓는다.
void getReadyAsDepthWrite(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence);
// 그림자맵의 상태가 현재 Shader Resource가 아니라면 Shader Resource로 변경한다.
void getReadyAsShaderResource(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);
// 그림자맵의 상태가 현재 Shader Resource가 아니라면 Shader Resource로 변경한다.
// cmdListPool에서 자체적으로 Rendering Slave 타입 명령 컨텍스트 하나를 할당해 명령을 기록하고
// cmdQ를 실행한 뒤, 명령 컨텍스트를 fence에 연관시켜놓는다.
void getReadyAsShaderResource(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence);
// CSM shadow map의 cascade ci를 Depth Write 상태로 전환한다.
void getCSMReadyAsDepthWrite(const std::string& key, std::size_t roomIdx, u32t cascadeIdx, ID3D12GraphicsCommandList* cmdList);
// CSM shadow map의 cascade ci를 Shader Resource 상태로 전환한다.
void getCSMReadyAsShaderResource(const std::string& key, std::size_t roomIdx, u32t cascadeIdx, ID3D12GraphicsCommandList* cmdList);
// 모든 cascade를 Depth Write 상태로 전환하는 배리어 명령을 기록하고 제출한다.
void getCSMAllReadyAsDepthWrite(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence);
// 모든 cascade를 Shader Resource 상태로 전환하는 배리어 명령을 기록하고 제출한다.
void getCSMAllReadyAsShaderResource(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence);
// 모든 cascade DSV를 depth=1로 클리어하는 명령을 단 한 번 제출한다.
// 각 shadow pipeline이 개별 클리어 없이 누적 기록할 수 있도록,
// getCSMAllReadyAsDepthWrite() 직후에 호출해야 한다.
void clearCSMAllShadowMaps(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence);

// 미등록 key가 있으면 오류 메시지를 표시하고 std::exit(-1)을 호출한다.
// Dispatcher 생성자에서 필수 리소스 등록 여부를 확인하기 위해 사용한다.
void validateRequiredKeys(std::initializer_list<std::string_view> keys);

// key -> per-room ShadowMapData 벡터. 인덱스는 roomIdx.
extern std::unordered_map<std::string, std::vector<ShadowMapData>>    shadowMapData;
// key -> per-room CSMShadowMapData 벡터. 인덱스는 roomIdx.
extern std::unordered_map<std::string, std::vector<CSMShadowMapData>> csmShadowMapData;

}	// namespace SharedResources::ShadowMap

// GBuffer 텍스처 1세트 (4 color RT + 1 depth RT)
struct GBufferData {
	Texture gb0;    // R8G8B8A8_UNORM  — Albedo.rgb + AO.a
	Texture gb1;    // R16G16_FLOAT    — NormalV oct-encoded
	Texture gb2;    // R8G8B8A8_UNORM  — LightAccum.rgb + Roughness.a
	Texture gb3;    // R8_UNORM        — Metallic
	Texture depth;  // R32_TYPELESS resource; DSV=D32_FLOAT, SRV=R32_FLOAT
	u32t width;
	u32t height;
	// OMSetRenderTargets / ClearRenderTargetView 용 RTV 핸들 (addGBuffer 시 캐싱)
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[4];
	// ClearDepthStencilView 용 DSV 핸들 (addGBuffer 시 캐싱)
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
	// 리소스 상태 추적 (전환 최적화용)
	D3D12_RESOURCE_STATES curStateGB[4];
	D3D12_RESOURCE_STATES curStateDepth;
};

namespace GBuffer {

// gBufferData[roomIdx]: roomIdx번째 방의 GBuffer 리소스 세트.
// addGBuffer() 호출 전에는 비어 있다.
extern std::vector<GBufferData> gBufferData;

// roomCnt개 방 각각에 대해 GBuffer 텍스처 세트를 생성하여 gBufferData에 등록한다.
// rtvPool: 색상 RT 4개 × roomCnt 슬롯이 필요하다 (호출자가 미리 크기 확보).
// dsvPool: 깊이 버퍼 1개 × roomCnt 슬롯이 필요하다.
// srvTexPool: bindless SRV 5개 × roomCnt 슬롯이 필요하다.
void addGBuffer( ID3D12Device* device, u32t width, u32t height,
	std::size_t roomCnt, DescriptorPool& rtvPool,
	DescriptorPool& dsvPool, DescriptorPool& srvTexPool
);

// roomIdx번째 GBuffer의 모든 리소스를 쓰기 상태로 전환한다.
// 색상 RT: PIXEL_SHADER_RESOURCE → RENDER_TARGET
// 깊이 버퍼: PIXEL_SHADER_RESOURCE → DEPTH_WRITE
// (이미 쓰기 상태인 리소스는 전환하지 않는다.)
void transitionToWrite(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);

// roomIdx번째 GBuffer의 모든 리소스를 읽기 상태로 전환한다.
// 색상 RT: RENDER_TARGET → PIXEL_SHADER_RESOURCE
// 깊이 버퍼: DEPTH_WRITE → PIXEL_SHADER_RESOURCE
// (이미 읽기 상태인 리소스는 전환하지 않는다.)
void transitionToRead(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);

// roomIdx번째 GBuffer의 색상 RT 4개와 깊이 버퍼를 클리어한다.
// 호출 전에 transitionToWrite()로 리소스 상태를 쓰기 상태로 만들어야 한다.
void clearGBuffer(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);

}	// namespace GBuffer

}	// namespace SharedResources

#endif	// __sharedResources_HPP
