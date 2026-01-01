#ifndef __GFX_HPP
#define __GFX_HPP

#include "gfxUtil.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "font.hpp"

#include "sharedResources.hpp"
#include "samplePipeline.hpp"
#include "pbrPipeline.hpp"
#include "pbrSkinnedPipeline.hpp"
#include "billboardPipeline.hpp"
#include "skyboxPipeline.hpp"
#include "BVPipeline.hpp"
#include "uiPipeline.hpp"
#include "spriteAnimation.hpp"

// animation update lock
// animBlender도 erase (~23:00)

// physic state에 evVelocity, evOmega 별도 추가 - 물리 계산 적용 X
// 물리 업데이트에 evXX 누산, 메시지 처리 시에 evXX 계산
// evXX로 애니메이션 재생하도록 수정 (~01:00)

// UI Fence 대신 Resource Barrier (~01:30)

// Muzzle Flash 이펙트 올바른 위치에 재생
// 메시지의 pos 꺼내서 피격 이펙트 출력
// player dead 플래그 제대로 하도록 (~03:00)

// 다른 플레이어 hp ui 출력? (~07:00)

// 레벨 디자인? (~09:30)

// 스프라이트 시트 애니메이션 기능
// - append 가능하도록
// - C++ 코드 수정
// 크로스헤어
// 화면에 피격 효과
// 레벨디자인
// Static Dynamic object 분리 추출
// CSM
// timer game이 완전 소유하도록 수정
// metallic, roughness map 별도 지정 가능하도록 Texture Mapping 자유도 높이기 - 12.24.
// function도 사이즈별로 pool 쓰게
// ExecuteCommands 별도 스레드 호출
// Clickable UI 구현
// 마우스 피킹
// 오브젝트 배치 - 12.25.
// CSM 섀도우맵 뷰
// G버퍼 만들어서 특정 텍스처 볼 수 있도록 - 12.24.
// Deferred Shading
// G버퍼 뷰 - 12.26.
// 점조명 그림자매핑
// 다중 그림자매핑 - 12.27.
// Object 클래스 일반화
// 피격 패킷에 들어있는 위치에 이펙트 재생
// 다른 플레이어 체력바 표시
// 몬스터 에셋 원하는 아바타로 뽑아낼 수 있게 연구 - 12.28.
// PhysicState에 충돌 감지 여부, 회복 방식 설정 가능하도록 구현
// 몬스터들 띄우기
// 몬스터들의 랜덤 이동 - 12.29.
// 청크 단위 지형 구현
// 청크 하나 로드 & 테셀레이션
// 청크 파일 관리법 설계, 여러 청크 로드
// 파티클 이펙트(종진)
// 애니메이션된 위치와 방향 얻기, 상대 물리량 얻기
// 파일->리소스 함수 멀티스레드 구현
// 애니메이션 priority 계산
// Rigidbody Physics
// Active Ragdoll
// frustum 충돌처리 구현
// view frustum culling
// software culling
// 멀티스레드 물리
// 멀티스레드 애니메이션
// BoundingVolumeHierarchy-Unity
// BoundingVolumeHierarchy-C++
// 그리드 공간분할
// 공간분할 충돌처리 최적화
// 
// 물리 LOD 설정
// 그래픽 LOD 설정 및 추출
// 그래픽 LOD 구현
// Vegetation (biome 표현)
// 단순 근거리 원거리 공격 구현
// 자연스러운 공격 충돌영역 표현
// 공격 피드백 구현(넉백, 이펙트, 데미지)
// 회피, 방어 기획 및 구현
// 스프링 팔로잉 카메라 구현
// 전투 UI 구현
// 인벤토리 구현
// 다양한 무기, 스킬 구현
// 몬스터 집단 이동형태 구현
// 몬스터 집단 AI 구현
// 전술 시스템 보너스 구현
// 네임드 전투 하나 구현
// 맵 이동 구현
// 보스 전투 하나 구현
// 환경매핑
// Irradiance map 등 빌드 후 표시
// Light Probe 유니티에서 정의 및 추출
// 완벽한 IBL
// PCF 일반화
// VSM, EVSM 등의 그림자 알고리즘
// pn-triangle 테셀레이션
// Bloom
// HDR
// Subsuface Scattering
// SSAO, HBAO+
// Screen Space Reflection
// Volumetric Rendering
// Atmosphere Rendering
// TAA
// fmod 사운드 프로그래밍

