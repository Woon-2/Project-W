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
#include "meshParticlePipeline.hpp"
#include "smokeBlendCGPipeline.hpp"
#include "blendCGMeshPipeline.hpp"
#include "swordSlashPipeline.hpp"
#include "twoSidesPipeline.hpp"
#include "skyboxPipeline.hpp"
#include "BVPipeline.hpp"
#include "uiPipeline.hpp"
#include "terrainPipeline.hpp"
#include "spriteAnimation.hpp"

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
	bool needsUploadInfo;
	Samplers sampler = Samplers::TrilinearWrap;
};

struct RequestSpriteAnimLoad {
	std::filesystem::path sheetPath;	// DDS 포맷의 스프라이트 시트 경로
	int rows;							// 스프라이트 시트의 행 수
	int cols;							// 스프라이트 시트의 열 수
	int frameCount;						// 실제 유효 프레임 수 (rows * cols 이하)
	SpriteAnimType type;
	Milliseconds frameTime;
	SpriteAnimationClip* pDest;
};

// .meshbin v1 파일 로드 요청.
// texturePath는 파일 내 경로 문자열로부터 "../resources/Textures/" 기준으로 해석한다.
struct RequestMeshBinLoad {
	std::filesystem::path meshPath;
	std::unordered_map<std::string, Texture>* pTexHashMap;
	Mesh*    pDestMesh;
	Texture* pDestTex;
};

struct RequestTextImageLoad {
	UINT width;
	UINT height;
	TextImage* pDest;
};

struct RequestTerrainLoad {
	std::filesystem::path terrainDir;
	std::unordered_map<std::string, Texture>* pTexHashMap;
	TerrainData* pDest;
};

// Configuration passed to GFX::loadAssets()
struct AssetConfigs {
	struct ShadowMapConfig {
		std::string key          = "ShadowMap";
		// cascade별 shadow map 해상도 (index 0 = 가장 가까운 cascade)
		std::array<u32t, MAX_CSM_CASCADES> cascadeResolutions = { 2048u, 1024u, 1024u, 512u };
		u32t        cascadeCount = static_cast<u32t>(MAX_CSM_CASCADES);
		DXGI_FORMAT format       = DXGI_FORMAT_D32_FLOAT;
	} shadowMap;

	struct CascadeConfig {
		float nearZ  = 0.1f;
		float farZ   = 500.f;
		float lambda = 0.8f;
	} cascade;
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
	void addDrawEvent( const MeshParticlePipeline::DrawEvent& drawEvent );
	// 카메라 데이터를 입력한다.
	void addCameraData( const MeshParticlePipeline::CameraData& cameraData );
	// 프레임 데이터를 입력한다.
	void addFrameData( const MeshParticlePipeline::FrameData& frameData );
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent( const SmokeBlendCGPipeline::DrawEvent& drawEvent );
	// 카메라 데이터를 입력한다.
	void addCameraData( const SmokeBlendCGPipeline::CameraData& cameraData );
	// 프레임 데이터를 입력한다.
	void addFrameData( const SmokeBlendCGPipeline::FrameData& frameData );
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent( const BlendCGMeshPipeline::DrawEvent& drawEvent );
	// 카메라 데이터를 입력한다.
	void addCameraData( const BlendCGMeshPipeline::CameraData& cameraData );
	// 프레임 데이터를 입력한다.
	void addFrameData( const BlendCGMeshPipeline::FrameData& frameData );
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent( const SwordSlashPipeline::DrawEvent& drawEvent );
	// 카메라 데이터를 입력한다.
	void addCameraData( const SwordSlashPipeline::CameraData& cameraData );
	// 프레임 데이터를 입력한다.
	void addFrameData( const SwordSlashPipeline::FrameData& frameData );
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent( const TwoSidesPipeline::DrawEvent& drawEvent );
	// 카메라 데이터를 입력한다.
	void addCameraData( const TwoSidesPipeline::CameraData& cameraData );
	// 프레임 데이터를 입력한다.
	void addFrameData( const TwoSidesPipeline::FrameData& frameData );
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
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent(const TerrainPipeline::DrawEvent& drawEvent);
	// 카메라 데이터를 입력한다.
	void addCameraData(const TerrainPipeline::CameraData& cameraData);
	// 조명 데이터를 입력한다.
	void addLightData(const TerrainPipeline::LightData& lightData);
	// 프레임 데이터를 입력한다.
	void addFrameData(const TerrainPipeline::FrameData& frameData);

	void addRequestModelLoad(const RequestModelLoad& request);
	void addRequestSkyboxLoad(const RequestSkyboxLoad& request);
	void addRequestTextureLoad( const RequestTextureLoad& request );
	void addRequestSpritesLoad( const RequestSpriteAnimLoad& request );
	void addRequestTextImageLoad( const RequestTextImageLoad& request );
	void addRequestTerrainLoad(const RequestTerrainLoad& request);
	void addRequestMeshBinLoad(const RequestMeshBinLoad& request);

	// 파이프라인들이 자체적으로 사용하는 리소스들과
	// addRequestXXLoad 꼴의 함수로 요청된 리소스들을 로드한다.
	void loadAssets(const AssetConfigs& configs = AssetConfigs{});

	// 요청된 드로우콜들을 모아 객체들을 그리고 화면에 띄운다.
	void render();

