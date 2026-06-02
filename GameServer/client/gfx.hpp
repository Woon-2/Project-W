#ifndef __GFX_HPP
#define __GFX_HPP

#include "gfxUtil.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "font.hpp"

#include <functional>

#include "sharedResources.hpp"
#include "samplePipeline.hpp"
#include "pbrPipeline.hpp"
#include "pbrSkinnedPipeline.hpp"
#include "billboardPipeline.hpp"
#include "meshParticlePipeline.hpp"
#include "windRingPipeline.hpp"
#include "smokeBlendCGPipeline.hpp"
#include "blendCGMeshPipeline.hpp"
#include "piercingMeshPipeline.hpp"
#include "piercingSlashMeshPipeline.hpp"
#include "swordSlashPipeline.hpp"
#include "twoSidesPipeline.hpp"
#include "trailPipeline.hpp"
#include "skyboxPipeline.hpp"
#include "BVPipeline.hpp"
#include "uiPipeline.hpp"
#include "terrainPipeline.hpp"
#include "terrainDeferredPipeline.hpp"
#include "spriteAnimation.hpp"
#include "pbrDeferredPipeline.hpp"
#include "pbrDeferredSkinnedPipeline.hpp"

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

struct RequestBakeAnimation {
	std::span< std::vector<mu::Mat4x4> > samples;
	ComPtr<ID3D12Resource> uploadBuffer;
	Texture* pDest;
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
	enum class RenderPath { Forward, Deferred };
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
	// Root Signature, Command Signature와 Shader(PSO)들을 만든다.
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
	void addDrawEvent(const PBRDeferredPipeline::DrawEvent& drawEvent);
	// 카메라 데이터를 입력한다.
	void addCameraData(const PBRDeferredPipeline::CameraData& cameraData);
	// 조명 데이터를 입력한다.
	void addLightData(const PBRDeferredPipeline::LightData& lightData);
	// 프레임 데이터를 입력한다.
	void addFrameData(const PBRDeferredPipeline::FrameData& frameData);
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent(const PBRDeferredSkinnedPipeline::DrawEvent& drawEvent);
	// 카메라 데이터를 입력한다.
	void addCameraData(const PBRDeferredSkinnedPipeline::CameraData& cameraData);
	// 조명 데이터를 입력한다.
	void addLightData(const PBRDeferredSkinnedPipeline::LightData& lightData);
	// 프레임 데이터를 입력한다.
	void addFrameData(const PBRDeferredSkinnedPipeline::FrameData& frameData);
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
	void addDrawEvent( const WindRingPipeline::DrawEvent& drawEvent );
	// 카메라 데이터를 입력한다.
	void addCameraData( const WindRingPipeline::CameraData& cameraData );
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
	void addDrawEvent( const PiercingMeshPipeline::DrawEvent& drawEvent );
	// 카메라 데이터를 입력한다.
	void addCameraData( const PiercingMeshPipeline::CameraData& cameraData );
	// 프레임 데이터를 입력한다.
	void addFrameData( const PiercingMeshPipeline::FrameData& frameData );
	// PiercingSlashMeshPipeline: per-pipeline draw/camera/frame submissions.
	void addDrawEvent( const PiercingSlashMeshPipeline::DrawEvent& drawEvent );
	void addCameraData( const PiercingSlashMeshPipeline::CameraData& cameraData );
	void addFrameData( const PiercingSlashMeshPipeline::FrameData& frameData );
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
	void addDrawEvent( TrailPipeline::DrawEvent&& drawEvent );
	// 카메라 데이터를 입력한다.
	void addCameraData( const TrailPipeline::CameraData& cameraData );
	// 프레임 데이터를 입력한다.
	void addFrameData( const TrailPipeline::FrameData& frameData );
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
	// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
	void addDrawEvent(const TerrainDeferredPipeline::DrawEvent& drawEvent);
	// 카메라 데이터를 입력한다.
	void addCameraData(const TerrainDeferredPipeline::CameraData& cameraData);
	// 조명 데이터를 입력한다.
	void addLightData(const TerrainDeferredPipeline::LightData& lightData);
	// 프레임 데이터를 입력한다.
	void addFrameData(const TerrainDeferredPipeline::FrameData& frameData);
	// Hi-z occlusion culling에 사용할 occluder의 정보를 입력한다.
	void addOccluder(const TerrainPipeline::OccluderInfo& occluderInfo);
	// Hi-z occlusion culling에 사용할 occluder의 정보를 입력한다.
	void addOccluder(const TerrainDeferredPipeline::OccluderInfo& occluderInfo);