extern HWND ghWnd;

struct RequestModelLoad {
	std::filesystem::path modelPath;
	std::unordered_map<std::string, Texture>* pTexHashMap;
	Model* pDest;
};

struct RequestSkyboxLoad {
	std::filesystem::path skyboxPath;
	Skybox* pDest;
};

struct RequestTextureLoad {
	std::string name;
	std::filesystem::path texturePath;
	Texture* pDest;
	std::unordered_map<std::string, Texture>* pTexHashMap;
};

struct RequestSpriteAnimLoad {
	std::filesystem::path spritesPath;
	SpriteAnimationClip* pDest;
	std::unordered_map<std::string, std::vector<Texture>>* pSpritesHashMap;
};

struct RequestTextImageLoad {
	UINT width;
	UINT height;
	TextImage* pDest;
};

// 렌더링을 총괄 책임지는 클래스
// - 장치 초기화: setupDXGI, init, createSwapChain
// - 객체 그리기: addDrawEvent로 객체마다 그려지길 원하는 파이프라인에 등록,
//					이후 render 함수를 호출해 한번에 전부 그리기
class GFX {
public:
	GFX() = default;
	GFX(const GFX&) = delete;
	GFX& operator=(const GFX&) = delete;
	GFX(GFX&&) noexcept = delete;
	GFX& operator=(GFX&&) noexcept = delete;
	// GFX가 소멸할 때, 제출된 모든 GPU작업이 완료되고 나서 소멸하도록 한다.
	~GFX();

	// 장치 초기화: setupDXGI, init, createSwapChain 순으로 호출한다.

	// DXGI Factory를 초기화하고, DXGI Adapter들을 열거한다.
	// 그리고 그 중 하나를 선택하여 curAdapter_에 저장한다.
	void setupDXGI(D3D_FEATURE_LEVEL d3dFeatureLevel);
	// D3D12 Device와 Command Queue, Descriptor Heap, Descriptor Pool들을 만든다.
	// 공용 샘플러들을 생성한다.
	// RenderingSlave, ResourceLoading 카테고리의 Command List Pool을 초기화한다.
	// Root Signature와 Shader(PSO)들을 만든다.
	// Load Fence를 만든다.
	// 그리고 DrawEvent들을 저장하기 위한 메모리를 예약한다.
	void init();
	// 윈도우와 연결된 SwapChain을 만든다.
	// Back Buffer 개수 만큼의 room을 가지는 파이프라인별 ShaderInputBuffer들을 만든다.
	// RenderingMaster 카테고리의 Command List Pool을
	// Back Buffer 개수 * 2의 크기를 갖도록 초기화한다.
	// 그리고 Back Buffer 개수 만큼의 Frame Fence들을 만든다.
	void createSwapChain();

	// 스레드 풀을 설정한다.
	// GFX는 스레드 풀이 설정되어있을 경우 멀티스레드로 동작한다.
	void setThreadPool(ThreadPool* threadPool) { threadPool_ = threadPool; }

	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent(const SamplePipeline::DrawEvent& drawEvent);
	// 카메라 데이터를 입력한다.
	void addCameraData(const SamplePipeline::CameraData& cameraData);
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent(const PBRPipeline::DrawEvent& drawEvent);
	// 카메라 데이터를 입력한다.
	void addCameraData(const PBRPipeline::CameraData& cameraData);
	// 조명 데이터를 입력한다.
	void addLightData(const PBRPipeline::LightData& lightData);
	// 프레임 데이터를 입력한다.
	void addFrameData(const PBRPipeline::FrameData& frameData);
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent(const PBRSkinnedPipeline::DrawEvent& drawEvent);
	// 카메라 데이터를 입력한다.
	void addCameraData(const PBRSkinnedPipeline::CameraData& cameraData);
	// 조명 데이터를 입력한다.
	void addLightData(const PBRSkinnedPipeline::LightData& lightData);
	// 프레임 데이터를 입력한다.
	void addFrameData(const PBRSkinnedPipeline::FrameData& frameData);
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent( const BillboardPipeline::DrawEvent& drawEvent );
	// 카메라 데이터를 입력한다.
	void addCameraData( const BillboardPipeline::CameraData& cameraData );
	// 프레임 데이터를 입력한다.
	void addFrameData( const BillboardPipeline::FrameData& frameData );
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent(const SkyboxPipeline::DrawEvent& drawEvent);
	// 카메라 데이터를 입력한다.
	void addCameraData(const SkyboxPipeline::CameraData& cameraData);
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent(const BVPipeline::DrawEvent& drawEvent);
	// 카메라 데이터를 입력한다.
	void addCameraData(const BVPipeline::CameraData& cameraData);
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent( const UIPipeline::DrawEvent& drawEvent );
	// 프레임 데이터를 입력한다.
	void addFrameData( const UIPipeline::FrameData& frameData );

