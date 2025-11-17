#ifndef __GFX_HPP
#define __GFX_HPP

#include "pch.hpp"
#include "gfxUtil.hpp"
#include "shader.hpp"
#include "mesh.hpp"

#include "samplePipeline.hpp"
#include "pbrPipeline.hpp"
#include "skyboxPipeline.hpp"
#include "BVPipeline.hpp"


// 그림자맵 주석 작성
// 2000, lightPos 처리 등 좀 더 표현력, 일반성 있게 구성
// 텍스처 vflip
// 1인칭 & 3인칭 카메라 분리 구현
// 애니메이션 + 총 달기
// gfx에 있는 texHashMap game쪽으로 옮기기
// Rigidbody Physics 구현
// 텍스처 포맷, 밉 개수도 추출
// 그림자맵 서로 다른 파이프라인간 공유할 수 있도록 바꾸기
// deferred shading 구현
// 사운드 프로그래밍
// 네트워크에서 받는 물리 정보 보간/외삽
// CSM
// 나무, 수풀 LOD써서 넣기
// 멀티스레드 업데이트
// 충돌체 렌더링 및 구현 (컬링용)
// Software Culling
// 스프링 팔로잉 카메라 구현
// * 임포트 관련
//   texHashMap 등 에셋 관련 자료구조가 클라이언트쪽에 있을 필요가 있음.
//   스카이박스 임포트 불완전
//   예외처리 좀 더 추가
// * Texture srv/uav의 idxRange 좀 더 표현력 있게 넣는 법 없을까.

// 서버
// 로그인-로그아웃, 룸 구조 만들기
// 2D맵 충돌처리
// ray scanning으로 사격, 피격 처리
// hp, 장탄수, 사격 쿨타임 등 전투 구조 구현
// 로그인 정보 db 연동
// 테스트 가능하도록 더미 로그인 데이터와 더미 플레이어 구현
// 테스트 프로그램 구현

extern HWND ghWnd;

struct RequestModelLoad {
	std::filesystem::path modelPath;
	Model* pDest;
};

struct RequestSkyboxLoad {
	std::filesystem::path skyboxPath;
	Skybox* pDest;
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
	void addDrawEvent(const SkyboxPipeline::DrawEvent& drawEvent);
	// 카메라 데이터를 입력한다.
	void addCameraData(const SkyboxPipeline::CameraData& cameraData);
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent(const BVPipeline::DrawEvent& drawEvent);
	// 카메라 데이터를 입력한다.
	void addCameraData(const BVPipeline::CameraData& cameraData);

	void addRequestModelLoad(const RequestModelLoad& request);
	void addRequestSkyboxLoad(const RequestSkyboxLoad& request);

	// 파이프라인들이 자체적으로 사용하는 리소스들과
	// addRequestXXLoad 꼴의 함수로 요청된 리소스들을 로드한다.
	// 
	void loadAssets();

	// 요청된 드로우콜들을 모아 객체들을 그리고 화면에 띄운다.
	void render();


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

	std::unordered_map<std::string, Texture> texHashMap_{};

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
	PBRPipeline::FrameData frameDataPBRPipeline_{};
	// Skybox Pipeline
	std::vector<SkyboxPipeline::DrawEvent> drawEventsSkyboxPipeline_{};
	SkyboxPipeline::Resources resourcesSkyboxPipeline_{};
	SkyboxPipeline::CameraData cameraDataSkyboxPipeline_{};
	// Bounding Volume Pipeline
	std::vector<BVPipeline::DrawEvent> drawEventsBVPipeline_{};
	BVPipeline::Resources resourcesBVPipeline_{};
	BVPipeline::CameraData cameraDataBVPipeline_{};

	std::map<std::string, Fence> fences_{};

	std::size_t frameIdx_ = 0u;

	std::vector<RequestModelLoad> requestsModelLoad_{};
	std::vector<RequestSkyboxLoad> requestsSkyboxLoad_{};

	ThreadPool* threadPool_;	// 설정되어있을 경우 멀티스레드로 동작한다.
};

#endif	// __GFX_HPP