	// CSM cascade 디버그 시각화를 토글한다 ('C' 키에 연결됨).
	void toggleCsmDebugVisualization() { csmDebugVisualization_ = !csmDebugVisualization_; }

	void WriteTextToBitmap( TextImage* pDestImage, UINT DestWidth, UINT DestHeight, UINT DestPitch, int* piOutWidth, int* piOutHeight, void* pFontObjHandle, const WCHAR* wchString, DWORD dwLen, D2D1_COLOR_F color = D2D1::ColorF( D2D1::ColorF::White ) );
	void UpdateTextureWithTextImage( TextImage* srcImage, UINT srcWidth, UINT srcHeight );
	// Creates a TextImage immediately (loadAssets must have been called first).
	void createTextImageImmediate(UINT width, UINT height, TextImage* pDest);
	// Returns the built-in default font handle (Tahoma 16pt).
	FontHandle* defaultFont() { return &tahomaFont_; }
	// Returns a 1x1 white pixel texture for solid-color UI rendering.
	const Texture* solidColorTex() const { return &solidColorImage_.texture; }
	// Creates a new Tahoma FontHandle with the specified point size.
	FontHandle createFont(float fontSize);
	// Measures text extents without rendering to a bitmap.
	void measureText(FontHandle* pFont, const WCHAR* str, DWORD len, float maxW, float maxH, int* outW, int* outH);

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
	// Mesh Particle Pipeline
	std::vector<MeshParticlePipeline::DrawEvent> drawEventsMeshParticlePipeline_{};
	MeshParticlePipeline::Resources              resourcesMeshParticlePipeline_{};
	MeshParticlePipeline::CameraData             cameraDataMeshParticlePipeline_{};
	MeshParticlePipeline::FrameData              frameDataMeshParticlePipeline_{};
	// Smoke Blend CG Pipeline
	std::vector<SmokeBlendCGPipeline::DrawEvent> drawEventsSmokeBlendCGPipeline_{};
	SmokeBlendCGPipeline::Resources              resourcesSmokeBlendCGPipeline_{};
	SmokeBlendCGPipeline::CameraData             cameraDataSmokeBlendCGPipeline_{};
	SmokeBlendCGPipeline::FrameData              frameDataSmokeBlendCGPipeline_{};
	// Blend CG Mesh Pipeline
	std::vector<BlendCGMeshPipeline::DrawEvent> drawEventsBlendCGMeshPipeline_{};
	BlendCGMeshPipeline::Resources              resourcesBlendCGMeshPipeline_{};
	BlendCGMeshPipeline::CameraData             cameraDataBlendCGMeshPipeline_{};
	BlendCGMeshPipeline::FrameData              frameDataBlendCGMeshPipeline_{};
	// Sword Slash Pipeline
	std::vector<SwordSlashPipeline::DrawEvent> drawEventsSwordSlashPipeline_{};
	SwordSlashPipeline::Resources              resourcesSwordSlashPipeline_{};
	SwordSlashPipeline::CameraData             cameraDataSwordSlashPipeline_{};
	SwordSlashPipeline::FrameData              frameDataSwordSlashPipeline_{};
	// Two Sides Pipeline
	std::vector<TwoSidesPipeline::DrawEvent> drawEventsTwoSidesPipeline_{};
	TwoSidesPipeline::Resources              resourcesTwoSidesPipeline_{};
	TwoSidesPipeline::CameraData             cameraDataTwoSidesPipeline_{};
	TwoSidesPipeline::FrameData              frameDataTwoSidesPipeline_{};
	// UI Pipeline
	std::vector<UIPipeline::DrawEvent> drawEventsUIPipeline_{};
	UIPipeline::Resources resourcesUIPipeline_{};
	UIPipeline::FrameData frameDataUIPipeline_{};
	// Terrain Pipeline
	std::vector<TerrainPipeline::DrawEvent> drawEventsTerrainPipeline_{};
	TerrainPipeline::Resources resourcesTerrainPipeline_{};
	TerrainPipeline::CameraData cameraDataTerrainPipeline_{};
	std::vector<TerrainPipeline::LightData> lightDataTerrainPipeline_{};
	TerrainPipeline::LightData              mainDirectionalLightTerrainPipeline_{};
	TerrainPipeline::FrameData              frameDataTerrainPipeline_{};

	// Font
	Font font_{};
	FontHandle tahomaFont_{};
	TextImage  solidColorImage_{};  // 1x1 white pixel — solid-color UI fallback

	std::map<std::string, Fence> fences_{};

	std::size_t frameIdx_ = 0u;

	std::vector<RequestModelLoad> requestsModelLoad_{};
	std::vector<RequestSkyboxLoad> requestsSkyboxLoad_{};
	std::vector<RequestTextureLoad> requestsTextureLoad_{};
	std::vector<RequestSpriteAnimLoad> requestsSpritesLoad_{};
	std::vector<RequestTextImageLoad> requestsTextImageLoad_{};
	std::vector<RequestTerrainLoad> requestsTerrainLoad_{};
	std::vector<RequestMeshBinLoad> requestsMeshBinLoad_{};

	ThreadPool* threadPool_ = nullptr;	// 설정되어있을 경우 멀티스레드로 동작한다.
	bool csmDebugVisualization_ = false;
};

#endif	// __GFX_HPP
