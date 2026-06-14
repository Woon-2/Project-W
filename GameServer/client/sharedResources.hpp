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
// 모든 cascade를 Depth Write 상태로 전환하는 배리어 명령을 기록한다.
void getCSMAllReadyAsDepthWrite(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);
// 모든 cascade를 Depth Write 상태로 전환하는 배리어 명령을 기록하고 제출한다.
void getCSMAllReadyAsDepthWrite(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence);
// 모든 cascade를 Shader Resource 상태로 전환하는 배리어 명령을 기록한다.
void getCSMAllReadyAsShaderResource(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);
// 모든 cascade를 Shader Resource 상태로 전환하는 배리어 명령을 기록하고 제출한다.
void getCSMAllReadyAsShaderResource(const std::string& key, std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence);
// 모든 cascade DSV를 depth=1로 클리어하는 명령을 단 한 번 기록한다.
// 각 shadow pipeline이 개별 클리어 없이 누적 기록할 수 있도록,
// getCSMAllReadyAsDepthWrite() 직후에 호출해야 한다.
void clearCSMAllShadowMaps(const std::string& key, std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);
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

// 모든 방의 GBuffer 리소스를 해제하고 RTV/DSV/SRV 풀 슬롯을 반납한 뒤 gBufferData를 비운다.
// 해상도가 바뀔 경우 eraseGBuffer 호출 후 변경된 해상도로 addGBuffer를 다시 호출해야 한다.
// (호출 전 GPU가 이 리소스들을 더 이상 사용하지 않도록 idle 상태여야 한다.)
void eraseGBuffer( DescriptorPool& rtvPool, DescriptorPool& dsvPool, DescriptorPool& srvTexPool );

}	// namespace GBuffer

// HDR scene-color render target (R16G16B16A16_FLOAT).
// The deferred lighting + forward passes render linear HDR radiance here,
// and a later tonemap-resolve pass reads it (as a bindless SRV) to produce
// the LDR backbuffer. One set per room.
struct SceneColorData {
	Texture color;                       // R16G16B16A16_FLOAT — RTV + bindless SRV
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;     // OMSetRenderTargets / ClearRenderTargetView 용 (addSceneColor 시 캐싱)
	D3D12_RESOURCE_STATES curState;      // 리소스 상태 추적 (전환 최적화용)
	u32t width;
	u32t height;
};

namespace SceneColor {

// sceneColorData[roomIdx]: roomIdx번째 방의 HDR scene-color RT. addSceneColor() 전에는 비어 있다.
extern std::vector<SceneColorData> sceneColorData;

// roomCnt개 방 각각에 대해 width x height 크기의 R16G16B16A16_FLOAT scene-color RT를 생성한다.
// rtvPool: color RT 1개 × roomCnt, srvTexPool: color SRV 1개 × roomCnt.
// 초기 상태는 RENDER_TARGET.
void addSceneColor( ID3D12Device* device, u32t width, u32t height,
	std::size_t roomCnt, DescriptorPool& rtvPool, DescriptorPool& srvTexPool
);

// roomIdx번째 scene-color RT를 쓰기 상태(RENDER_TARGET)로 전환한다.
// PIXEL_SHADER_RESOURCE → RENDER_TARGET (이미 쓰기면 no-op)
void transitionToWrite(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);
// roomIdx번째 scene-color RT를 읽기 상태(PIXEL_SHADER_RESOURCE)로 전환한다.
// RENDER_TARGET → PIXEL_SHADER_RESOURCE (이미 읽기면 no-op)
void transitionToRead(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);

// 모든 방의 scene-color 리소스를 해제하고 RTV/SRV 풀 슬롯을 반납한 뒤 sceneColorData를 비운다.
// 해상도가 바뀔 경우 eraseSceneColor 호출 후 변경된 해상도로 addSceneColor를 다시 호출해야 한다.
// (호출 전 GPU가 이 리소스들을 더 이상 사용하지 않도록 idle 상태여야 한다.)
void eraseSceneColor( DescriptorPool& rtvPool, DescriptorPool& srvTexPool );

}	// namespace SceneColor

// 로비 대기실 슬롯 캐릭터를 그리는 오프스크린 포트레이트 렌더 타깃.
// 가로 아틀라스 1장(cellCount개 셀 × cellW)에 셀별 viewport로 캐릭터를 그린 뒤,
// color SRV를 UI 슬롯 쿼드에 합성한다. depth는 샘플하지 않으므로 SRV 없이 DSV만 둔다.
struct PortraitRTData {
	Texture color;   // R8G8B8A8_UNORM — RTV + bindless SRV (UI 샘플)
	Texture depth;   // D32_FLOAT      — DSV only (미샘플)
	u32t width;
	u32t height;
	u32t cellCount;
	// OMSetRenderTargets / ClearRenderTargetView 용 핸들 (addPortraitRT 시 캐싱)
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
	// color 리소스 상태 추적 (depth는 항상 DEPTH_WRITE로 유지)
	D3D12_RESOURCE_STATES curStateColor;
};