	void addRequestModelLoad(const RequestModelLoad& request);
	void addRequestSkyboxLoad(const RequestSkyboxLoad& request);
	void addRequestTextureLoad( const RequestTextureLoad& request );
	void addRequestSpritesLoad( const RequestSpriteAnimLoad& request );
	void addRequestTextImageLoad( const RequestTextImageLoad& request );

	// 파이프라인들이 자체적으로 사용하는 리소스들과
	// addRequestXXLoad 꼴의 함수로 요청된 리소스들을 로드한다.
	void loadAssets();

	// 요청된 드로우콜들을 모아 객체들을 그리고 화면에 띄운다.
	void render();

	void WriteTextToBitmap( TextImage* pDestImage, UINT DestWidth, UINT DestHeight, UINT DestPitch, int* piOutWidth, int* piOutHeight, void* pFontObjHandle, const WCHAR* wchString, DWORD dwLen );
	void UpdateTextureWithTextImage( TextImage* srcImage, UINT srcWidth, UINT srcHeight );
	void UpdateTexure( ID3D12Resource* pDestTexResource, ID3D12Resource* pSrcTexResource );

private:
	// 공용 샘플러들 생성
	// gfxUtil.hpp의 Samplers enum과 인덱스를 맞춰주어야 한다.
	void createSamplers();
	// fenceName을 갖는 Fence의 desiredValue 값을 1 증가시키고
	// GPU 큐에 그 갱신 명령을 삽입한다.
	void signalFence(const std::string& fenceName);
	// fenceName을 갖는 Fence에 대해서 wait하고,
	// 사용이 끝난 명령 컨텍스트, 업로드 버퍼 등을 반환한다.
	void waitOnFence(const std::string& fenceName);

	ComPtr<IDXGIFactory4> dxgiFactory_ = nullptr;
	ComPtr<IDXGIAdapter1> curAdapter_ = nullptr;
	std::vector< ComPtr<IDXGIAdapter1> > adapters_{};
	std::vector<DXGI_ADAPTER_DESC1> adapterDescs_{};
	D3D_FEATURE_LEVEL d3dFeatureLevel_{};

	ComPtr<ID3D12Device> device_ = nullptr;
	ComPtr<ID3D12CommandQueue> cmdQ_ = nullptr;

	CommandListPool cmdListPool_{};

	// 스왑 체인 관련 변수들
	DXGI_SWAP_CHAIN_DESC1 scd_{};
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC scfd_{};
	ComPtr<IDXGISwapChain3> swapChain_ = nullptr;
	std::vector<ComPtr<ID3D12Resource>> backBuffers_{};
	std::vector<ComPtr<ID3D12Resource>> depthBuffers_{};
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> backBufferRtvs_{};
	std::vector<int> allocatedRtvIndices_{};
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> depthBufferDsvs_{};
	std::vector<int> allocatedDsvIndices_{};

