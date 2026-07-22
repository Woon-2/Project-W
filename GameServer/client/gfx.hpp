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
#include "energyOrbPipeline.hpp"
#include "HeatDistortionPipeline.hpp"
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
#include "minimapTerrainPipeline.hpp"
#include "minimapFogBlurPipeline.hpp"
#include "minimapPropPipeline.hpp"
#include "terrainDeferredPipeline.hpp"
#include "spriteAnimation.hpp"
#include "pbrDeferredPipeline.hpp"
#include "TonemapPipeline.hpp"
#include "BloomPipeline.hpp"
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
		std::array<u32t, MAX_CSM_CASCADES> cascadeResolutions = { 4096u, 4096u, 2048u, 2048u };
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

	// 제출된 모든 GPU 작업(프레임 + 로드)이 끝날 때까지 블로킹 대기한다.
	// Game 소멸자 본문에서 멤버 소멸 전에 호출해야 한다: gfx_보다 뒤에 선언된
	// 멤버(지형/파티클/UI 텍스처 등)는 ~GFX의 드레인보다 먼저 파괴되므로,
	// GPU가 아직 참조 중인 리소스를 해제해 디바이스 행(TDR)을 유발할 수 있다.
	void drainGpu();

	// Present의 vsync 여부(기본 on). 한 GPU에서 여러 클라이언트가 vsync 없이
	// 풀스피드로 Present하면 DWM까지 굶겨 TDR(디바이스 제거)을 유발할 수 있다.
	void setVsync(bool enabled) { vsyncEnabled_ = enabled; }

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

	// 런타임 해상도 변경: GPU를 idle 시킨 뒤 스왑체인 백버퍼/깊이버퍼/GBuffer/HiZ맵을
	// 새 해상도(width×height)로 재생성한다. 뷰포트/시저는 매 프레임 gClientRect에서
	// 계산되므로 호출 전에 gClientRect가 갱신되어 있어야 한다(applyClientResolution 참고).
	// 메인(렌더) 스레드에서, render()와 같은 프레임의 update 단계에서 호출해야 한다.
	void resize(u32t width, u32t height);

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
	void addDrawEvent( const EnergyOrbPipeline::DrawEvent& drawEvent );
	// 카메라 데이터를 입력한다.
	void addCameraData( const EnergyOrbPipeline::CameraData& cameraData );
	// 프레임 데이터를 입력한다.
	void addFrameData( const EnergyOrbPipeline::FrameData& frameData );
	// Heat-distortion (boss intimidation): per-frame screen-space sources. The game
	// computes screen center/radius/tint/depth per boss and pushes them here before
	// render(). Consumed by both the pre-bloom haze pass and the tonemap warp, then
	// cleared at the end of render(). setHeatGlobals sets shared time/strength.
	void addHeatSource( const HeatDistortionShader::HeatSource& source );
	void setHeatGlobals( float timeSec, float warpStrength, float glowStrength );

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
	// Submits a trail drawn into SceneColorHDR BEFORE bloom (glowing path-guidance
	// ribbon). Shares TrailPipeline's camera/frame data with the regular trail pass.
	void addHDRTrailDrawEvent( TrailPipeline::DrawEvent&& drawEvent );
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
	// UI 클리핑 영역 스택(스크롤 영역용). push 시 현재 top과 교집합을 취한다.
	// 이후 제출되는 UIPipeline::DrawEvent에 클립이 stamping된다.
	void pushUIClip( const RECT& rect );
	void popUIClip();
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
	// 정적 prop occluder(근거리 BVH prop)를 Hi-Z source depth에 기록하기 위해 입력한다.
	void addOccluder(const PBRDeferredPipeline::OccluderInfo& occluderInfo);

	// ===== 로비 대기실 슬롯 포트레이트 (오프스크린 RT → UI 합성) =====
	// 슬롯 수 / 셀 해상도(세로형). 포트레이트 카메라 aspect = kPortraitCellW / kPortraitCellH.
	static constexpr u32t kMaxPortraitSlots = 4u;
	static constexpr u32t kPortraitCellW    = 384u;
	static constexpr u32t kPortraitCellH    = 640u;
	// 슬롯당 캐릭터 1명이지만 Object::render는 submesh마다 DrawEvent를 내고
	// mainUpdate는 boneData를 draw event마다 누적하므로, 상한을 submesh 단위로 잡는다.
	static constexpr u32t kMaxPortraitDrawEventsPerSlot = 64u;
	static constexpr u32t kMaxPortraitBonesPerCharacter = 256u;
	static constexpr u32t kMaxPortraitBonesPerSlot = kMaxPortraitBonesPerCharacter * kMaxPortraitDrawEventsPerSlot;
	// 슬롯당 장착 무기 1개분의 submesh 상한(non-skinned 정적 메시).
	static constexpr u32t kMaxPortraitStaticDrawEventsPerSlot = 16u;

	// 슬롯 slot의 캐릭터 스킨드 draw event를 제출한다(포워드 PBRSkinnedPipeline).
	void addLobbyPortraitDrawEvent(u32t slot, PBRSkinnedPipeline::DrawEvent&& drawEvent);
	// 슬롯 slot의 장착 무기(non-skinned) draw event를 제출한다(포워드 PBRPipeline).
	void addLobbyPortraitDrawEventStatic(u32t slot, PBRPipeline::DrawEvent&& drawEvent);
	// 슬롯 slot의 포트레이트 카메라를 설정한다.
	void setLobbyPortraitCamera(u32t slot, const PBRSkinnedPipeline::CameraData& cameraData);
	// 포트레이트 조명(로비 방향광, 모든 슬롯 공유)을 추가한다. shadow는 끄되 direct light는 넣는다.
	void addLobbyPortraitLightData(const PBRSkinnedPipeline::LightData& lightData);
	// 포트레이트 프레임 데이터(globalAmbient)를 설정한다.
	void addLobbyPortraitFrameData(const PBRSkinnedPipeline::FrameData& frameData);
	// 대기실 활성 여부. true인 동안 render()가 포트레이트 패스를 수행(0슬롯이어도 clear+transitionToRead).
	void setLobbyPortraitActive(bool active);
	// 이번 프레임 render()가 사용할 room의 포트레이트 color 텍스처. UI 슬롯이 이 텍스처를 샘플한다.
	const Texture* lobbyPortraitTextureForThisFrame() const;
	// 슬롯 slot 셀의 sub-rect uvScaleBias(uv' = uv*xy + zw)를 반환한다.
	XMFLOAT4 lobbyPortraitCellUvScaleBias(u32t slot) const;

	// ===== 미니맵 배경 캐시 (지형 diffuse + prop + fog-of-war) =====
	static constexpr u32t kMinimapRTSize       = 1024u;  // 캐시 RT 해상도(정사각). 단일 RT라 메모리 저렴
	// 캐시 텍스처가 덮는 월드 변(邊, 미터). 청크 크기(200m)와 무관하게 시야에 맞춰 작게 잡아
	// 해상도(px/m)를 확보한다. 매 프레임 이 텍스처를 UV sub-rect로 스크롤(MinimapHUD)하고,
	// 플레이어가 중심에서 kMinimapRebakeMoveThreshold 이상 벗어나면 재굽기(plus 청크 로드/언로드).
	static constexpr float kMinimapCoverageWorld = 360.f;
	static constexpr float kMinimapRebakeMoveThreshold = 50.f;  // 이 거리 이상 이동 시 재굽기
	static constexpr float kMinimapWorldRadius = 60.f;   // 기본 시야 반경(미터, 줌 base) — MinimapHUD가 사용
	static constexpr float kMinimapFogBlurRadiusTexels = 48.f;  // fog-of-war 페이드 폭(텍셀, 1024 기준)

	// 이번 프레임 render()가 미니맵 캐시를 재굽도록 요청한다. TerrainChunkManager가
	// minimapDirty()를 보고 호출 — Portrait와 달리 요청된 프레임 한 번만 수행된다.
	void requestMinimapRebake();
	// 재굽기 패스에 지형 청크 draw event를 제출한다(요청되지 않은 프레임에는 버려진다).
	void addMinimapDrawEvent(MinimapTerrainPipeline::DrawEvent&& drawEvent);
	// 재굽기 패스에 scatter prop(나무/바위 등) draw event를 제출한다(지형 위에 top-down albedo로 굽힘).
	void addMinimapPropDrawEvent(MinimapPropPipeline::DrawEvent&& drawEvent);
	// 미니맵 직교 카메라(North-up, 플레이어 중심)를 설정한다.
	void setMinimapCamera(const MinimapTerrainPipeline::CameraData& cameraData);
	// 마지막으로 재굽힌 미니맵 캐시의 최종(블러+합성 완료) 텍스처. 매 프레임 UI가 샘플한다.
	const Texture* minimapTextureForThisFrame() const;

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

	// Coarse progress of the most recent loadRequestedAssets() batch: fraction of
	// queued requests already processed (0..1). Lock-free; safe to read from another
	// thread while a background load runs. Returns 1.0 when nothing is queued.
	float assetLoadFraction() const noexcept {
		const auto total = assetLoadTotal_.load(std::memory_order_relaxed);
		if (total == 0u) return 1.f;
		const auto done = assetLoadDone_.load(std::memory_order_relaxed);
		return (std::min)(1.f, static_cast<float>(done) / static_cast<float>(total));
	}

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
	// None(0) → Albedo → Normal → AO → Roughness → Metallic → LightAccum → Depth
	//        → IBL diffuse → IBL specular → BRDF → CSM cascade 0~3 shadow map → None
	void cycleGBufferDebugMode() { gBufferDebugMode_ = (gBufferDebugMode_ + 1u) % 15u; }

	// Returns false when text rendering fails (e.g. device loss); callers keep
	// their text dirty and retry on a later frame.
	bool WriteTextToBitmap( TextImage* pDestImage, UINT DestWidth, UINT DestHeight, UINT DestPitch, int* piOutWidth, int* piOutHeight, void* pFontObjHandle, const WCHAR* wchString, DWORD dwLen, D2D1_COLOR_F color = D2D1::ColorF( D2D1::ColorF::White ) );
	void UpdateTextureWithTextImage( TextImage* srcImage, UINT srcWidth, UINT srcHeight );
	// Creates a TextImage immediately (loadAssets must have been called first).
	void createTextImageImmediate(UINT width, UINT height, TextImage* pDest);
	// Returns the built-in default font handle (Tahoma 16pt).
	FontHandle* defaultFont() { return &tahomaFont_; }
	// Returns a 1x1 white pixel texture for solid-color UI rendering.
	const Texture* solidColorTex() const { return &solidColorImage_.texture; }
	// Creates a new Tahoma FontHandle with the specified point size.
	FontHandle createFont(float fontSize);
	// Creates a font using a system font family. Existing callers remain Regular
	// unless they explicitly request another weight.
	FontHandle createFont(
		const WCHAR* fontFamilyName,
		float fontSize,
		DWRITE_FONT_WEIGHT fontWeight = DWRITE_FONT_WEIGHT_REGULAR);
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
	// Owns the dedicated submission thread; every ExecuteCommandLists / Present / Signal
	// on cmdQ_ is routed through this so the queue is touched by one thread only.
	// Declared after cmdQ_ so it is destroyed (thread joined) before the queue is released.
	std::unique_ptr<RenderSubmitter> submitter_;

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
	DescriptorPool srvTex3DPool_{};	// 파이프라인에서 사용하려면 bind 호출 필요 (color grading LUT 등 volume texture)
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
	// 로비 대기실 슬롯 포트레이트 (슬롯별 전용 채널 — 버퍼 덮어쓰기 방지)
	std::array<std::vector<PBRSkinnedPipeline::DrawEvent>, kMaxPortraitSlots> drawEventsLobbyPortrait_{};
	std::array<PBRSkinnedPipeline::Resources, kMaxPortraitSlots> resourcesLobbyPortrait_{};
	std::array<PBRSkinnedPipeline::CameraData, kMaxPortraitSlots> cameraDataLobbyPortrait_{};
	std::vector<PBRSkinnedPipeline::LightData> lightDataLobbyPortrait_{};   // 모든 슬롯 공유
	PBRSkinnedPipeline::LightData mainDirectionalLightLobbyPortrait_{};      // cascadeCount=0 (no-shadow)
	PBRSkinnedPipeline::FrameData frameDataLobbyPortrait_{};
	bool lobbyPortraitActive_ = false;
	// 슬롯별 장착 무기(non-skinned) draw event. 카메라/조명/프레임 데이터는 위 스킨드 포트레이트
	// 값(cameraDataLobbyPortrait_/lightDataLobbyPortrait_/frameDataLobbyPortrait_)을 그대로
	// PBRPipeline 타입으로 변환해 공유한다(필드 구성 동일, 별도 멤버 불필요).
	std::array<std::vector<PBRPipeline::DrawEvent>, kMaxPortraitSlots> drawEventsLobbyPortraitStatic_{};
	std::array<PBRPipeline::Resources, kMaxPortraitSlots> resourcesLobbyPortraitStatic_{};

	std::vector<MinimapTerrainPipeline::DrawEvent> drawEventsMinimap_{};
	std::vector<MinimapPropPipeline::DrawEvent>    drawEventsMinimapProp_{};
	MinimapTerrainPipeline::Resources  resourcesMinimapTerrain_{};
	MinimapPropPipeline::Resources     resourcesMinimapProp_{};
	MinimapFogBlurPipeline::Resources  resourcesMinimapFogBlur_{};
	MinimapTerrainPipeline::CameraData cameraDataMinimap_{};
	bool minimapRebakeRequested_ = false;
	// Skybox Pipeline
	std::vector<SkyboxPipeline::DrawEvent> drawEventsSkyboxPipeline_{};
	SkyboxPipeline::Resources resourcesSkyboxPipeline_{};
	SkyboxPipeline::CameraData cameraDataSkyboxPipeline_{};
	// Tonemap resolve pass (HDR scene-color -> LDR backbuffer)
	TonemapPipeline::Resources resourcesTonemapPipeline_{};
	// Bloom pass (HDR scene-color -> bloom mip chain, composited in the resolve)
	BloomPipeline::Resources resourcesBloomPipeline_{};
	// IBL precompute params cbuffer array (7 dispatches: 1 irradiance + 5 prefilter mips + 1 BRDF)
	ConstantBufferArray iblParamsCBs_{};
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
	// Energy Orb Pipeline
	std::vector<EnergyOrbPipeline::DrawEvent> drawEventsEnergyOrbPipeline_{};
	EnergyOrbPipeline::Resources              resourcesEnergyOrbPipeline_{};
	EnergyOrbPipeline::CameraData             cameraDataEnergyOrbPipeline_{};
	EnergyOrbPipeline::FrameData              frameDataEnergyOrbPipeline_{};
	// Heat Distortion Pipeline (boss intimidation). Per-frame screen-space sources +
	// shared globals; consumed by the pre-bloom haze pass and the tonemap warp.
	std::vector<HeatDistortionShader::HeatSource> heatSources_{};
	HeatDistortionPipeline::Resources             resourcesHeatDistortion_{};
	float heatTimeSec_      = 0.0f;
	float heatWarpStrength_ = 1.0f;
	float heatGlowStrength_ = 1.0f;
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
	// Trail Pipeline (HDR / pre-bloom) — independent resources so the two trail
	// dispatchers do not clobber the same StructuredBuffer in a frame.
	std::vector<TrailPipeline::DrawEvent>    drawEventsTrailPipelineHDR_{};
	TrailPipeline::Resources                 resourcesTrailPipelineHDR_{};
	// UI Pipeline
	std::vector<UIPipeline::DrawEvent> drawEventsUIPipeline_{};
	UIPipeline::Resources resourcesUIPipeline_{};
	UIPipeline::FrameData frameDataUIPipeline_{};
	std::vector<RECT> uiClipStack_{};   // UI 스크롤 영역 클리핑 스택
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
	std::vector<PBRDeferredPipeline::OccluderInfo> occluderInfosPBRDeferred_{};

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

	// loadRequestedAssets() progress counters (see assetLoadFraction()).
	std::atomic<uint32_t> assetLoadTotal_{ 0u };
	std::atomic<uint32_t> assetLoadDone_{ 0u };

	ThreadPool* threadPool_ = nullptr;	// 설정되어있을 경우 멀티스레드로 동작한다.
	bool csmDebugVisualization_ = false;
	bool hiZCullEnabled_      = true;
	bool vsyncEnabled_        = true;   // Present(1,0) 기본. setVsync 참고
	RenderPath renderPath_    = RenderPath::Deferred;
	u32t gBufferDebugMode_    = 0u;  // 0=None, 1=Albedo, ..., 7=Depth, 8=IBL diffuse, 9=IBL specular, 10=BRDF LUT, 11~14=CSM cascade 0~3 shadow map
	float tonemapExposure_    = 1.25f;  // linear exposure multiplier applied in the tonemap resolve pass
	float bloomThreshold_     = 0.77f;  // bloom brightness threshold (HDR luminance)
	float bloomIntensity_     = 0.1f; // bloom additive strength at composite (0 = bloom off)
};

#endif	// __GFX_HPP