	void addRequestModelLoad(const RequestModelLoad& request);
	void addRequestSkyboxLoad(const RequestSkyboxLoad& request);
	void addRequestTextureLoad( const RequestTextureLoad& request );
	void addRequestSpritesLoad( const RequestSpriteAnimLoad& request );
	void addRequestTextImageLoad( const RequestTextImageLoad& request );
	void addRequestMeshBinLoad(const RequestMeshBinLoad& request);
	void addRequestBakeAnimation(const RequestBakeAnimation& request);

	// 파이프라인들이 자체적으로 사용하는 공용 리소스들(그림자맵/GBuffer/HiZ/정적 메시/white 텍스처)을
	// 생성한다. 실행 시 메인 스레드에서 한 번만 호출한다.
	void initSharedResources(const AssetConfigs& configs = AssetConfigs{});

	// addRequestXXLoad 꼴의 함수로 요청된 리소스들(모델/스카이박스/텍스처/메시 등)을 로드한다.
	// initSharedResources 호출 이후에 사용한다. 요청 처리만 수행하므로
	// ThreadPool 워커에서 백그라운드로 호출할 수 있다.
	// (메인 렌더와 공유되는 디스크립터 풀은 자체 뮤텍스로 보호된다.)
	void loadRequestedAssets();

	// 공용 리소스 초기화 + 요청 리소스 로드를 한 번에 수행하는 편의 함수.
	void loadAssets(const AssetConfigs& configs = AssetConfigs{}) {
		initSharedResources(configs);
		loadRequestedAssets();
	}

	// Terrain chunk streaming support.
	// Runs a resource-load recording on a ResourceLoading command context bound to
	// "LoadFence": the recorder records GPU copies / SRV creation using the supplied
	// device, command list, bindless texture pool, and fence. When wait==true the CPU
	// blocks until the GPU finishes (synchronous baseline). MAIN THREAD ONLY.
	// (loadAssets() must have run first so the command list pool / fence exist.)
	void recordTerrainResourceLoad(
		const std::function<void(ID3D12Device*, ID3D12GraphicsCommandList*, DescriptorPool&, Fence&)>& recorder,
		bool wait);

	// Bindless texture descriptor pool — terrain chunk splat/palette SRVs allocate
	// here, and the manager frees a chunk's splat descriptor on unload.
	DescriptorPool& bindlessTexPool() { return srvTexPool_; }
	ID3D12Device*   device()          { return device_.Get(); }

	// 요청된 드로우콜들을 모아 객체들을 그리고 화면에 띄운다.
	void render();

	// CSM cascade 디버그 시각화를 토글한다 ('C' 키에 연결됨).
	void toggleCsmDebugVisualization() { csmDebugVisualization_ = !csmDebugVisualization_; }

	// Hi-Z occlusion culling 활성화 여부 ('H' 키에 연결됨).
	void setHiZCullEnabled(bool v) { hiZCullEnabled_ = v; }
	bool isHiZCullEnabled() const  { return hiZCullEnabled_; }

	// Hi-Z 컬링 통계 (이전 프레임 기준).
	struct HiZStats { u32t visible; u32t total; };
	HiZStats getHiZStats() const;

	// Hi-Z occlusion culling 결과 조회 (1-frame delay).
	// renderObjectId가 범위 밖이거나 Hi-Z 비활성화면 true (visible로 가정).
	bool getHiZObjectVisible(u32t renderObjectId) const;

	// objectVisibility 배열 크기를 [0, maxId] 범위로 초기화 (setupStage 이후 호출).
	void setMaxRenderObjectId(u32t maxId);

	// Deferred / Forward 렌더 경로 선택
	RenderPath renderPath() const { return renderPath_; }
	void setRenderPath(RenderPath path) { renderPath_ = path; }

	// GBuffer 채널 debug 뷰를 순환한다 ('G' 키에 연결됨).
	// None(0) → Albedo → Normal → AO → Roughness → Metallic → LightAccum → Depth → None
	void cycleGBufferDebugMode() { gBufferDebugMode_ = (gBufferDebugMode_ + 1u) % 8u; }

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
	DescriptorPool uavPool_{};	// 파이프라인에서 사용하려면 bind 호출 필요
	DescriptorHeap samHeap_{};	// gpuVisible, SetDescriptorHeaps 함수 호출 필요
	DescriptorPool samPool_{};	// 파이프라인에서 사용하려면 bind 호출 필요
	DescriptorPool cmpSamPool_{};	// 파이프라인에서 사용하려면 bind 호출 필요