	// 디스크립터 힙과 디스크립터 풀
	DescriptorHeap rtvHeap_{};
	DescriptorPool rtvPool_{};
	DescriptorHeap dsvHeap_{};
	DescriptorPool dsvPool_{};
	DescriptorHeap srvCbvUavHeap_{};	// gpuVisible, SetDescriptorHeaps 함수 호출 필요
	DescriptorPool srvTexPool_{};	// 파이프라인에서 사용하려면 bind 호출 필요
	DescriptorPool srvTexArrayPool_{};	// 파이프라인에서 사용하려면 bind 호출 필요
	DescriptorPool srvTexCubePool_{};	// 파이프라인에서 사용하려면 bind 호출 필요
	DescriptorHeap samHeap_{};	// gpuVisible, SetDescriptorHeaps 함수 호출 필요
	DescriptorPool samPool_{};	// 파이프라인에서 사용하려면 bind 호출 필요
	DescriptorPool cmpSamPool_{};	// 파이프라인에서 사용하려면 bind 호출 필요

	// 루트 시그너처와 셰이더들
	std::map<std::string, std::shared_ptr<RootSig>> rootSigs_{};
	std::map<std::string, ComPtr<ID3D12PipelineState>> shaders_{};

	// 파이프라인 관련 변수들
	// Sample Pipeline
	std::vector<SamplePipeline::DrawEvent> drawEventsSamplePipeline_{};
	SamplePipeline::Resources resourcesSamplePipeline_{};
	SamplePipeline::CameraData cameraDataSamplePipeline_{};
	// PBR Pipeline
	std::vector<PBRPipeline::DrawEvent> drawEventsPBRPipeline_{};
	PBRPipeline::Resources resourcesPBRPipeline_{};
	PBRPipeline::CameraData cameraDataPBRPipeline_{};
	std::vector<PBRPipeline::LightData> lightDataPBRPipeline_{};
	PBRPipeline::LightData mainDirectionalLightPBRPipeline_{};
	PBRPipeline::FrameData frameDataPBRPipeline_{};
	// PBR-skinned Pipeline
	std::vector<PBRSkinnedPipeline::DrawEvent> drawEventsPBRSkinnedPipeline_{};
	PBRSkinnedPipeline::Resources resourcesPBRSkinnedPipeline_{};
	PBRSkinnedPipeline::CameraData cameraDataPBRSkinnedPipeline_{};
	std::vector<PBRSkinnedPipeline::LightData> lightDataPBRSkinnedPipeline_{};
	PBRSkinnedPipeline::LightData mainDirectionalLightPBRSkinnedPipeline_{};
	PBRSkinnedPipeline::FrameData frameDataPBRSkinnedPipeline_{};
	// Skybox Pipeline
	std::vector<SkyboxPipeline::DrawEvent> drawEventsSkyboxPipeline_{};
	SkyboxPipeline::Resources resourcesSkyboxPipeline_{};
	SkyboxPipeline::CameraData cameraDataSkyboxPipeline_{};
	// Bounding Volume Pipeline
	std::vector<BVPipeline::DrawEvent> drawEventsBVPipeline_{};
	BVPipeline::Resources resourcesBVPipeline_{};
	BVPipeline::CameraData cameraDataBVPipeline_{};
	// Billboard Pipeline
	std::vector<BillboardPipeline::DrawEvent> drawEventsBillboardPipeline_{};
	BillboardPipeline::Resources resourcesBillboardPipeline_{};
	BillboardPipeline::CameraData cameraDataBillboardPipeline_{};
	BillboardPipeline::FrameData frameDataBillboardPipeline_{};
	// UI Pipeline
	std::vector<UIPipeline::DrawEvent> drawEventsUIPipeline_{};
	UIPipeline::Resources resourcesUIPipeline_{};
	UIPipeline::FrameData frameDataUIPipeline_{};

	// Font
	Font font_{};
	FontHandle tahomaFont_{};

	std::map<std::string, Fence> fences_{};

	std::size_t frameIdx_ = 0u;

	std::vector<RequestModelLoad> requestsModelLoad_{};
	std::vector<RequestSkyboxLoad> requestsSkyboxLoad_{};
	std::vector<RequestTextureLoad> requestsTextureLoad_{};
	std::vector<RequestSpriteAnimLoad> requestsSpritesLoad_{};
	std::vector<RequestTextImageLoad> requestsTextImageLoad_{};

	ThreadPool* threadPool_ = nullptr;	// 설정되어있을 경우 멀티스레드로 동작한다.
};

#endif	// __GFX_HPP