namespace Portrait {

// portraitData[roomIdx]: roomIdx번째 방의 포트레이트 RT 세트. addPortraitRT() 전에는 비어 있다.
extern std::vector<PortraitRTData> portraitData;

// roomCnt개 방 각각에 대해 cellW*cellCount × cellH 크기의 가로 아틀라스 RT(color+depth)를 생성한다.
// rtvPool: color RT 1개 × roomCnt, dsvPool: depth 1개 × roomCnt, srvTexPool: color SRV 1개 × roomCnt.
void addPortraitRT( ID3D12Device* device, u32t cellW, u32t cellH, u32t cellCount,
	std::size_t roomCnt, DescriptorPool& rtvPool, DescriptorPool& dsvPool, DescriptorPool& srvTexPool
);

// roomIdx번째 color RT를 쓰기 상태(RENDER_TARGET)로 전환한다. (이미 쓰기면 no-op)
void transitionToWrite(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);
// roomIdx번째 color RT를 읽기 상태(PIXEL_SHADER_RESOURCE)로 전환한다. (이미 읽기면 no-op)
void transitionToRead(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);
// roomIdx번째 color를 투명(0,0,0,0)으로, depth를 1.0으로 클리어한다.
// 호출 전 transitionToWrite()로 color를 RENDER_TARGET 상태로 만들어야 한다.
void clearPortraitRT(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);

}	// namespace Portrait

namespace HiZMap {

// clearDepth를 1.0f보다 작은 값으로 설정함으로써
// distance culling의 효과를 동시에 볼 수 있다.
inline constexpr auto clearDepth = 0.9988f;

struct HiZMapData {
	Texture srcTex;
	Texture mips;
	u32t mipLevelCnt;
	u32t srcWidth;
	u32t srcHeight;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;	// srcTex dsv
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle;	// mips srv
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> uavHandles;	// mips uav (per-mip uav)
	// mip별 uav가 차지한 uavPool 슬롯 인덱스. eraseHiZMaps에서 전부 반납해야 한다
	// (mips.idxUav.idxResource는 마지막 mip 인덱스만 보관하므로 그것만으로는 누수됨).
	std::vector<int> uavPoolIndices;
};

extern std::vector<HiZMapData> hiZMaps;

// hiZMap은 화면(뷰포트) 해상도가 바뀌면 다시 만들어야 한다.
// 해상도가 바뀔 경우 eraseHiZMaps를 호출 후 다시 변경된 해상도에 맞게 addHiZMaps를 호출해야 한다.
void addHiZMaps( ID3D12Device* device,
	u32t width, u32t height,
	std::size_t roomCnt, DescriptorPool& srvTexPool, DescriptorPool& uavPool,
	DescriptorPool& dsvPool
);

void eraseHiZMaps( DescriptorPool& srvTexPool,
	DescriptorPool& uavPool, DescriptorPool& dsvPool
);

void clearHiZMap(std::size_t roomIdx, ID3D12GraphicsCommandList* cmdList);
void clearHiZMap(std::size_t roomIdx, CommandListPool& cmdListPool, ID3D12CommandQueue* cmdQ, Fence& fence);

}	// namespace SharedResources::HiZMap

namespace IBL {

// Precomputed image-based-lighting resources. There is a single, scene-wide
// instance (not per-room): the environment is global, so these are computed
// once at load time and shared by every room's lighting pass.
//
//   irradiance  : 32x32 R16G16B16A16_FLOAT cube — diffuse irradiance convolution.
//   prefiltered : 128x128 R16G16B16A16_FLOAT cube w/ 5 mips — specular split-sum.
//   brdfLUT     : 256x256 R16G16_FLOAT 2D — environment BRDF integration.
//
// SRVs are bindless (TextureCube pool for the cubes, Texture2D pool for the LUT).
// UAVs (one per writable mip) are used only by the precompute dispatches.
struct IBLData {
	Texture irradiance;     // cube, TextureCube SRV (srvTexCubePool), 1 UAV
	Texture prefiltered;    // cube, TextureCube SRV (srvTexCubePool), per-mip UAVs
	Texture brdfLUT;        // 2D,   Texture2D   SRV (srvTexPool),    1 UAV

	// Per-mip UAV pool slot indices / GPU handles for the prefiltered cube.
	// (createUAV overwrites Texture::idxUav each call, so each mip is recorded here.)
	std::vector<int>                         prefilteredMipUavIdx;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> prefilteredMipUavHandles;

	int                         irradianceUavIdx     = -1;
	D3D12_GPU_DESCRIPTOR_HANDLE irradianceUavHandle  = {};
	int                         brdfUavIdx           = -1;
	D3D12_GPU_DESCRIPTOR_HANDLE brdfUavHandle        = {};

	u32t prefilteredMipCount = 0u;

	bool created = false;
};

// Single scene-wide IBL resource set. Populated by addIBL(), released by eraseIBL().
extern IBLData iblData;

// Creates the IBL textures (irradiance cube, prefiltered cube, BRDF LUT) and their
// bindless SRVs + per-mip UAVs. Idempotent guard: a second call without eraseIBL is a no-op.
// All textures start in D3D12_RESOURCE_STATE_UNORDERED_ACCESS so precomputeIBL can write
// them directly; precomputeIBL transitions them to PIXEL_SHADER_RESOURCE when done.
void addIBL( ID3D12Device* device,
	DescriptorPool& uavPool, DescriptorPool& srvTexCubePool, DescriptorPool& srvTexPool
);

// Releases the IBL textures and returns every UAV/SRV pool slot.
void eraseIBL( DescriptorPool& uavPool, DescriptorPool& srvTexCubePool, DescriptorPool& srvTexPool );

}	// namespace SharedResources::IBL

}	// namespace SharedResources

#endif	// __sharedResources_HPP