	// 루트 시그너처, 명령 시그너처와 셰이더들
	std::map<std::string, std::shared_ptr<RootSig>> rootSigs_{};
	std::shared_ptr<CmdSig> cmdSig_{};
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
	// Wind Ring Pipeline
	std::vector<WindRingPipeline::DrawEvent> drawEventsWindRingPipeline_{};
	WindRingPipeline::Resources              resourcesWindRingPipeline_{};
	WindRingPipeline::CameraData             cameraDataWindRingPipeline_{};
	WindRingPipeline::FrameData              frameDataWindRingPipeline_{};
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
	// Piercing Mesh Pipeline
	std::vector<PiercingMeshPipeline::DrawEvent> drawEventsPiercingMeshPipeline_{};
	PiercingMeshPipeline::Resources              resourcesPiercingMeshPipeline_{};
	PiercingMeshPipeline::CameraData             cameraDataPiercingMeshPipeline_{};
	PiercingMeshPipeline::FrameData              frameDataPiercingMeshPipeline_{};
	// Piercing Slash Mesh Pipeline
	std::vector<PiercingSlashMeshPipeline::DrawEvent> drawEventsPiercingSlashMeshPipeline_{};
	PiercingSlashMeshPipeline::Resources              resourcesPiercingSlashMeshPipeline_{};
	PiercingSlashMeshPipeline::CameraData             cameraDataPiercingSlashMeshPipeline_{};
	PiercingSlashMeshPipeline::FrameData              frameDataPiercingSlashMeshPipeline_{};
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
	// Trail Pipeline
	std::vector<TrailPipeline::DrawEvent>    drawEventsTrailPipeline_{};
	TrailPipeline::Resources                 resourcesTrailPipeline_{};
	TrailPipeline::CameraData                cameraDataTrailPipeline_{};
	TrailPipeline::FrameData                 frameDataTrailPipeline_{};
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
	// Terrain Deferred Pipeline (deferred GBuffer pass)
	std::vector<TerrainDeferredPipeline::DrawEvent> drawEventsTerrainDeferredPipeline_{};
	TerrainDeferredPipeline::Resources              resourcesTerrainDeferredPipeline_{};
	TerrainDeferredPipeline::CameraData             cameraDataTerrainDeferredPipeline_{};
	TerrainDeferredPipeline::LightData              mainDirectionalLightTerrainDeferredPipeline_{};
	TerrainDeferredPipeline::FrameData              frameDataTerrainDeferredPipeline_{};
	// PBR Deferred Pipeline (static mesh GBuffer pass)
	std::vector<PBRDeferredPipeline::DrawEvent> drawEventsPBRDeferredPipeline_{};
	PBRDeferredPipeline::Resources resourcesPBRDeferredPipeline_{};
	PBRDeferredPipeline::CameraData cameraDataPBRDeferredPipeline_{};
	std::vector<PBRDeferredPipeline::LightData> lightDataPBRDeferredPipeline_{};
	PBRDeferredPipeline::LightData mainDirectionalLightPBRDeferredPipeline_{};
	PBRDeferredPipeline::FrameData frameDataPBRDeferredPipeline_{};
	// PBR Deferred Skinned Pipeline (skinned mesh GBuffer pass)
	std::vector<PBRDeferredSkinnedPipeline::DrawEvent> drawEventsPBRDeferredSkinnedPipeline_{};
	PBRDeferredSkinnedPipeline::Resources resourcesPBRDeferredSkinnedPipeline_{};
	PBRDeferredSkinnedPipeline::CameraData cameraDataPBRDeferredSkinnedPipeline_{};
	std::vector<PBRDeferredSkinnedPipeline::LightData> lightDataPBRDeferredSkinnedPipeline_{};
	PBRDeferredSkinnedPipeline::LightData mainDirectionalLightPBRDeferredSkinnedPipeline_{};
	PBRDeferredSkinnedPipeline::FrameData frameDataPBRDeferredSkinnedPipeline_{};
	// Deferred Lighting Pass resources (fullscreen triangle)
	std::vector<PBRDeferredPipeline::LightData> lightDataPBRDeferredLighting_{};
	StructuredBuffer deferredLightingLightData_{};    // t1 per-room
	ConstantBuffer   deferredLightingPerFrameData_{}; // b0 per-room
	// Hi-z Occluder Pass resources
	std::vector<TerrainPipeline::OccluderInfo> occluderInfosTerrain_{};
	std::vector<TerrainDeferredPipeline::OccluderInfo> occluderInfosTerrainDeferred_{};

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
	std::vector<RequestMeshBinLoad> requestsMeshBinLoad_{};
	std::vector<RequestBakeAnimation> requestsBakeAnimation_{};

	ThreadPool* threadPool_ = nullptr;	// 설정되어있을 경우 멀티스레드로 동작한다.
	bool csmDebugVisualization_ = false;
	bool hiZCullEnabled_      = true;
	RenderPath renderPath_    = RenderPath::Deferred;
	u32t gBufferDebugMode_    = 0u;  // 0=None, 1=Albedo, ..., 7=Depth
};

#endif	// __GFX_HPP
