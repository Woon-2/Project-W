#include "pch.hpp"
#include "gfx.hpp"
#include "iblPrecomputePipeline.hpp"
#include "errorHandling.hpp"

GFX::HiZStats GFX::getHiZStats() const {
	return {
		resourcesPBRDeferredSkinnedPipeline_.hiZPass.lastVisibleCount,
		resourcesPBRDeferredSkinnedPipeline_.hiZPass.lastTotalCount
	};
}

bool GFX::getHiZObjectVisible(u32t renderObjectId) const {
	if (!hiZCullEnabled_) return true;
	const auto& vis = resourcesPBRDeferredSkinnedPipeline_.hiZPass.objectVisibility;
	if (renderObjectId >= vis.size()) return true;
	return vis[renderObjectId];
}

void GFX::setMaxRenderObjectId(u32t maxId) {
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.objectVisibility.assign(
		static_cast<std::size_t>(maxId) + 1u, true);
}

// GFX가 소멸할 때, 제출된 모든 GPU작업이 완료되고 나서 소멸하도록 한다.
GFX::~GFX() {
	drainGpu();
	// Join the submission thread after the GPU is idle and the queue is drained.
	if (submitter_) {
		submitter_->stop();
	}
}

void GFX::drainGpu() {
	// Make sure every queued ExecuteCommandLists / Present / Signal has actually been
	// issued to the queue before we wait on the GPU fences below; otherwise the fence
	// values they target would never be reached.
	if (submitter_) {
		submitter_->flushBlocking();
	}
	for (std::size_t i = 0u; i < backBuffers_.size(); ++i) {
		waitOnFence("FrameFence"s + std::to_string(i));
	}
	waitOnFence("LoadFence");
}

// DXGI Factory를 초기화하고, DXGI Adapter들을 열거한다.
// 그리고 그 중 하나를 선택하여 curAdapter_에 저장한다.
void GFX::setupDXGI(D3D_FEATURE_LEVEL d3dFeatureLevel) {
#ifdef DXGI_DEBUG_INFO
	DXGIDebugInfo::init();
#endif

	d3dFeatureLevel_ = d3dFeatureLevel;

#ifdef DXGI_DEBUG_INFO
	DISPLAY_ERROR_DX_HR(
		CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(IDXGIFactory4), &dxgiFactory_),
		true
	);
#else
	DISPLAY_ERROR_DX_HR(
		CreateDXGIFactory2(0, __uuidof(IDXGIFactory4), &dxgiFactory_),
		true
	);
#endif


	setDXName(dxgiFactory_.Get(), "DXGIFactory");

	// 출력 인자가 nullptr인 D3D12CreateDevice의 호출은 해당 그래픽 어댑터가
	// d3d12를 지원하는지 검사할 수 있다.
	// 이를 통해 d3d12를 지원하는 그래픽 어댑터들을 adapters_에 열거하자.
	auto pAdapter = ComPtr<IDXGIAdapter1>{};
	DXGI_ADAPTER_DESC1 desc{};

	for (UINT i = 0; dxgiFactory_->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		pAdapter->GetDesc1(&desc);

		auto adapterName = std::string(std::wcslen(desc.Description) * 2, '\0');
		std::wcstombs(adapterName.data(), desc.Description, adapterName.size());

		setDXName(pAdapter.Get(), adapterName);

		if ( D3D12CreateDevice(pAdapter.Get(), d3dFeatureLevel,
			__uuidof(ID3D12Device), nullptr) >= 0
		) {
			adapters_.push_back(std::move(pAdapter));
			adapterDescs_.push_back(desc);
		}
	}

	// WARP 어댑터가 중복 감지되므로 현재 아래 코드는 사용하지 않음.

	//auto hr = dxgiFactory_->EnumWarpAdapter(__uuidof(IDXGIAdapter1), &pAdapter);
	//if (hr == S_OK) {
	//	pAdapter->GetDesc1(&desc);

	//	if ( D3D12CreateDevice(pAdapter.Get(), d3dFeatureLevel,
	//		__uuidof(ID3D12Device), nullptr) >= 0
	//	) {
	//		adapters_.push_back(std::move(pAdapter));
	//		adapterDescs_.push_back(desc);
	//	}
	//}

	std::cout << "----------[그래픽 어댑터 설정]----------\n";
	std::cout << "사용할 그래픽 어댑터를 골라주세요.\n";

	for (std::size_t i = 0u; i < adapters_.size(); ++i) {
		std::wcout << i << " - " << adapterDescs_[i].Description << '\n';
	}
	std::cout << "선택: ";
	int idx{};
	std::cout << idx << '\n';
	// std::cin >> idx;
	std::wcout << idx << " - " << adapterDescs_[idx].Description;
	std::cout << "이 선택되었습니다.\n";
	std::cout << "----------------------------------------\n";

	curAdapter_ = adapters_[idx];
}

// D3D12 Device와 Command Queue, Descriptor Heap, Descriptor Pool들을 만든다.
// 공용 샘플러들을 생성한다.
// RenderingSlave, ResourceLoading 카테고리의 Command List Pool을 초기화한다.
// Root Signature, Command Signature와 Shader(PSO)들을 만든다.
// Load Fence를 만든다.
// 그리고 DrawEvent들을 저장하기 위한 메모리를 예약한다.
void GFX::init() {
#ifdef DXGI_DEBUG_INFO
	// D3D12 디버그 계층 활성화
	ComPtr<ID3D12Debug> pDebug = nullptr;
	D3D12GetDebugInterface(__uuidof(ID3D12Debug), &pDebug);
	pDebug->EnableDebugLayer();
#endif

	// D3D12 Device 생성
	DISPLAY_ERROR_STR(curAdapter_, "[GFX Error] 활성화된 그래픽 어댑터가 없습니다. "
		"GFX::setupDXGI의 호출이 GFX::init 호출 전에 이루어졌는지 확인하세요.", true);

	DISPLAY_ERROR_DX_HR(
		D3D12CreateDevice(curAdapter_.Get(), d3dFeatureLevel_, __uuidof(ID3D12Device), &device_),
		true
	);
	setD3DName(device_.Get(), "Device");

	auto qDesc = D3D12_COMMAND_QUEUE_DESC{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0
	};

	// Command Queue 생성
	DISPLAY_ERROR_DX_HR(
		device_->CreateCommandQueue(&qDesc, __uuidof(ID3D12CommandQueue), &cmdQ_),
		true
	);
	setD3DName(cmdQ_.Get(), "CommandQueue");

	// 제출 스레드 바인딩. 렌더 루프 진입(첫 render 호출) 전까지는 inline 모드로 동작해
	// 초기화 단계의 일회성 GPU 작업(폰트/IBL/리소스 로드)이 메인 스레드에서 그대로 실행된다.
	// 첫 render()에서 goAsync()로 전용 스레드를 띄운다.
	submitter_ = std::make_unique<RenderSubmitter>();
	submitter_->start(cmdQ_.Get(), /*async=*/false);

	// RenderingSlave, ResourceLoading 카테고리의 Command List Pool 초기화
	// RenderingSlave 용량 = 한 프레임에 동시에 빌려가는 command list 총수의 상한
	// (파이프라인 dispatch마다 1개 + UI/PBR 멀티스레드 dispatch는 jobCnt개 + GFX 내부
	//  barrier/HiZ/light/copy 등). fence 신호(프레임 끝) 전까지 풀에서 빠져있으므로
	//  파이프라인 종류가 늘면 함께 늘려야 한다. 64는 mesh 파티클 효과(bloodEffect 등)가
	//  활성화되면 초과되어 뒤쪽 dispatch(UI)가 할당 실패했다 → 128로 마진 확보.
	cmdListPool_.init(device_.Get(), CommandListUsage::RenderingSlave, 128u);
	cmdListPool_.init(device_.Get(), CommandListUsage::ResourceLoading, 16u);

	// Descriptor Heap 및 Pool들 생성
	// RTV 슬롯 (room 최대 3 가정, room당): backbuffer 1 + GBuffer 4 + SceneColor 1 + Portrait 1
	//   + Bloom mip chain (최대 6) + Minimap(texA/texB) 2 = 15 → 15 × 3 rooms = 45 → 여유 포함 64.
	rtvHeap_ = DescriptorHeap( device_.Get(), D3D12_DESCRIPTOR_HEAP_DESC{
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		.NumDescriptors = 64u,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask = 0
	} );

	// RTV Pool: RTVHeap의 [0, 64) 범위
	rtvPool_ = DescriptorPool( 64u, rtvHeap_.cpuStart, rtvHeap_.gpuStart, rtvHeap_.desc.Type,
		rtvHeap_.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		device_->GetDescriptorHandleIncrementSize(rtvHeap_.desc.Type)
	);

	// DSV 슬롯: backbuffer depth 3 + CSM(cascade 4 × 3 rooms) 12 + GBuffer depth 3 + HiZ depth 3 = 21
	// + Portrait depth × 3 rooms = 24 → 여유 포함 32
	dsvHeap_ = DescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_DESC{
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		.NumDescriptors = 32u,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask = 0
	} );

	// DSV Pool: DSVHeap의 [0, 32) 범위
	dsvPool_ = DescriptorPool( 32u, dsvHeap_.cpuStart, dsvHeap_.gpuStart, dsvHeap_.desc.Type,
		dsvHeap_.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		device_->GetDescriptorHandleIncrementSize(dsvHeap_.desc.Type)
	);

	// SRV & CBV & UAV Heap은 GPU Visible, bind 필요
	// 2100(Tex2D 1800 + TexArray 100 + TexCube 100 + UAV 100) + Tex3D 16(color grading LUT 등) = 2116
	srvCbvUavHeap_ = DescriptorHeap( device_.Get(), D3D12_DESCRIPTOR_HEAP_DESC{
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.NumDescriptors = 2116u,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		.NodeMask = 0
	} );

	const auto cbvSrvUavIncSize = device_->GetDescriptorHandleIncrementSize(srvCbvUavHeap_.desc.Type);
	auto cpuStart = srvCbvUavHeap_.cpuStart;
	auto gpuStart = srvCbvUavHeap_.gpuStart;

	// SRV Texture Pool: SRVHeap의 [0, 1800) 범위
	// 텍스처 리소스 중 기본 Texture2D가 대부분이다.
	srvTexPool_ = DescriptorPool(1800u, cpuStart, gpuStart,
		srvCbvUavHeap_.desc.Type, srvCbvUavHeap_.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		cbvSrvUavIncSize
	);

	cpuStart.ptr += 1800u * cbvSrvUavIncSize;
	gpuStart.ptr += 1800u * cbvSrvUavIncSize;

	// SRV Texture Array Pool: SRVHeap의 [1800, 1900) 범위
	srvTexArrayPool_ = DescriptorPool(100u, cpuStart, gpuStart,
		srvCbvUavHeap_.desc.Type, srvCbvUavHeap_.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		cbvSrvUavIncSize
	);

	cpuStart.ptr += 100u * cbvSrvUavIncSize;
	gpuStart.ptr += 100u * cbvSrvUavIncSize;

	// SRV Texture Cube Pool: SRVHeap의 [1900, 2000) 범위
	srvTexCubePool_ = DescriptorPool(100u, cpuStart, gpuStart,
		srvCbvUavHeap_.desc.Type, srvCbvUavHeap_.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		cbvSrvUavIncSize
	);

	cpuStart.ptr += 100u * cbvSrvUavIncSize;
	gpuStart.ptr += 100u * cbvSrvUavIncSize;

	// UAV Pool: SRVHeap의 [2000, 2100) 범위
	uavPool_ = DescriptorPool(100u, cpuStart, gpuStart,
		srvCbvUavHeap_.desc.Type, srvCbvUavHeap_.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		cbvSrvUavIncSize
	);

	cpuStart.ptr += 100u * cbvSrvUavIncSize;
	gpuStart.ptr += 100u * cbvSrvUavIncSize;

	// SRV Texture3D Pool: SRVHeap의 [2100, 2116) 범위 (color grading LUT 등 volume texture)
	srvTex3DPool_ = DescriptorPool(16u, cpuStart, gpuStart,
		srvCbvUavHeap_.desc.Type, srvCbvUavHeap_.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		cbvSrvUavIncSize
	);

	cpuStart.ptr += 16u * cbvSrvUavIncSize;
	gpuStart.ptr += 16u * cbvSrvUavIncSize;

	// Sampler Heap은 GPU Visible, bind 필요
	samHeap_ = DescriptorHeap( device_.Get(), D3D12_DESCRIPTOR_HEAP_DESC{
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
		.NumDescriptors = 16u,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		.NodeMask = 0
	} );

	const auto samIncSize = device_->GetDescriptorHandleIncrementSize(samHeap_.desc.Type);
	cpuStart = samHeap_.cpuStart;
	gpuStart = samHeap_.gpuStart;

	// Sampler Pool: SAMHeap의 [0, 14) 범위
	samPool_ = DescriptorPool(14u, cpuStart, gpuStart, samHeap_.desc.Type,
		samHeap_.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		device_->GetDescriptorHandleIncrementSize(samHeap_.desc.Type)
	);

	cpuStart.ptr += 14u * samIncSize;
	gpuStart.ptr += 14u * samIncSize;

	// Comparison Sampler Pool: SAMHeap의 [14, 16) 범위
	// 그림자 구현을 위해 예약된 샘플러
	cmpSamPool_ = DescriptorPool(2u, cpuStart, gpuStart, samHeap_.desc.Type,
		samHeap_.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		device_->GetDescriptorHandleIncrementSize(samHeap_.desc.Type)
	);

	// 공용 샘플러들 생성
	createSamplers();

	// Font 초기화
	font_.init( device_.Get(), cmdQ_.Get(), 1024u, 256u, ghWnd );
	tahomaFont_ = font_.CreateFontObject( L"Tahoma", 16.0f);

	// Root Signatures & Command Signature & Shaders 생성
	auto defaultRootSig = DefaultRootSig{};
	defaultRootSig.build(device_.Get());

	cmdSig_ = std::make_shared<CmdSig>();
	cmdSig_->build(device_.Get(), defaultRootSig);

	shaders_.try_emplace("SampleShader", createSampleShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("ShadowMapShader", createShadowMapShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("ShadowMapSkinnedShader", createShadowMapSkinnedShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("ShadowMapCSMShader", createShadowMapCSMShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("ShadowMapCSMMaskedShader", createShadowMapCSMMaskedShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("ShadowMapSkinnedCSMShader", createShadowMapSkinnedCSMShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("PBRShader", createPBRShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("PBRSkinnedShader", createPBRSkinnedShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("BillboardShader", createBillboardShader( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("BillboardShaderAdditive", createBillboardShaderAdditive( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("BillboardShaderMultiply", createBillboardShaderMultiply( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("BillboardShaderPremultiplied", createBillboardShaderPremultiplied( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("MeshParticleShader", createMeshParticleShader( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("EnergyOrbShader", createEnergyOrbShader( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("WindRingShader", createWindRingShader( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("SmokeBlendCGShader", createSmokeBlendCGShader( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("BlendCGMeshShader", createBlendCGMeshShader( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("PiercingMeshShader", createPiercingMeshShader( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("PiercingSlashMeshShader", createPiercingSlashMeshShader( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("SwordSlashShader", createSwordSlashShader( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("TwoSidesShader",  createTwoSidesShader(  device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("TrailShader",          createTrailShader(         device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("TrailShaderAdditive",  createTrailShaderAdditive( device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("TrailShaderHDR",       createTrailShaderHDR(      device_.Get(), defaultRootSig.get() ));
	shaders_.try_emplace("SkyboxShader", createSkyboxShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("SkyboxShaderHDR", createSkyboxShaderHDR(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("BVShader", createBVShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace( "UIShader", createUIShader( device_.Get(), defaultRootSig.get() ) );
	shaders_.try_emplace("TerrainShader", createTerrainShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("MinimapTerrainShader", createMinimapTerrainShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("MinimapPropShader", createMinimapPropShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("TerrainShadowMapShader", createTerrainShadowMapShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("TerrainShadowMapCSMShader", createTerrainShadowMapCSMShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("PBRShaderCSMDebug", createPBRShaderCSMDebug(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("PBRSkinnedShaderCSMDebug", createPBRSkinnedShaderCSMDebug(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("TerrainShaderCSMDebug", createTerrainShaderCSMDebug(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("PBRDeferredGBufferShader",        createPBRDeferredGBufferShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("PBRDeferredIndirectGBufferShader", createPBRDeferredIndirectGBufferShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("PBRDeferredSkinnedGBufferShader", createPBRDeferredSkinnedGBufferShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("PBRDeferredSkinnedIndirectGBufferShader", createPBRDeferredSkinnedIndirectGBufferShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("PBRDeferredLightingShader",       createPBRDeferredLightingShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("TonemapResolveShader",            createTonemapResolveShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("BloomPrefilterShader",            createBloomPrefilterShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("BloomDownsampleShader",           createBloomDownsampleShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("BloomUpsampleShader",             createBloomUpsampleShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("HeatHazeShader",                  createHeatHazeShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("MinimapFogBlurShader",            createMinimapFogBlurShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("IBLIrradianceShader",             createIBLIrradianceShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("IBLPrefilterShader",              createIBLPrefilterShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("IBLBRDFLUTShader",                createIBLBRDFLUTShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("TerrainDeferredGBufferShader",    createTerrainDeferredGBufferShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("HiZOccluderShader", createHiZOccluderShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("HiZMapShader", createHiZMapShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("HiZClearShader", createHiZClearShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("HiZCullShader", createHiZCullShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("HiZCompactShader", createHiZCompactShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("HiZCommandShader", createHiZCommandShader(device_.Get(), defaultRootSig.get()));
	shaders_.try_emplace("PrefixSumShader", createPrefixSumShader(device_.Get(), defaultRootSig.get()));

	rootSigs_.try_emplace("DefaultRootSignature", std::make_shared<DefaultRootSig>(std::move(defaultRootSig)));

	// Load Fence 추가
	const auto fenceName = "LoadFence"s;
	fences_.try_emplace(fenceName, Fence{});
	DISPLAY_ERROR_DX_HR(
		device_->CreateFence(0u, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &fences_.at(fenceName).fence),
		false
	);
	setD3DName(fences_.at(fenceName).fence.Get(), fenceName);

	// Draw Event들을 저장할 메모리 예약
	drawEventsSamplePipeline_.reserve(1000u);
	drawEventsPBRPipeline_.reserve(1000u);
	drawEventsPBRSkinnedPipeline_.reserve(1000u);
	drawEventsBVPipeline_.reserve(1000u);
	drawEventsBillboardPipeline_.reserve(1000u);
	drawEventsMeshParticlePipeline_.reserve(256u);
	drawEventsEnergyOrbPipeline_.reserve(EnergyOrbPipeline::kMaxOrbDrawcalls);
	drawEventsSmokeBlendCGPipeline_.reserve(256u);
	drawEventsBlendCGMeshPipeline_.reserve(256u);
	drawEventsPiercingMeshPipeline_.reserve(256u);
	drawEventsPiercingSlashMeshPipeline_.reserve(256u);
	drawEventsSwordSlashPipeline_.reserve(256u);
	drawEventsTwoSidesPipeline_.reserve(256u);
	drawEventsTrailPipeline_.reserve(TrailPipeline::kMaxDrawEvents);
	drawEventsTrailPipelineHDR_.reserve(128u);
	drawEventsSkyboxPipeline_.reserve(10u);
	drawEventsTerrainPipeline_.reserve(4u);
	drawEventsTerrainDeferredPipeline_.reserve(4u);
	drawEventsPBRDeferredPipeline_.reserve(1000u);
	drawEventsPBRDeferredSkinnedPipeline_.reserve(1000u);
}

// 윈도우와 연결된 SwapChain을 만든다.
// Back Buffer 개수 만큼의 room을 가지는 Constant Buffer들을 만든다.
// RenderingMaster 카테고리의 Command List Pool을
// Back Buffer 개수 * 2의 크기를 갖도록 초기화한다.
// 그리고 Back Buffer 개수 만큼의 Frame Fence들을 만든다.
// Scaling, BufferCount는 후에 매개변수로 전달받도록 하자.
void GFX::createSwapChain() {
	// 스왑체인 생성
	scd_ = DXGI_SWAP_CHAIN_DESC1{
		.Width = static_cast<UINT>(gClientRect.right - gClientRect.left),
		.Height = static_cast<UINT>(gClientRect.bottom - gClientRect.top),
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.Stereo = false,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = static_cast<UINT>(3),
		.Scaling = DXGI_SCALING_STRETCH,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
		.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	};

	scfd_ = DXGI_SWAP_CHAIN_FULLSCREEN_DESC{
		.RefreshRate = DXGI_RATIONAL{ .Numerator = 144, .Denominator = 1 },
		.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
		.Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
		.Windowed = true
	};

	ComPtr<IDXGISwapChain1> tmp = nullptr;

	DISPLAY_ERROR_DX_HR(
		dxgiFactory_->CreateSwapChainForHwnd(cmdQ_.Get(), ghWnd, &scd_, &scfd_, nullptr, &tmp),
		true
	);
	tmp.As(&swapChain_);
	DISPLAY_ERROR_DX_HR(dxgiFactory_->MakeWindowAssociation(ghWnd, DXGI_MWA_NO_ALT_ENTER), false);

	setDXName(swapChain_.Get(), "SwapChain");

	// 백버퍼 및 깊이버퍼 리소스와 뷰 생성
	backBuffers_.resize(3u);

	// 백버퍼는 스왑체인에서 이미 만들었으므로,
	// swapChain_->GetBuffer를 통해 가져와 디스크립터만 만든다.
	for (int i = 0; i < 3; ++i) {
		DISPLAY_ERROR_DX_HR(
			swapChain_->GetBuffer(i, __uuidof(ID3D12Resource), &backBuffers_[i]),
			true
		);
		setD3DName(backBuffers_[i].Get(), "BackBuffer"s + std::to_string(i));
	}

	for (int i = 0; i < 3; ++i) {
		auto idx = allocatedRtvIndices_.emplace_back(rtvPool_.alloc());
		auto cpuHandle = backBufferRtvs_.emplace_back(rtvPool_.cpuHandle(idx));
		DISPLAY_ERROR_DX_VOID(
			device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, cpuHandle),
			true
		);
	}

	// 깊이 버퍼는 우리가 따로 만들어주어야 한다.
	for (int i = 0; i < 3; ++i) {
		auto depthBuffer = createDepthBuffer( device_.Get(), DXGI_FORMAT_D32_FLOAT,
			DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u }
		);
		setD3DName(depthBuffer.Get(), "DepthBuffer"s + std::to_string(i));
		depthBuffers_.push_back(std::move(depthBuffer));
	}

	for (int i = 0; i < 3; ++i) {
		auto idx = allocatedDsvIndices_.emplace_back(dsvPool_.alloc());
		auto cpuHandle = depthBufferDsvs_.emplace_back(dsvPool_.cpuHandle(idx));
		DISPLAY_ERROR_DX_VOID(
			device_->CreateDepthStencilView(depthBuffers_[i].Get(), nullptr, cpuHandle),
			true
		);
	}

	// RenderingMaster 카테고리의 Command List Pool을
	// Back Buffer 개수 * 2의 크기를 갖도록 초기화한다.
	// RenderingMaster 카테고리의 명령 컨텍스트는 한 프레임의 렌더링에
	// 클리어용, 출력용으로 두 개가 쓰인다.
	cmdListPool_.init(device_.Get(), CommandListUsage::RenderingMaster, (backBuffers_.size()) * 2);

	// 백버퍼 개수만큼의 room을 가지는 파이프라인별 리소스들 생성
	// 1000u, 32u를 변수로 대체하기
	// Sample Pipeline ----
	resourcesSamplePipeline_.perInstanceData.init(
		device_.Get(), sizeof(SampleShader::PerInstanceData) * 1000u, backBuffers_.size(), "Sample_PerInstanceData"
	);
	resourcesSamplePipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(SampleShader::PerDrawcallData), 1000u, backBuffers_.size(), "Sample_PerDrawcallData"
	);
	// PBR Pipeline ----
	resourcesPBRPipeline_.shadowPass.perInstanceData.init(
		device_.Get(), sizeof(ShadowMapShader::PerInstanceData) * 16'384u, backBuffers_.size(), "PBR_Shadow_PerInstanceData"
	);
	resourcesPBRPipeline_.shadowPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(ShadowMapShader::PerDrawcallData), 1000u, backBuffers_.size(), "PBR_Shadow_PerDrawcallData"
	);
	resourcesPBRPipeline_.shadowPass.perFrameData = createConstantBufferArray(
		device_.Get(), sizeof(ShadowMapCSMShader::PerFrameData), MAX_CSM_CASCADES, backBuffers_.size(), "PBR_Shadow_PerFrameData"
	);
	resourcesPBRPipeline_.mainPass.perInstanceData.init(
		device_.Get(), sizeof(PBRShader::PerInstanceData) * 16'384u, backBuffers_.size(), "PBR_Main_PerInstanceData"
	);
	resourcesPBRPipeline_.mainPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(PBRShader::PerDrawcallData), 1000u, backBuffers_.size(), "PBR_Main_PerDrawcallData"
	);
	resourcesPBRPipeline_.mainPass.lightData.init(
		device_.Get(), sizeof(PBRShader::Light) * 32u, backBuffers_.size(), "PBR_Main_LightData"
	);
	resourcesPBRPipeline_.mainPass.perFrameData.init(
		device_.Get(), sizeof(PBRShader::PerFrameData), backBuffers_.size(), "PBR_Main_PerFrameData"
	);
	// PBR-skinned Pipeline ----
	resourcesPBRSkinnedPipeline_.shadowPass.perInstanceData.init(
		device_.Get(), sizeof(ShadowMapSkinnedShader::PerInstanceData) * 10000u, backBuffers_.size(), "PBRSkinned_Shadow_PerInstanceData"
	);
	resourcesPBRSkinnedPipeline_.shadowPass.boneData.init(
		device_.Get(), sizeof(ShadowMapSkinnedShader::BoneData) * 800'000u, backBuffers_.size(), "PBRSkinned_Shadow_BoneData"
	);
	resourcesPBRSkinnedPipeline_.shadowPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(ShadowMapSkinnedShader::PerDrawcallData), 1000u, backBuffers_.size(), "PBRSkinned_Shadow_PerDrawcallData"
	);
	resourcesPBRSkinnedPipeline_.shadowPass.perFrameData = createConstantBufferArray(
		device_.Get(), sizeof(ShadowMapSkinnedCSMShader::PerFrameData), MAX_CSM_CASCADES, backBuffers_.size(), "PBRSkinned_Shadow_PerFrameData"
	);
	resourcesPBRSkinnedPipeline_.mainPass.perInstanceData.init(
		device_.Get(), sizeof(PBRSkinnedShader::PerInstanceData) * 10000u, backBuffers_.size(), "PBRSkinned_Main_PerInstanceData"
	);
	resourcesPBRSkinnedPipeline_.mainPass.boneData.init(
		device_.Get(), sizeof(PBRSkinnedShader::BoneData) * 800'000u, backBuffers_.size(), "PBRSkinned_Main_BoneData"
	);
	resourcesPBRSkinnedPipeline_.mainPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(PBRSkinnedShader::PerDrawcallData), 1000u, backBuffers_.size(), "PBRSkinned_Main_PerDrawcallData"
	);
	resourcesPBRSkinnedPipeline_.mainPass.lightData.init(
		device_.Get(), sizeof(PBRSkinnedShader::Light) * 32u, backBuffers_.size(), "PBRSkinned_Main_LightData"
	);
	resourcesPBRSkinnedPipeline_.mainPass.perFrameData.init(
		device_.Get(), sizeof(PBRSkinnedShader::PerFrameData), backBuffers_.size(), "PBRSkinned_Main_PerFrameData"
	);
	// Skybox Pipeline ----
	resourcesSkyboxPipeline_.perFrameData.init(
		device_.Get(), sizeof(SkyboxShader::PerFrameData), backBuffers_.size(), "Skybox_PerFrameData"
	);
	resourcesSkyboxPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(SkyboxShader::PerDrawcallData), 32u, backBuffers_.size(), "Skybox_PerDrawcallData"
	);
	// Tonemap resolve pass ----
	resourcesTonemapPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(TonemapResolveShader::PerDrawcallData), 1u, backBuffers_.size(), "Tonemap_PerDrawcallData"
	);
	// Heat-distortion haze pass ----
	resourcesHeatDistortion_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(HeatDistortionShader::PerDrawcallData), 1u, backBuffers_.size(), "HeatDistortion_PerDrawcallData"
	);
	// Bloom pass ---- one cbuffer element per bloom pass (prefilter + down/upsample chain)
	resourcesBloomPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(BloomShader::PerDrawcallData), BloomPipeline::kMaxBloomPasses, backBuffers_.size(), "Bloom_PerDrawcallData"
	);
	// IBL precompute params (7 dispatches, room count 1 — load-time static)
	iblParamsCBs_ = createConstantBufferArray(
		device_.Get(), sizeof(IBLShader::IBLParams), 7u, 1u, "IBL_Params"
	);
	// Bounding Volume Pipeline ----
	resourcesBVPipeline_.perInstanceData.init(
		device_.Get(), sizeof(BVShader::PerInstanceData) * 120'000u, backBuffers_.size(), "BV_PerInstanceData"
	);
	resourcesBVPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(BVShader::PerDrawcallData), 120'000u, backBuffers_.size(), "BV_PerDrawcallData"
	);
	// Billboard Pipeline ----
	resourcesBillboardPipeline_.perInstanceData.init(
		device_.Get(), sizeof( BillboardShader::PerInstanceData ) * 4096u, backBuffers_.size(), "Billboard_PerInstanceData"
	);
	resourcesBillboardPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( BillboardShader::PerDrawcallData ), 4096u, backBuffers_.size(), "Billboard_PerDrawcallData"
	);
	resourcesBillboardPipeline_.perFrameData.init(
		device_.Get(), sizeof( BillboardShader::PerFrameData ), backBuffers_.size(), "Billboard_PerFrameData"
	);
	// Mesh Particle Pipeline ----
	resourcesMeshParticlePipeline_.perInstanceData.init(
		device_.Get(), sizeof( MeshParticleShader::PerInstanceData ) * 256u, backBuffers_.size(), "MeshParticle_PerInstanceData"
	);
	resourcesMeshParticlePipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( MeshParticleShader::PerDrawcallData ), 256u, backBuffers_.size(), "MeshParticle_PerDrawcallData"
	);
	resourcesMeshParticlePipeline_.perFrameData.init(
		device_.Get(), sizeof( MeshParticleShader::PerFrameData ), backBuffers_.size(), "MeshParticle_PerFrameData"
	);

	resourcesEnergyOrbPipeline_.perInstanceData.init(
		device_.Get(), sizeof( EnergyOrbShader::PerInstanceData ) * EnergyOrbPipeline::kMaxOrbDrawcalls, backBuffers_.size(), "EnergyOrb_PerInstanceData"
	);
	resourcesEnergyOrbPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( EnergyOrbShader::PerDrawcallData ), EnergyOrbPipeline::kMaxOrbDrawcalls, backBuffers_.size(), "EnergyOrb_PerDrawcallData"
	);
	resourcesEnergyOrbPipeline_.perFrameData.init(
		device_.Get(), sizeof( EnergyOrbShader::PerFrameData ), backBuffers_.size(), "EnergyOrb_PerFrameData"
	);
	resourcesEnergyOrbPipeline_.boneData.init(
		device_.Get(), sizeof( EnergyOrbShader::BoneData ) * 64'000u, backBuffers_.size(), "EnergyOrb_BoneData"
	);
	// Wind Ring Pipeline ----
	resourcesWindRingPipeline_.perInstanceData.init(
		device_.Get(), sizeof( WindRingShader::PerInstanceData ) * 256u, backBuffers_.size(), "WindRing_PerInstanceData"
	);
	resourcesWindRingPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( WindRingShader::PerDrawcallData ), 256u, backBuffers_.size(), "WindRing_PerDrawcallData"
	);
	resourcesWindRingPipeline_.perFrameData.init(
		device_.Get(), sizeof( WindRingShader::PerFrameData ), backBuffers_.size(), "WindRing_PerFrameData"
	);
	// Smoke Blend CG Pipeline ----
	resourcesSmokeBlendCGPipeline_.perInstanceData.init(
		device_.Get(), sizeof( SmokeBlendCGShader::PerInstanceData ) * 256u, backBuffers_.size(), "SmokeBlendCG_PerInstanceData"
	);
	resourcesSmokeBlendCGPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( SmokeBlendCGShader::PerDrawcallData ), 256u, backBuffers_.size(), "SmokeBlendCG_PerDrawcallData"
	);
	resourcesSmokeBlendCGPipeline_.perFrameData.init(
		device_.Get(), sizeof( SmokeBlendCGShader::PerFrameData ), backBuffers_.size(), "SmokeBlendCG_PerFrameData"
	);
	// Blend CG Mesh Pipeline ----
	resourcesBlendCGMeshPipeline_.perInstanceData.init(
		device_.Get(), sizeof( BlendCGMeshShader::PerInstanceData ) * 256u, backBuffers_.size(), "BlendCGMesh_PerInstanceData"
	);
	resourcesBlendCGMeshPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( BlendCGMeshShader::PerDrawcallData ), 256u, backBuffers_.size(), "BlendCGMesh_PerDrawcallData"
	);
	resourcesBlendCGMeshPipeline_.perFrameData.init(
		device_.Get(), sizeof( BlendCGMeshShader::PerFrameData ), backBuffers_.size(), "BlendCGMesh_PerFrameData"
	);
	// Piercing Mesh Pipeline ----
	resourcesPiercingMeshPipeline_.perInstanceData.init(
		device_.Get(), sizeof( PiercingMeshShader::PerInstanceData ) * 256u, backBuffers_.size(), "PiercingMesh_PerInstanceData"
	);
	resourcesPiercingMeshPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( PiercingMeshShader::PerDrawcallData ), 256u, backBuffers_.size(), "PiercingMesh_PerDrawcallData"
	);
	resourcesPiercingMeshPipeline_.perFrameData.init(
		device_.Get(), sizeof( PiercingMeshShader::PerFrameData ), backBuffers_.size(), "PiercingMesh_PerFrameData"
	);
	// Piercing Slash Mesh Pipeline ----
	resourcesPiercingSlashMeshPipeline_.perInstanceData.init(
		device_.Get(), sizeof( PiercingSlashMeshShader::PerInstanceData ) * 256u, backBuffers_.size(), "PiercingSlashMesh_PerInstanceData"
	);
	resourcesPiercingSlashMeshPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( PiercingSlashMeshShader::PerDrawcallData ), 256u, backBuffers_.size(), "PiercingSlashMesh_PerDrawcallData"
	);
	resourcesPiercingSlashMeshPipeline_.perFrameData.init(
		device_.Get(), sizeof( PiercingSlashMeshShader::PerFrameData ), backBuffers_.size(), "PiercingSlashMesh_PerFrameData"
	);
	// Sword Slash Pipeline ----
	resourcesSwordSlashPipeline_.perInstanceData.init(
		device_.Get(), sizeof( SwordSlashShader::PerInstanceData ) * 256u, backBuffers_.size(), "SwordSlash_PerInstanceData"
	);
	resourcesSwordSlashPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( SwordSlashShader::PerDrawcallData ), 256u, backBuffers_.size(), "SwordSlash_PerDrawcallData"
	);
	resourcesSwordSlashPipeline_.perFrameData.init(
		device_.Get(), sizeof( SwordSlashShader::PerFrameData ), backBuffers_.size(), "SwordSlash_PerFrameData"
	);
	// Two Sides Pipeline ----
	resourcesTwoSidesPipeline_.perInstanceData.init(
		device_.Get(), sizeof( TwoSidesShader::PerInstanceData ) * 256u, backBuffers_.size(), "TwoSides_PerInstanceData"
	);
	resourcesTwoSidesPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( TwoSidesShader::PerDrawcallData ), 256u, backBuffers_.size(), "TwoSides_PerDrawcallData"
	);
	resourcesTwoSidesPipeline_.perFrameData.init(
		device_.Get(), sizeof( TwoSidesShader::PerFrameData ), backBuffers_.size(), "TwoSides_PerFrameData"
	);
	// Trail Pipeline ----
	// System-wide trail vertex pool. Sized for "kMaxParticles * kMaxTrailSegments"
	// in the worst case (4096 * 32 = 131072 vertices ~= 4MB per back-buffer copy).
	resourcesTrailPipeline_.perInstanceData.init(
		device_.Get(), sizeof( TrailShader::PerInstanceData ) * TrailPipeline::kMaxTrailVertices, backBuffers_.size(), "Trail_PerInstanceData"
	);
	resourcesTrailPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( TrailShader::PerDrawcallData ), TrailPipeline::kMaxDrawEvents, backBuffers_.size(), "Trail_PerDrawcallData"
	);
	resourcesTrailPipeline_.perFrameData.init(
		device_.Get(), sizeof( TrailShader::PerFrameData ), backBuffers_.size(), "Trail_PerFrameData"
	);
	// Trail Pipeline (HDR / pre-bloom) ----
	// Smaller pool: only path-guidance ribbons use this channel (a few windowed
	// segments per frame). The dispatcher caps draw events at perDrawcallData.size().
	constexpr u32t kHdrTrailMaxDrawcalls = 128u;
	constexpr u32t kHdrTrailMaxVertices  = kHdrTrailMaxDrawcalls * static_cast<u32t>( TrailPipeline::kMaxVerticesPerTrail );
	resourcesTrailPipelineHDR_.perInstanceData.init(
		device_.Get(), sizeof( TrailShader::PerInstanceData ) * kHdrTrailMaxVertices, backBuffers_.size(), "TrailHDR_PerInstanceData"
	);
	resourcesTrailPipelineHDR_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( TrailShader::PerDrawcallData ), kHdrTrailMaxDrawcalls, backBuffers_.size(), "TrailHDR_PerDrawcallData"
	);
	resourcesTrailPipelineHDR_.perFrameData.init(
		device_.Get(), sizeof( TrailShader::PerFrameData ), backBuffers_.size(), "TrailHDR_PerFrameData"
	);
	// UI Pipeline ----
	resourcesUIPipeline_.perInstanceData.init(
		device_.Get(), sizeof( UIShader::PerInstanceData ) * 50'000u, backBuffers_.size(), "UI_PerInstanceData"
	);
	resourcesUIPipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof( UIShader::PerDrawcallData ), 50'000u, backBuffers_.size(), "UI_PerDrawcallData"
	);
	resourcesUIPipeline_.perFrameData.init(
		device_.Get(), sizeof( UIShader::PerFrameData ), backBuffers_.size(), "UI_PerFrameData"
	);
	// Terrain Pipeline ----
	resourcesTerrainPipeline_.shadowPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(TerrainShadowMapShader::PerDrawcallData), 1000u, backBuffers_.size(), "Terrain_Shadow_PerDrawcallData"
	);
	resourcesTerrainPipeline_.shadowPass.perFrameData = createConstantBufferArray(
		device_.Get(), sizeof(TerrainShadowMapCSMShader::PerFrameData), MAX_CSM_CASCADES, backBuffers_.size(), "Terrain_Shadow_PerFrameData"
	);
	resourcesTerrainPipeline_.mainPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(TerrainShader::PerDrawcallData), 1000u, backBuffers_.size(), "Terrain_Main_PerDrawcallData"
	);
	resourcesTerrainPipeline_.mainPass.perFrameData.init(
		device_.Get(), sizeof(TerrainShader::PerFrameData), backBuffers_.size(), "Terrain_Main_PerFrameData"
	);
	resourcesTerrainPipeline_.mainPass.lightData.init(
		device_.Get(), sizeof(PBRShader::Light) * 32u, backBuffers_.size(), "Terrain_Main_LightData"
	);
	// Terrain Deferred Pipeline ----
	resourcesTerrainDeferredPipeline_.occluderPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(HiZOccluderShader::PerDrawcallData), 1000u, backBuffers_.size(), "TerrainDeferred_Occluder_PerDrawcallData"
	);
	resourcesTerrainDeferredPipeline_.occluderPass.perInstanceData.init(
		device_.Get(), sizeof(HiZOccluderShader::PerInstanceData) * 1000u, backBuffers_.size(), "TerrainDeferred_Occluder_PerInstanceData"
	);
	resourcesTerrainDeferredPipeline_.shadowPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(TerrainShadowMapShader::PerDrawcallData), 1000u, backBuffers_.size(), "TerrainDeferred_Shadow_PerDrawcallData"
	);
	resourcesTerrainDeferredPipeline_.shadowPass.perFrameData = createConstantBufferArray(
		device_.Get(), sizeof(TerrainShadowMapCSMShader::PerFrameData), MAX_CSM_CASCADES, backBuffers_.size(), "TerrainDeferred_Shadow_PerFrameData"
	);
	resourcesTerrainDeferredPipeline_.gBufferPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(TerrainShader::PerDrawcallData), 1000u, backBuffers_.size(), "TerrainDeferred_GBuffer_PerDrawcallData"
	);
	resourcesTerrainDeferredPipeline_.gBufferPass.perFrameData.init(
		device_.Get(), sizeof(TerrainDeferredGBufferShader::PerFrameData), backBuffers_.size(), "TerrainDeferred_GBuffer_PerFrameData"
	);
	// PBR Deferred Pipeline ----
	resourcesPBRDeferredPipeline_.shadowPass.perInstanceData.init(
		device_.Get(), sizeof(ShadowMapCSMShader::PerInstanceData) * 120'000u, backBuffers_.size(), "PBRDeferred_Shadow_PerInstanceData"
	);
	resourcesPBRDeferredPipeline_.shadowPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(ShadowMapCSMShader::PerDrawcallData), 10'000u, backBuffers_.size(), "PBRDeferred_Shadow_PerDrawcallData"
	);
	resourcesPBRDeferredPipeline_.shadowPass.perDrawcallDataMasked = createConstantBufferArray(
		device_.Get(), sizeof(ShadowMapCSMMaskedShader::PerDrawcallData), 10'000u, backBuffers_.size(), "PBRDeferred_Shadow_PerDrawcallDataMasked"
	);
	resourcesPBRDeferredPipeline_.shadowPass.perFrameData = createConstantBufferArray(
		device_.Get(), sizeof(ShadowMapCSMShader::PerFrameData), MAX_CSM_CASCADES, backBuffers_.size(), "PBRDeferred_Shadow_PerFrameData"
	);
	resourcesPBRDeferredPipeline_.gBufferPass.perInstanceData.init(
		device_.Get(), sizeof(PBRDeferredGBufferShader::PerInstanceData) * 120'000u, backBuffers_.size(), "PBRDeferred_GBuffer_PerInstanceData"
	);
	resourcesPBRDeferredPipeline_.gBufferPass.lightData.init(
		device_.Get(), sizeof(PBRDeferredGBufferShader::Light) * 32u, backBuffers_.size(), "PBRDeferred_GBuffer_LightData"
	);
	resourcesPBRDeferredPipeline_.gBufferPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(PBRDeferredGBufferShader::PerDrawcallData), 10'000u, backBuffers_.size(), "PBRDeferred_GBuffer_PerDrawcallData"
	);
	resourcesPBRDeferredPipeline_.gBufferPass.perFrameData.init(
		device_.Get(), sizeof(PBRDeferredGBufferShader::PerFrameData), backBuffers_.size(), "PBRDeferred_GBuffer_PerFrameData"
	);
	// PBR Deferred Pipeline — Hi-Z occluder pass (near-camera collidable props) ----
	{
		constexpr auto kMaxStaticHiZ = PBRDeferredPipeline::Resources::HiZPass::MAX_HIZ_INSTANCES;
		resourcesPBRDeferredPipeline_.occluderPass.perDrawcallData = createConstantBufferArray(
			device_.Get(), sizeof(HiZOccluderShader::PerDrawcallData), 1000u, backBuffers_.size(), "PBRDeferred_Occluder_PerDrawcallData"
		);
		resourcesPBRDeferredPipeline_.occluderPass.perInstanceData.init(
			device_.Get(), sizeof(HiZOccluderShader::PerInstanceData) * kMaxStaticHiZ, backBuffers_.size(), "PBRDeferred_Occluder_PerInstanceData"
		);
		// PBR Deferred Pipeline — Hi-Z occlusion cull + indirect draw (BVH props) ----
		resourcesPBRDeferredPipeline_.hiZPass.groupOffsets.init(
			device_.Get(), sizeof(u32t) * 10'000u, backBuffers_.size(), "PBRDeferred_HiZ_GroupOffset"
		);
		resourcesPBRDeferredPipeline_.hiZPass.indirectCmd.init(
			device_.Get(), sizeof(HiZCommandShader::IndirectCommand) * 10'000u, backBuffers_.size(), "PBRDeferred_HiZ_IndirectCommand"
		);
		resourcesPBRDeferredPipeline_.hiZPass.perFrameDataClear.init(
			device_.Get(), sizeof(HiZClearShader::PerFrameData), backBuffers_.size(), "PBRDeferred_HiZ_PerFrameDataClear"
		);
		resourcesPBRDeferredPipeline_.hiZPass.perFrameDataCommand.init(
			device_.Get(), sizeof(HiZCommandShader::PerFrameData), backBuffers_.size(), "PBRDeferred_HiZ_PerFrameDataCommand"
		);
		resourcesPBRDeferredPipeline_.hiZPass.perFrameDataCompact.init(
			device_.Get(), sizeof(HiZCompactShader::PerFrameData), backBuffers_.size(), "PBRDeferred_HiZ_PerFrameDataCompact"
		);
		resourcesPBRDeferredPipeline_.hiZPass.perFrameDataCull.init(
			device_.Get(), sizeof(HiZCullShader::PerFrameData), backBuffers_.size(), "PBRDeferred_HiZ_PerFrameDataCull"
		);
		resourcesPBRDeferredPipeline_.hiZPass.perGroupCnt.init(
			device_.Get(), sizeof(u32t) * 10'000u, backBuffers_.size(), "PBRDeferred_HiZ_PerGroupCnt"
		);
		resourcesPBRDeferredPipeline_.hiZPass.perGroupData.init(
			device_.Get(), sizeof(HiZCompactShader::PerGroupData) * 10'000u, backBuffers_.size(), "PBRDeferred_HiZ_PerGroupData"
		);
		resourcesPBRDeferredPipeline_.hiZPass.perInstanceDataCompact.init(
			device_.Get(), sizeof(HiZCompactShader::PerInstanceData) * kMaxStaticHiZ, backBuffers_.size(), "PBRDeferred_HiZ_PerInstanceDataCompact"
		);
		resourcesPBRDeferredPipeline_.hiZPass.perInstanceDataCull.init(
			device_.Get(), sizeof(HiZCullShader::PerInstanceData) * kMaxStaticHiZ, backBuffers_.size(), "PBRDeferred_HiZ_PerInstanceDataCull"
		);
		resourcesPBRDeferredPipeline_.hiZPass.visibleFlags.init(
			device_.Get(), sizeof(u32t) * kMaxStaticHiZ, backBuffers_.size(), "PBRDeferred_HiZ_VisibleFlags"
		);
		resourcesPBRDeferredPipeline_.hiZPass.visibleIndices.init(
			device_.Get(), sizeof(u32t) * kMaxStaticHiZ, backBuffers_.size(), "PBRDeferred_HiZ_VisibleIndices"
		);
		resourcesPBRDeferredPipeline_.hiZPass.cullScratch.init(
			device_.Get(), sizeof(u32t) * kMaxStaticHiZ, backBuffers_.size(), "PBRDeferred_HiZ_CullScratch"
		);
		resourcesPBRDeferredPipeline_.hiZPass.gBufferPerInstanceData.init(
			device_.Get(), sizeof(PBRDeferredGBufferShader::PerInstanceData) * kMaxStaticHiZ, backBuffers_.size(), "PBRDeferred_HiZ_GBufferPerInstanceData"
		);
		resourcesPBRDeferredPipeline_.hiZPass.gBufferPerDrawcallData = createConstantBufferArray(
			device_.Get(), sizeof(PBRDeferredGBufferShader::PerDrawcallData), 1000u, backBuffers_.size(), "PBRDeferred_HiZ_GBufferPerDrawcallData"
		);
	}
	// PBR Deferred Skinned Pipeline ----
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.groupOffsets.init(
		device_.Get(), sizeof(u32t) * 4000u, backBuffers_.size(), "PBRDeferredSkinned_HiZ_GroupOffset"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.indirectCmd.init(
		device_.Get(), sizeof(HiZCommandShader::IndirectCommand) * 4000u, backBuffers_.size(), "PBRDeferredSkinned_HiZ_IndirectCommand"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.perFrameDataClear.init(
		device_.Get(), sizeof(HiZClearShader::PerFrameData), backBuffers_.size(), "PBRDeferredSkinned_HiZ_PerFrameDataClear"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.perFrameDataCommand.init(
		device_.Get(), sizeof(HiZCommandShader::PerFrameData), backBuffers_.size(), "PBRDeferredSkinned_HiZ_PerFrameDataCommand"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.perFrameDataCompact.init(
		device_.Get(), sizeof(HiZCompactShader::PerFrameData), backBuffers_.size(), "PBRDeferredSkinned_HiZ_PerFrameDataCompact"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.perFrameDataCull.init(
		device_.Get(), sizeof(HiZCullShader::PerFrameData), backBuffers_.size(), "PBRDeferredSkinned_HiZ_PerFrameDataCull"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.perGroupCnt.init(
		device_.Get(), sizeof(u32t) * 4000u, backBuffers_.size(), "PBRDeferredSkinned_HiZ_PerGroupCnt"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.perGroupData.init(
		device_.Get(), sizeof(HiZCompactShader::PerGroupData) * 4000u, backBuffers_.size(), "PBRDeferredSkinned_HiZ_PerGroupData"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.perInstanceDataCompact.init(
		device_.Get(), sizeof(HiZCompactShader::PerInstanceData) * 200'000u, backBuffers_.size(), "PBRDeferredSkinned_HiZ_PerInstanceDataCompact"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.perInstanceDataCull.init(
		device_.Get(), sizeof(HiZCullShader::PerInstanceData) * 200'000u, backBuffers_.size(), "PBRDeferredSkinned_HiZ_PerInstanceDataCull"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.visibleFlags.init(
		device_.Get(), sizeof(u32t) * 200'000u, backBuffers_.size(), "PBRDeferredSkinned_HiZ_VisibleFlags"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.visibleIndices.init(
		device_.Get(), sizeof(u32t) * 200'000u, backBuffers_.size(), "PBRDeferredSkinned_HiZ_VisibleIndices"
	);
	// visibility feedback: 단일 리소스 2-slot ring (roomCnt=1, byteWidth = 2 * slotBytes).
	// DEFAULT(UAV) 면과 READBACK 면 모두 단일 리소스로 2슬롯을 byte offset으로 표현한다.
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.visibilityFeedback.init(
		device_.Get(),
		2u * static_cast<u64t>(PBRDeferredSkinnedPipeline::Resources::HiZPass::MAX_HIZ_INSTANCES) * sizeof(u32t),
		1u, "PBRDeferredSkinned_HiZ_VisibilityFeedback"
	);
	resourcesPBRDeferredSkinnedPipeline_.hiZPass.visibilityFeedback.initReadback(
		device_.Get(),
		2u * static_cast<u64t>(PBRDeferredSkinnedPipeline::Resources::HiZPass::MAX_HIZ_INSTANCES) * sizeof(u32t)
	);
	resourcesPBRDeferredSkinnedPipeline_.shadowPass.perInstanceData.init(
		device_.Get(), sizeof(ShadowMapSkinnedCSMShader::PerInstanceData) * 10'000u, backBuffers_.size(), "PBRDeferredSkinned_Shadow_PerInstanceData"
	);
	resourcesPBRDeferredSkinnedPipeline_.shadowPass.boneData.init(
		device_.Get(), sizeof(ShadowMapSkinnedCSMShader::BoneData) * 400'000u, backBuffers_.size(), "PBRDeferredSkinned_Shadow_BoneData"
	);
	resourcesPBRDeferredSkinnedPipeline_.shadowPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(ShadowMapSkinnedCSMShader::PerDrawcallData), 1000u, backBuffers_.size(), "PBRDeferredSkinned_Shadow_PerDrawcallData"
	);
	resourcesPBRDeferredSkinnedPipeline_.shadowPass.perFrameData = createConstantBufferArray(
		device_.Get(), sizeof(ShadowMapSkinnedCSMShader::PerFrameData), MAX_CSM_CASCADES, backBuffers_.size(), "PBRDeferredSkinned_Shadow_PerFrameData"
	);
	resourcesPBRDeferredSkinnedPipeline_.gBufferPass.perInstanceData.init(
		device_.Get(), sizeof(PBRDeferredSkinnedGBufferShader::PerInstanceData) * 10'000u, backBuffers_.size(), "PBRDeferredSkinned_GBuffer_PerInstanceData"
	);
	resourcesPBRDeferredSkinnedPipeline_.gBufferPass.lightData.init(
		device_.Get(), sizeof(PBRDeferredSkinnedGBufferShader::Light) * 32u, backBuffers_.size(), "PBRDeferredSkinned_GBuffer_LightData"
	);
	resourcesPBRDeferredSkinnedPipeline_.gBufferPass.boneData.init(
		device_.Get(), sizeof(PBRDeferredSkinnedGBufferShader::BoneData) * 400'000u, backBuffers_.size(), "PBRDeferredSkinned_GBuffer_BoneData"
	);
	resourcesPBRDeferredSkinnedPipeline_.gBufferPass.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(PBRDeferredSkinnedGBufferShader::PerDrawcallData), 1000u, backBuffers_.size(), "PBRDeferredSkinned_GBuffer_PerDrawcallData"
	);
	resourcesPBRDeferredSkinnedPipeline_.gBufferPass.perFrameData.init(
		device_.Get(), sizeof(PBRDeferredSkinnedGBufferShader::PerFrameData), backBuffers_.size(), "PBRDeferredSkinned_GBuffer_PerFrameData"
	);
	// Deferred Lighting Pass ----
	deferredLightingLightData_.init(
		device_.Get(), sizeof(PBRShader::Light) * 32u, backBuffers_.size(), "DeferredLighting_LightData"
	);
	deferredLightingPerFrameData_.init(
		device_.Get(), sizeof(PBRDeferredLightingShader::PerFrameData), backBuffers_.size(), "DeferredLighting_PerFrameData"
	);

	// 프레임 펜스 생성
	// i번째 프레임을 렌더링한 후 i-(백버퍼 수 - 1)번째 프레임의 펜스를 기다리도록 한다.
	for (std::size_t i = 0u; i < backBuffers_.size(); ++i) {
		const auto fenceName = "FrameFence"s + std::to_string(i);
		fences_.try_emplace(fenceName, Fence{});
		DISPLAY_ERROR_DX_HR(
			device_->CreateFence(0u, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &fences_.at(fenceName).fence),
			false
		);
		setD3DName(fences_.at(fenceName).fence.Get(), fenceName);
	}
}

// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent(const SamplePipeline::DrawEvent& drawEvent) {
	drawEventsSamplePipeline_.push_back(drawEvent);
}

// 카메라 데이터를 입력한다.
void GFX::addCameraData(const SamplePipeline::CameraData& cameraData) {
	cameraDataSamplePipeline_ = cameraData;
}

// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent(const PBRPipeline::DrawEvent& drawEvent) {
	drawEventsPBRPipeline_.push_back(drawEvent);
}

// 카메라 데이터를 입력한다.
void GFX::addCameraData(const PBRPipeline::CameraData& cameraData) {
	cameraDataPBRPipeline_ = cameraData;
}

// 조명 데이터를 입력한다.
void GFX::addLightData(const PBRPipeline::LightData& lightData) {
	lightDataPBRPipeline_.push_back(lightData);
	if (lightData.isMainDirectionalLight) {
		mainDirectionalLightPBRPipeline_ = lightData;
	}
}

// 프레임 데이터를 입력한다.
void GFX::addFrameData(const PBRPipeline::FrameData& frameData) {
	frameDataPBRPipeline_ = frameData;
}

// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent(const PBRSkinnedPipeline::DrawEvent& drawEvent) {
	drawEventsPBRSkinnedPipeline_.push_back(drawEvent);
}

// 카메라 데이터를 입력한다.
void GFX::addCameraData(const PBRSkinnedPipeline::CameraData& cameraData) {
	cameraDataPBRSkinnedPipeline_ = cameraData;
}

// 조명 데이터를 입력한다.
void GFX::addLightData(const PBRSkinnedPipeline::LightData& lightData) {
	lightDataPBRSkinnedPipeline_.push_back(lightData);
	if (lightData.isMainDirectionalLight) {
		mainDirectionalLightPBRSkinnedPipeline_ = lightData;
	}
}

// 프레임 데이터를 입력한다.
void GFX::addFrameData(const PBRSkinnedPipeline::FrameData& frameData) {
	frameDataPBRSkinnedPipeline_ = frameData;
}

// ===== 로비 대기실 슬롯 포트레이트 =====
void GFX::addLobbyPortraitDrawEvent(u32t slot, PBRSkinnedPipeline::DrawEvent&& drawEvent) {
	if (slot >= kMaxPortraitSlots) return;
	drawEventsLobbyPortrait_[slot].push_back(std::move(drawEvent));
}

void GFX::addLobbyPortraitDrawEventStatic(u32t slot, PBRPipeline::DrawEvent&& drawEvent) {
	if (slot >= kMaxPortraitSlots) return;
	drawEventsLobbyPortraitStatic_[slot].push_back(std::move(drawEvent));
}

void GFX::setLobbyPortraitCamera(u32t slot, const PBRSkinnedPipeline::CameraData& cameraData) {
	if (slot >= kMaxPortraitSlots) return;
	cameraDataLobbyPortrait_[slot] = cameraData;
}

void GFX::addLobbyPortraitLightData(const PBRSkinnedPipeline::LightData& lightData) {
	lightDataLobbyPortrait_.push_back(lightData);
}

void GFX::addLobbyPortraitFrameData(const PBRSkinnedPipeline::FrameData& frameData) {
	frameDataLobbyPortrait_ = frameData;
}

void GFX::setLobbyPortraitActive(bool active) {
	lobbyPortraitActive_ = active;
}

const Texture* GFX::lobbyPortraitTextureForThisFrame() const {
	if (SharedResources::Portrait::portraitData.empty()) return nullptr;
	const auto roomIdx = frameIdx_ % backBuffers_.size();
	return &SharedResources::Portrait::portraitData[roomIdx].color;
}

XMFLOAT4 GFX::lobbyPortraitCellUvScaleBias(u32t slot) const {
	// 가로 아틀라스에서 slot 셀의 sub-rect. uv' = uv * (scaleX, scaleY) + (biasX, biasY).
	const float scaleX = 1.f / static_cast<float>(kMaxPortraitSlots);
	const float biasX  = static_cast<float>(slot) * scaleX;
	return XMFLOAT4(scaleX, 1.f, biasX, 0.f);
}

// ===== 미니맵 배경 캐시 =====
void GFX::requestMinimapRebake() {
	minimapRebakeRequested_ = true;
}

void GFX::addMinimapDrawEvent(MinimapTerrainPipeline::DrawEvent&& drawEvent) {
	drawEventsMinimap_.push_back(std::move(drawEvent));
}

void GFX::addMinimapPropDrawEvent(MinimapPropPipeline::DrawEvent&& drawEvent) {
	drawEventsMinimapProp_.push_back(std::move(drawEvent));
}

void GFX::setMinimapCamera(const MinimapTerrainPipeline::CameraData& cameraData) {
	cameraDataMinimap_ = cameraData;
}

const Texture* GFX::minimapTextureForThisFrame() const {
	if (!SharedResources::Minimap::created) return nullptr;
	return &SharedResources::Minimap::minimapData.texA;
}

// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent(const PBRDeferredPipeline::DrawEvent& drawEvent) {
	drawEventsPBRDeferredPipeline_.push_back(drawEvent);
}

// 카메라 데이터를 입력한다.
void GFX::addCameraData(const PBRDeferredPipeline::CameraData& cameraData) {
	cameraDataPBRDeferredPipeline_ = cameraData;
}

// 조명 데이터를 입력한다.
void GFX::addLightData(const PBRDeferredPipeline::LightData& lightData) {
	lightDataPBRDeferredPipeline_.push_back(lightData);
	lightDataPBRDeferredLighting_.push_back(lightData);
	if (lightData.isMainDirectionalLight) {
		mainDirectionalLightPBRDeferredPipeline_ = lightData;
	}
}

// 프레임 데이터를 입력한다.
void GFX::addFrameData(const PBRDeferredPipeline::FrameData& frameData) {
	frameDataPBRDeferredPipeline_ = frameData;
}

// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent(const PBRDeferredSkinnedPipeline::DrawEvent& drawEvent) {
	drawEventsPBRDeferredSkinnedPipeline_.push_back(drawEvent);
}

// 카메라 데이터를 입력한다.
void GFX::addCameraData(const PBRDeferredSkinnedPipeline::CameraData& cameraData) {
	cameraDataPBRDeferredSkinnedPipeline_ = cameraData;
}

// 조명 데이터를 입력한다.
void GFX::addLightData(const PBRDeferredSkinnedPipeline::LightData& lightData) {
	lightDataPBRDeferredSkinnedPipeline_.push_back(lightData);
	if (lightData.isMainDirectionalLight) {
		mainDirectionalLightPBRDeferredSkinnedPipeline_ = lightData;
	}
}

// 프레임 데이터를 입력한다.
void GFX::addFrameData(const PBRDeferredSkinnedPipeline::FrameData& frameData) {
	frameDataPBRDeferredSkinnedPipeline_ = frameData;
}

// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent( const BillboardPipeline::DrawEvent& drawEvent ) {
	drawEventsBillboardPipeline_.push_back( drawEvent );
}

// 카메라 데이터를 입력한다.
void GFX::addCameraData( const BillboardPipeline::CameraData& cameraData ) {
	cameraDataBillboardPipeline_ = cameraData;
}

// 프레임 데이터를 입력한다.
void GFX::addFrameData( const BillboardPipeline::FrameData& frameData ) {
	frameDataBillboardPipeline_ = frameData;
}

void GFX::addDrawEvent( const MeshParticlePipeline::DrawEvent& drawEvent ) {
	drawEventsMeshParticlePipeline_.push_back( drawEvent );
}

void GFX::addCameraData( const MeshParticlePipeline::CameraData& cameraData ) {
	cameraDataMeshParticlePipeline_ = cameraData;
}

void GFX::addFrameData( const MeshParticlePipeline::FrameData& frameData ) {
	frameDataMeshParticlePipeline_ = frameData;
}

void GFX::addDrawEvent( const EnergyOrbPipeline::DrawEvent& drawEvent ) {
	drawEventsEnergyOrbPipeline_.push_back( drawEvent );
}

void GFX::addCameraData( const EnergyOrbPipeline::CameraData& cameraData ) {
	cameraDataEnergyOrbPipeline_ = cameraData;
}

void GFX::addFrameData( const EnergyOrbPipeline::FrameData& frameData ) {
	frameDataEnergyOrbPipeline_ = frameData;
}

void GFX::addHeatSource( const HeatDistortionShader::HeatSource& source ) {
	if (heatSources_.size() >= HeatDistortionShader::kMaxSources) return;
	heatSources_.push_back( source );
}

void GFX::setHeatGlobals( float timeSec, float warpStrength, float glowStrength ) {
	heatTimeSec_      = timeSec;
	heatWarpStrength_ = warpStrength;
	heatGlowStrength_ = glowStrength;
}

void GFX::addDrawEvent( const WindRingPipeline::DrawEvent& drawEvent ) {
	drawEventsWindRingPipeline_.push_back( drawEvent );
}

void GFX::addCameraData( const WindRingPipeline::CameraData& cameraData ) {
	cameraDataWindRingPipeline_ = cameraData;
}

void GFX::addDrawEvent( const SmokeBlendCGPipeline::DrawEvent& drawEvent ) {
	drawEventsSmokeBlendCGPipeline_.push_back( drawEvent );
}

void GFX::addCameraData( const SmokeBlendCGPipeline::CameraData& cameraData ) {
	cameraDataSmokeBlendCGPipeline_ = cameraData;
}

void GFX::addFrameData( const SmokeBlendCGPipeline::FrameData& frameData ) {
	frameDataSmokeBlendCGPipeline_ = frameData;
}

void GFX::addDrawEvent( const BlendCGMeshPipeline::DrawEvent& drawEvent ) {
	drawEventsBlendCGMeshPipeline_.push_back( drawEvent );
}

void GFX::addCameraData( const BlendCGMeshPipeline::CameraData& cameraData ) {
	cameraDataBlendCGMeshPipeline_ = cameraData;
}

void GFX::addFrameData( const BlendCGMeshPipeline::FrameData& frameData ) {
	frameDataBlendCGMeshPipeline_ = frameData;
}

void GFX::addDrawEvent( const PiercingMeshPipeline::DrawEvent& drawEvent ) {
	drawEventsPiercingMeshPipeline_.push_back( drawEvent );
}

void GFX::addCameraData( const PiercingMeshPipeline::CameraData& cameraData ) {
	cameraDataPiercingMeshPipeline_ = cameraData;
}

void GFX::addFrameData( const PiercingMeshPipeline::FrameData& frameData ) {
	frameDataPiercingMeshPipeline_ = frameData;
}

void GFX::addDrawEvent( const PiercingSlashMeshPipeline::DrawEvent& drawEvent ) {
	drawEventsPiercingSlashMeshPipeline_.push_back( drawEvent );
}

void GFX::addCameraData( const PiercingSlashMeshPipeline::CameraData& cameraData ) {
	cameraDataPiercingSlashMeshPipeline_ = cameraData;
}

void GFX::addFrameData( const PiercingSlashMeshPipeline::FrameData& frameData ) {
	frameDataPiercingSlashMeshPipeline_ = frameData;
}

void GFX::addDrawEvent( const SwordSlashPipeline::DrawEvent& drawEvent ) {
	drawEventsSwordSlashPipeline_.push_back( drawEvent );
}

void GFX::addCameraData( const SwordSlashPipeline::CameraData& cameraData ) {
	cameraDataSwordSlashPipeline_ = cameraData;
}

void GFX::addFrameData( const SwordSlashPipeline::FrameData& frameData ) {
	frameDataSwordSlashPipeline_ = frameData;
}

void GFX::addDrawEvent( const TwoSidesPipeline::DrawEvent& drawEvent ) {
	drawEventsTwoSidesPipeline_.push_back( drawEvent );
}

void GFX::addCameraData( const TwoSidesPipeline::CameraData& cameraData ) {
	cameraDataTwoSidesPipeline_ = cameraData;
}

void GFX::addFrameData( const TwoSidesPipeline::FrameData& frameData ) {
	frameDataTwoSidesPipeline_ = frameData;
}

void GFX::addDrawEvent( TrailPipeline::DrawEvent&& drawEvent ) {
	drawEventsTrailPipeline_.push_back( std::move( drawEvent ) );
}

void GFX::addHDRTrailDrawEvent( TrailPipeline::DrawEvent&& drawEvent ) {
	drawEventsTrailPipelineHDR_.push_back( std::move( drawEvent ) );
}

void GFX::addCameraData( const TrailPipeline::CameraData& cameraData ) {
	cameraDataTrailPipeline_ = cameraData;
}

void GFX::addFrameData( const TrailPipeline::FrameData& frameData ) {
	frameDataTrailPipeline_ = frameData;
}

// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent(const UIPipeline::DrawEvent& drawEvent) {
	drawEventsUIPipeline_.push_back(drawEvent);
	// 현재 활성 클립이 있으면 이 이벤트에 stamping(스크롤 영역 클리핑).
	if (!uiClipStack_.empty()) {
		auto& e = drawEventsUIPipeline_.back();
		e.clip = true;
		e.clipRect = uiClipStack_.back();
	}
}

void GFX::pushUIClip(const RECT& rect) {
	RECT c = rect;
	if (!uiClipStack_.empty()) {
		const RECT& t = uiClipStack_.back();
		c.left   = std::max(c.left,   t.left);
		c.top    = std::max(c.top,    t.top);
		c.right  = std::min(c.right,  t.right);
		c.bottom = std::min(c.bottom, t.bottom);
	}
	if (c.right  < c.left) c.right  = c.left;
	if (c.bottom < c.top)  c.bottom = c.top;
	uiClipStack_.push_back(c);
}

void GFX::popUIClip() {
	if (!uiClipStack_.empty()) uiClipStack_.pop_back();
}

// 프레임 데이터를 입력한다.
void GFX::addFrameData(const UIPipeline::FrameData& frameData) {
	frameDataUIPipeline_ = frameData;
}

void GFX::addRequestModelLoad(const RequestModelLoad& request) {
	requestsModelLoad_.push_back(request);
}

void GFX::addRequestSkyboxLoad(const RequestSkyboxLoad& request) {
	requestsSkyboxLoad_.push_back(request);
}

void GFX::addRequestTextureLoad( const RequestTextureLoad& request )
{
	requestsTextureLoad_.push_back( request );
}

void GFX::addRequestSpritesLoad( const RequestSpriteAnimLoad& request )
{
	requestsSpritesLoad_.push_back( request );
}

void GFX::addRequestTextImageLoad( const RequestTextImageLoad& request )
{
	requestsTextImageLoad_.push_back( request );
}

// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent(const TerrainPipeline::DrawEvent& drawEvent) {
	drawEventsTerrainPipeline_.push_back(drawEvent);
}
// 카메라 데이터를 입력한다.
void GFX::addCameraData(const TerrainPipeline::CameraData& cameraData) {
	cameraDataTerrainPipeline_ = cameraData;
}
// 조명 데이터를 입력한다.
void GFX::addLightData(const TerrainPipeline::LightData& lightData) {
	lightDataTerrainPipeline_.push_back(lightData);
	if (lightData.isMainDirectionalLight) {
		mainDirectionalLightTerrainPipeline_ = lightData;
	}
}
// 프레임 데이터를 입력한다.
void GFX::addFrameData( const TerrainPipeline::FrameData& frameData ) {
	frameDataTerrainPipeline_ = frameData;
}
// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent(const TerrainDeferredPipeline::DrawEvent& drawEvent) {
	drawEventsTerrainDeferredPipeline_.push_back(drawEvent);
}
// 카메라 데이터를 입력한다.
void GFX::addCameraData(const TerrainDeferredPipeline::CameraData& cameraData) {
	cameraDataTerrainDeferredPipeline_ = cameraData;
}
// 조명 데이터를 입력한다.
void GFX::addLightData(const TerrainDeferredPipeline::LightData& lightData) {
	if (lightData.isMainDirectionalLight) {
		mainDirectionalLightTerrainDeferredPipeline_ = lightData;
	}
}
// 프레임 데이터를 입력한다.
void GFX::addFrameData(const TerrainDeferredPipeline::FrameData& frameData) {
	frameDataTerrainDeferredPipeline_ = frameData;
}

// Hi-z occlusion culling에 사용할 occluder의 정보를 입력한다.
void GFX::addOccluder(const TerrainPipeline::OccluderInfo& occluderInfo) {
	occluderInfosTerrain_.push_back(occluderInfo);
}

// Hi-z occlusion culling에 사용할 occluder의 정보를 입력한다.
void GFX::addOccluder(const TerrainDeferredPipeline::OccluderInfo& occluderInfo) {
	occluderInfosTerrainDeferred_.push_back(occluderInfo);
}

// 정적 prop occluder(근거리 BVH prop)를 Hi-Z source depth에 기록하기 위해 입력한다.
void GFX::addOccluder(const PBRDeferredPipeline::OccluderInfo& occluderInfo) {
	occluderInfosPBRDeferred_.push_back(occluderInfo);
}

void GFX::addRequestMeshBinLoad(const RequestMeshBinLoad& request) {
	requestsMeshBinLoad_.push_back(request);
}

void GFX::addRequestBakeAnimation(const RequestBakeAnimation& request) {
	requestsBakeAnimation_.push_back(request);
}

// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent(const SkyboxPipeline::DrawEvent& drawEvent) {
	drawEventsSkyboxPipeline_.push_back(drawEvent);
}

// 카메라 데이터를 입력한다.
void GFX::addCameraData(const SkyboxPipeline::CameraData& cameraData) {
	cameraDataSkyboxPipeline_ = cameraData;
}

// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent(const BVPipeline::DrawEvent& drawEvent) {
	drawEventsBVPipeline_.push_back(drawEvent);
}

// 카메라 데이터를 입력한다.
void GFX::addCameraData(const BVPipeline::CameraData& cameraData) {
	cameraDataBVPipeline_ = cameraData;
}

// 파이프라인들이 자체적으로 사용하는 리소스들과
// addRequestXXLoad 꼴의 함수로 요청된 리소스들을 로드/생성한다.
// 반드시 모든 장치 초기화가 끝나고 호출되어야 한다.
void GFX::initSharedResources(const AssetConfigs& configs) {
	auto& fence = fences_.at("LoadFence");

	// 명령 리스트와 명령 할당자 할당
	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR(
		cmdListPool_.allocOne(CommandListUsage::ResourceLoading, cmdCtx),
		"[GFX Error] GFX::loadMeshes: 사용 가능한 명령 리스트가 없습니다. "
		"CommandListPool::init 호출이 이루어지지 않았거나, 할당받은 명령 리스트가 반납되지 않았거나,"
		"너무 많은 명령 리스트의 할당이 요청되었습니다.",
		false
	);
	auto& cmdList = cmdCtx.cmdList;
	auto& cmdAlloc = cmdCtx.cmdAlloc;

	// 명령 리스트 초기화
	DISPLAY_ERROR_DX_VOID(cmdAlloc->Reset(), false);
	DISPLAY_ERROR_DX_VOID(cmdList->Reset(cmdAlloc.Get(), nullptr), false);
	
	// 파이프라인 공용 리소스 로드
	SharedResources::ShadowMap::addCSMShadowMap(
		configs.shadowMap.key, device_.Get(),
		configs.shadowMap.format,
		configs.shadowMap.cascadeResolutions,
		configs.shadowMap.cascadeCount,
		backBuffers_.size(), srvTexPool_, dsvPool_
	);
	// GBuffer 생성 (deferred path용)
	SharedResources::GBuffer::addGBuffer(
		device_.Get(),
		static_cast<u32t>(gClientRect.right  - gClientRect.left),
		static_cast<u32t>(gClientRect.bottom - gClientRect.top),
		backBuffers_.size(), rtvPool_, dsvPool_, srvTexPool_
	);
	// HDR scene-color RT (deferred path lighting/skybox 렌더 타깃; tonemap resolve로 백버퍼에 합성)
	SharedResources::SceneColor::addSceneColor(
		device_.Get(),
		static_cast<u32t>(gClientRect.right  - gClientRect.left),
		static_cast<u32t>(gClientRect.bottom - gClientRect.top),
		backBuffers_.size(), rtvPool_, srvTexPool_
	);
	// Bloom HDR mip chain (half-res; deferred path 합성용). SceneColor와 동일 수명/resize.
	SharedResources::Bloom::addBloom(
		device_.Get(),
		static_cast<u32t>(gClientRect.right  - gClientRect.left),
		static_cast<u32t>(gClientRect.bottom - gClientRect.top),
		backBuffers_.size(), rtvPool_, srvTexPool_
	);
	// IBL 타깃 리소스 생성 (irradiance/prefiltered 큐브 + BRDF LUT). 정적(환경 의존),
	// 스카이박스 큐브 상주 후 loadRequestedAssets에서 precomputeIBL이 채운다.
	SharedResources::IBL::addIBL(device_.Get(), uavPool_, srvTexCubePool_, srvTexPool_);
	// Color grading LUT. 씬 전역, 정적, tonemap resolve 패스가 gamma 보정 직후 항상 적용한다.
	SharedResources::ColorGrading::addColorGradingLUT(
		device_.Get(), cmdList.Get(), fence, srvTex3DPool_,
		"../resources/LUT/warm-natural_6.C0008.cube"
	);
	// hi-z map 생성 (hi-z occlusion culling용)
	SharedResources::HiZMap::addHiZMaps(
		device_.Get(), static_cast<u32t>(gClientRect.right  - gClientRect.left),
		static_cast<u32t>(gClientRect.bottom - gClientRect.top), backBuffers_.size(),
		srvTexPool_, uavPool_, dsvPool_
	);
	// 로비 대기실 슬롯 캐릭터용 오프스크린 포트레이트 RT (가로 아틀라스, 세로형 셀)
	SharedResources::Portrait::addPortraitRT(
		device_.Get(), kPortraitCellW, kPortraitCellH, kMaxPortraitSlots,
		backBuffers_.size(), rtvPool_, dsvPool_, srvTexPool_
	);
	// 포트레이트 슬롯별 전용 Resources (mainPass만, 소용량. shadowPass는 호출 안 하므로 미init).
	for (u32t s = 0u; s < kMaxPortraitSlots; ++s) {
		auto& res = resourcesLobbyPortrait_[s];
		const auto sfx = "_Portrait" + std::to_string(s);
		res.mainPass.perInstanceData.init(
			device_.Get(), sizeof(PBRSkinnedShader::PerInstanceData) * kMaxPortraitDrawEventsPerSlot,
			backBuffers_.size(), ("Skinned_Main_PerInstanceData" + sfx)
		);
		res.mainPass.boneData.init(
			device_.Get(), sizeof(PBRSkinnedShader::BoneData) * kMaxPortraitBonesPerSlot,
			backBuffers_.size(), ("Skinned_Main_BoneData" + sfx)
		);
		res.mainPass.perDrawcallData = createConstantBufferArray(
			device_.Get(), sizeof(PBRSkinnedShader::PerDrawcallData), kMaxPortraitDrawEventsPerSlot,
			backBuffers_.size(), ("Skinned_Main_PerDrawcallData" + sfx)
		);
		res.mainPass.lightData.init(
			device_.Get(), sizeof(PBRSkinnedShader::Light) * 32u,
			backBuffers_.size(), ("Skinned_Main_LightData" + sfx)
		);
		res.mainPass.perFrameData.init(
			device_.Get(), sizeof(PBRSkinnedShader::PerFrameData),
			backBuffers_.size(), ("Skinned_Main_PerFrameData" + sfx)
		);
	}
	// 포트레이트 슬롯별 장착 무기(non-skinned) 전용 Resources (mainPass만, 소용량).
	for (u32t s = 0u; s < kMaxPortraitSlots; ++s) {
		auto& res = resourcesLobbyPortraitStatic_[s];
		const auto sfx = "_PortraitStatic" + std::to_string(s);
		res.mainPass.perInstanceData.init(
			device_.Get(), sizeof(PBRShader::PerInstanceData) * kMaxPortraitStaticDrawEventsPerSlot,
			backBuffers_.size(), ("PBR_Main_PerInstanceData" + sfx)
		);
		res.mainPass.perDrawcallData = createConstantBufferArray(
			device_.Get(), sizeof(PBRShader::PerDrawcallData), kMaxPortraitStaticDrawEventsPerSlot,
			backBuffers_.size(), ("PBR_Main_PerDrawcallData" + sfx)
		);
		res.mainPass.lightData.init(
			device_.Get(), sizeof(PBRShader::Light) * 32u,
			backBuffers_.size(), ("PBR_Main_LightData" + sfx)
		);
		res.mainPass.perFrameData.init(
			device_.Get(), sizeof(PBRShader::PerFrameData),
			backBuffers_.size(), ("PBR_Main_PerFrameData" + sfx)
		);
	}
	// 미니맵 배경 캐시 RT 쌍(texA/texB) — 단일 인스턴스(per-room 아님; 위 SharedResources::Minimap 주석 참조).
	SharedResources::Minimap::addMinimapRT(
		device_.Get(), kMinimapRTSize, rtvPool_, srvTexPool_
	);
	resourcesMinimapTerrain_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(MinimapTerrainShader::PerDrawcallData), MinimapTerrainPipeline::kMaxDrawEvents,
		backBuffers_.size(), "MinimapTerrain_PerDrawcallData"
	);
	resourcesMinimapProp_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(MinimapPropShader::PerDrawcallData), MinimapPropPipeline::kMaxDrawEvents,
		backBuffers_.size(), "MinimapProp_PerDrawcallData"
	);
	resourcesMinimapFogBlur_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(MinimapFogBlurShader::PerDrawcallData), MinimapFogBlurPipeline::kPassCount,
		backBuffers_.size(), "MinimapFogBlur_PerDrawcallData"
	);

	// 파이프라인 자체 리소스 로드
	// BVPipeline
	BVPipeline::initStaticModels(device_.Get(), cmdList.Get(), fence);
	// BillboardPipeline
	BillboardPipeline::initStaticPointMesh( device_.Get(), cmdList.Get(), fence );
	// UIPipeline
	UIPipeline::initStaticQuadMesh( device_.Get(), cmdList.Get(), fence );

	// 1x1 white pixel texture — solid-color UI fallback
	createTextImageImmediate( 1, 1, &solidColorImage_ );
	solidColorImage_.pData = { 0xFF, 0xFF, 0xFF, 0xFF };
	UpdateTextureWithTextImage( &solidColorImage_, 1, 1 );
	::UpdateTexture( cmdList.Get(), solidColorImage_.textureUpload, solidColorImage_.texture );

	dumpLog();

	// 공용 리소스 명령 기록 끝, 명령 실행
	DISPLAY_ERROR_DX_VOID(cmdList->Close(), false);
	ID3D12CommandList* sharedCmdLists[] = { cmdList.Get() };
	DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, sharedCmdLists), false);

	// 펜스 동기화
	fence.associatedCmdCtxs_[etoi(CommandListUsage::ResourceLoading)].push_back(std::move(cmdCtx));
	signalFence("LoadFence");
	waitOnFence("LoadFence");
}

void GFX::loadRequestedAssets() {
	auto& fence = fences_.at("LoadFence");

	// 명령 리스트와 명령 할당자 할당
	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR(
		cmdListPool_.allocOne(CommandListUsage::ResourceLoading, cmdCtx),
		"[GFX Error] GFX::loadRequestedAssets: 사용 가능한 명령 리스트가 없습니다. "
		"CommandListPool::init 호출이 이루어지지 않았거나, 할당받은 명령 리스트가 반납되지 않았습니다.",
		false
	);
	auto& cmdList = cmdCtx.cmdList;
	auto& cmdAlloc = cmdCtx.cmdAlloc;

	// 명령 리스트 초기화
	DISPLAY_ERROR_DX_VOID(cmdAlloc->Reset(), false);
	DISPLAY_ERROR_DX_VOID(cmdList->Reset(cmdAlloc.Get(), nullptr), false);

	// Progress tracking for this batch (all 7 request vectors, incl. bake animation).
	assetLoadTotal_.store(static_cast<uint32_t>(
		requestsModelLoad_.size() + requestsSkyboxLoad_.size() + requestsTextureLoad_.size()
		+ requestsSpritesLoad_.size() + requestsTextImageLoad_.size() + requestsMeshBinLoad_.size()
		+ requestsBakeAnimation_.size()), std::memory_order_relaxed);
	assetLoadDone_.store(0u, std::memory_order_relaxed);

	// 명령 기록 시작
	for (auto& request : requestsModelLoad_) {
		*request.pDest = loadModelFromFile( request.modelPath,
			device_.Get(), cmdList.Get(), *request.pTexHashMap, srvTexPool_, fence
		);
		assetLoadDone_.fetch_add(1, std::memory_order_relaxed);
	}

	dumpLog();

	for (auto& request : requestsSkyboxLoad_) {
		*request.pDest = loadSkyboxFromFile(request.skyboxPath, device_.Get(), cmdList.Get(), srvTexCubePool_, fence);
		assetLoadDone_.fetch_add(1, std::memory_order_relaxed);
	}

	dumpLog();

	// load textures
	for ( auto& request : requestsTextureLoad_ ) {
		if ( !request.pTexHashMap->contains( request.name ) ) {
			Texture::Type type{};
			auto [pPair, _] = request.pTexHashMap->try_emplace( request.name, loadTexture( device_.Get(), cmdList.Get(), request.texturePath, fence, type ) );
			auto& tex = pPair->second;
			createSRV( device_.Get(), tex, srvTexPool_ );
			tex.idxSrv.idxSampler = etoi( request.sampler );
			*request.pDest = tex;

			if (request.needsUploadInfo) {
				tex.uploadInfo = std::make_shared<TextureUploadInfo>();
				tex.uploadInfo->resDesc = tex.res->GetDesc();

				if ( tex.uploadInfo->resDesc.MipLevels > static_cast<UINT16>( tex.uploadInfo->footprints.size() ) )
					__debugbreak();

				device_->GetCopyableFootprints( &tex.uploadInfo->resDesc, 0, tex.uploadInfo->resDesc.MipLevels,
					0, tex.uploadInfo->footprints.data(), tex.uploadInfo->rowCounts.data(),
					tex.uploadInfo->rowSizes.data(), &tex.uploadInfo->totalSize
				);
			}
		}
		else {
			*request.pDest = request.pTexHashMap->at(request.name);
		}
		assetLoadDone_.fetch_add(1, std::memory_order_relaxed);
	}

	dumpLog();

	// load sprites
	for ( auto& request : requestsSpritesLoad_ ) {
		*request.pDest = loadSpriteSheetAnimation(
			request.sheetPath, request.rows, request.cols, request.frameCount,
			request.type, request.frameTime,
			device_.Get(), cmdList.Get(), srvTexPool_, fence
		);
		assetLoadDone_.fetch_add(1, std::memory_order_relaxed);
	}

	dumpLog();

	// load text texture
	for( auto& request : requestsTextImageLoad_ ) {
		*request.pDest = TextImage( device_.Get(), request.width, request.height, srvTexPool_ );
		assetLoadDone_.fetch_add(1, std::memory_order_relaxed);
	}

	dumpLog();

	// (Terrain is streamed per-chunk by TerrainChunkManager, not loaded here.)
	// load .meshbin files
	for (auto& request : requestsMeshBinLoad_) {
		auto [mesh, texRelPath] = loadMeshBin(request.meshPath, device_.Get(), cmdList.Get(), fence);
		*request.pDestMesh = std::move(mesh);

		if (!texRelPath.empty() && request.pDestTex) {
			const auto texPath = std::filesystem::path("../resources/Textures") / texRelPath;
			if (!request.pTexHashMap->contains(texRelPath)) {
				Texture::Type type{};
				auto [pPair, _] = request.pTexHashMap->try_emplace(
					texRelPath, loadTexture(device_.Get(), cmdList.Get(), texPath, fence, type)
				);
				auto& tex = pPair->second;
				createSRV(device_.Get(), tex, srvTexPool_);
				tex.idxSrv.idxSampler = etoi(Samplers::TrilinearWrap);
			}
			*request.pDestTex = request.pTexHashMap->at(texRelPath);
		}
		assetLoadDone_.fetch_add(1, std::memory_order_relaxed);
	}

	dumpLog();

	// bake animations
	for (auto& request : requestsBakeAnimation_) {
		*request.pDest = bakeAnimation(device_.Get(), cmdList.Get(), request.samples, request.uploadBuffer);
		createSRV(device_.Get(), *request.pDest, srvTexPool_);
		assetLoadDone_.fetch_add(1, std::memory_order_relaxed);
	}

	dumpLog();

	// 명령 기록 끝, 명령 실행
	DISPLAY_ERROR_DX_VOID(cmdList->Close(), false);

	ID3D12CommandList* tmpCmdLists[] = { cmdList.Get() };

	DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, tmpCmdLists), false);

	// 펜스 동기화
	fence.associatedCmdCtxs_[etoi(CommandListUsage::ResourceLoading)].push_back(std::move(cmdCtx));
	signalFence("LoadFence");
	waitOnFence("LoadFence");

	// IBL 프리컴퓨트: 스카이박스 큐브 데이터가 상주(LoadFence 완료)한 직후 1회 실행.
	// requestsSkyboxLoad_가 아직 비워지지 않아 로드된 Skybox(texSkybox)에 접근 가능.
	if (!requestsSkyboxLoad_.empty() && requestsSkyboxLoad_.front().pDest
		&& requestsSkyboxLoad_.front().pDest->texSkybox.res) {
		const auto& skybox = *requestsSkyboxLoad_.front().pDest;
		precomputeIBL(
			device_.Get(),
			skybox.texSkybox.idxSrv,
			skybox.texSkybox.res.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,   // loadDDS leaves the env cube in COPY_DEST (no read-state transition)
			rootSigs_.at("DefaultRootSignature").get(),
			shaders_.at("IBLIrradianceShader").Get(),
			shaders_.at("IBLPrefilterShader").Get(),
			shaders_.at("IBLBRDFLUTShader").Get(),
			{ srvCbvUavHeap_.heap, samHeap_.heap },
			srvTexPool_, srvTexArrayPool_, srvTexCubePool_, samPool_, cmpSamPool_,
			iblParamsCBs_,
			submitter_.get(), cmdListPool_, fences_.at("LoadFence"),
			// envIsLDR=false: 현재 환경맵이 LDR이지만 역Reinhard(c/(1-c))가 하늘의 1.0 근처
			// 채널을 폭발적으로 증폭해 과도한 파란 틴트를 유발하므로, 원본 LDR 값을 그대로 쓴다.
			// (진짜 HDR HDRI 확보 시 자연스럽게 HDR 레인지 확보 → 이 플래그 무관.)
			/*envIsLDR=*/ false
		);
	}

	// 요청 비우기
	requestsModelLoad_.clear();
	requestsSkyboxLoad_.clear();
	requestsTextureLoad_.clear();
	requestsSpritesLoad_.clear();
	requestsTextImageLoad_.clear();
	requestsMeshBinLoad_.clear();
	requestsBakeAnimation_.clear();
}

void GFX::recordTerrainResourceLoad(
	const std::function<void(ID3D12Device*, ID3D12GraphicsCommandList*, DescriptorPool&, Fence&)>& recorder,
	bool wait
) {
	auto& fence = fences_.at("LoadFence");

	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR(
		cmdListPool_.allocOne(CommandListUsage::ResourceLoading, cmdCtx),
		"[GFX Error] GFX::recordTerrainResourceLoad: 사용 가능한 명령 리스트가 없습니다.",
		false
	);
	auto& cmdList  = cmdCtx.cmdList;
	auto& cmdAlloc = cmdCtx.cmdAlloc;

	DISPLAY_ERROR_DX_VOID(cmdAlloc->Reset(), false);
	DISPLAY_ERROR_DX_VOID(cmdList->Reset(cmdAlloc.Get(), nullptr), false);

	// Record terrain GPU work (mesh uploads, splat SRV creation, ...).
	recorder(device_.Get(), cmdList.Get(), srvTexPool_, fence);

	DISPLAY_ERROR_DX_VOID(cmdList->Close(), false);
	ID3D12CommandList* tmpCmdLists[] = { cmdList.Get() };
	DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, tmpCmdLists), false);

	fence.associatedCmdCtxs_[etoi(CommandListUsage::ResourceLoading)].push_back(std::move(cmdCtx));
	signalFence("LoadFence");
	if (wait) waitOnFence("LoadFence");
}

void GFX::resize(u32t width, u32t height) {
	if (!swapChain_ || width == 0u || height == 0u) {
		return;
	}

	// 1. GPU가 모든 인플라이트 프레임 작업을 끝낼 때까지 대기(소멸자와 동일한 idle 패턴).
	//    drainGpu()는 제출 스레드 큐를 먼저 flush해 대기 중인 Present/Signal을 모두 발행한 뒤
	//    Fence를 대기한다. 이후 해상도 의존 리소스를 안전하게 해제/재생성할 수 있다.
	drainGpu();

	// 2. 해상도 의존 리소스 해제 + 디스크립터 풀 슬롯 반납.
	//    (포트레이트 RT는 셀 고정 크기라 해상도와 무관 → 건드리지 않는다.)
	SharedResources::GBuffer::eraseGBuffer(rtvPool_, dsvPool_, srvTexPool_);
	SharedResources::SceneColor::eraseSceneColor(rtvPool_, srvTexPool_);
	SharedResources::Bloom::eraseBloom(rtvPool_, srvTexPool_);
	SharedResources::HiZMap::eraseHiZMaps(srvTexPool_, uavPool_, dsvPool_);

	for (int idx : allocatedRtvIndices_) { rtvPool_.free(idx); }
	allocatedRtvIndices_.clear();
	backBufferRtvs_.clear();

	for (int idx : allocatedDsvIndices_) { dsvPool_.free(idx); }
	allocatedDsvIndices_.clear();
	depthBufferDsvs_.clear();

	// ResizeBuffers 전 백버퍼 리소스 참조를 모두 해제해야 한다.
	backBuffers_.clear();
	depthBuffers_.clear();

	// 3. 스왑체인 백버퍼 리사이즈(개수/포맷/플래그는 생성 시와 동일).
	DISPLAY_ERROR_DX_HR(
		swapChain_->ResizeBuffers(
			static_cast<UINT>(3), width, height,
			DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
		),
		true
	);
	scd_.Width  = width;
	scd_.Height = height;

	// ResizeBuffers는 flip-model 스왑체인의 current back buffer 인덱스를 0으로 리셋한다.
	// backbufIdx를 frameIdx_ % N으로 결정론적으로 계산하므로, 다음 프레임의 인덱스가 0이
	// 되도록 frameIdx_를 N의 배수로 올림 정렬해 스왑체인 상태와 다시 동기화한다.
	// (GPU는 위에서 idle 상태이므로 모든 Fence가 완료되어 재정렬이 안전하다.)
	const auto n = backBuffers_.size();
	frameIdx_ = ((frameIdx_ + n - 1u) / n) * n;

	// 4. 백버퍼 + RTV 재생성 (createSwapChain과 동일한 절차).
	backBuffers_.resize(3u);
	for (int i = 0; i < 3; ++i) {
		DISPLAY_ERROR_DX_HR(
			swapChain_->GetBuffer(i, __uuidof(ID3D12Resource), &backBuffers_[i]),
			true
		);
		setD3DName(backBuffers_[i].Get(), "BackBuffer"s + std::to_string(i));
	}
	for (int i = 0; i < 3; ++i) {
		auto idx = allocatedRtvIndices_.emplace_back(rtvPool_.alloc());
		auto cpuHandle = backBufferRtvs_.emplace_back(rtvPool_.cpuHandle(idx));
		DISPLAY_ERROR_DX_VOID(
			device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, cpuHandle),
			true
		);
	}

	// 5. 깊이 버퍼 + DSV 재생성. createDepthBuffer는 gClientRect를 읽으므로
	//    호출 전에 gClientRect가 새 해상도로 갱신돼 있어야 한다(applyClientResolution).
	for (int i = 0; i < 3; ++i) {
		auto depthBuffer = createDepthBuffer( device_.Get(), DXGI_FORMAT_D32_FLOAT,
			DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u }
		);
		setD3DName(depthBuffer.Get(), "DepthBuffer"s + std::to_string(i));
		depthBuffers_.push_back(std::move(depthBuffer));
	}
	for (int i = 0; i < 3; ++i) {
		auto idx = allocatedDsvIndices_.emplace_back(dsvPool_.alloc());
		auto cpuHandle = depthBufferDsvs_.emplace_back(dsvPool_.cpuHandle(idx));
		DISPLAY_ERROR_DX_VOID(
			device_->CreateDepthStencilView(depthBuffers_[i].Get(), nullptr, cpuHandle),
			true
		);
	}

	// 6. GBuffer + HiZ맵을 새 해상도로 재생성 (deferred path / occlusion culling).
	SharedResources::GBuffer::addGBuffer(
		device_.Get(), width, height, backBuffers_.size(), rtvPool_, dsvPool_, srvTexPool_
	);
	SharedResources::SceneColor::addSceneColor(
		device_.Get(), width, height, backBuffers_.size(), rtvPool_, srvTexPool_
	);
	SharedResources::Bloom::addBloom(
		device_.Get(), width, height, backBuffers_.size(), rtvPool_, srvTexPool_
	);
	SharedResources::HiZMap::addHiZMaps(
		device_.Get(), width, height, backBuffers_.size(), srvTexPool_, uavPool_, dsvPool_
	);
}

void GFX::render() {
	// 예외 검사
	DISPLAY_ERROR_STR(curAdapter_, "[GFX Error] GFX::render: 활성화된 그래픽 어댑터가 없습니다. "
		"GFX::setupDXGI의 호출이 이루어졌는지 확인하세요.", false);

	DISPLAY_ERROR_STR(device_, "[GFX Error] GFX::render: 장치 초기화가 이루어지지 않았습니다. "
		"GFX::init 호출이 이루어졌는지 확인하세요.", false);

	DISPLAY_ERROR_STR(submitter_.get(), "[GFX Error] GFX::render: 명령 큐가 활성화되지 않았습니다. "
		"GFX::init 호출이 이루어졌는지 확인하세요.", false);

	DISPLAY_ERROR_STR(swapChain_, "[GFX Error] GFX::render: 스왑 체인이 활성화되지 않았습니다. "
		"GFX::createSwapChain 호출이 이루어졌는지 확인하세요.", false);

	if (!curAdapter_ || !device_ || !cmdQ_ || !swapChain_) {
		return;
	}

	// 초기화 단계가 끝나고 첫 프레임에 진입했으므로 제출 스레드를 가동한다(이후 no-op).
	// 이 시점 이후의 모든 ExecuteCommandLists/Present/Signal은 제출 스레드에서 비동기로 처리된다.
	submitter_->goAsync();

	// 펜스 동기화
	// 렌더링할 수 있는 백버퍼가 있다면,
	// wait하지 않고 이어서 렌더링을 하도록 한다.
	// 백버퍼는 순차적으로 사용되므로, (백버퍼 개수 - 1) 프레임 전의 렌더링에 대한
	// Fence를 wait하면 이를 구현할 수 있다.
	auto idxFenceToWait = (frameIdx_ - (backBuffers_.size() - 1)) % backBuffers_.size();
	waitOnFence("FrameFence" + std::to_string(idxFenceToWait));

	// Dispatcher들은 명령 리스트 풀에서 스스로 명령 컨텍스트를 할당하고 기록, 실행한다.
	// 사용이 끝난 명령 컨텍스트는 펜스와 연관되어 gpu 사용이 끝남을 감지한 후
	// 반환되는 게 규칙이므로, Dispatcher에 명령 컨텍스트와 연관시킬 펜스를 전달해야 한다.
	auto idxFenceToSignal = frameIdx_ % backBuffers_.size();
	auto fenceNameToSignal = "FrameFence" + std::to_string(idxFenceToSignal);
	auto& fenceToSignal = fences_.at(fenceNameToSignal);

	// 렌더 타겟 클리어를 위한 명령 컨텍스트 할당
	CommandContext cmdCtxClear{};
	DISPLAY_ERROR_STR(
		cmdListPool_.allocOne(CommandListUsage::RenderingMaster, cmdCtxClear),
		"[GFX Error] GFX::render: 사용 가능한 명령 리스트가 없습니다. "
		"CommandListPool::init 호출이 이루어지지 않았거나, 할당받은 명령 리스트가 반납되지 않았습니다.",
		false
	);
	auto cmdListClear = cmdCtxClear.cmdList.Get();
	auto cmdAllocClear = cmdCtxClear.cmdAlloc.Get();

	if (!cmdListClear) {
		return;
	}

	// 클리어 명령 리스트 초기화
	DISPLAY_ERROR_DX_VOID( cmdAllocClear->Reset(), false );
	DISPLAY_ERROR_DX_VOID( cmdListClear->Reset(cmdAllocClear, nullptr), false );

	// 클리어 명령 기록 시작
	// back buffer 인덱스는 결정론적으로 계산한다. GetCurrentBackBufferIndex()는 호출된
	// Present 횟수에 따라 갱신되는데, Present가 제출 스레드에서 비동기로 일어나므로 메인 스레드가
	// 이를 질의하면 stale 값을 받아(아직 직전 프레임 Present 미처리) 잘못된 back buffer에 기록 →
	// WRONGSWAPCHAINBUFFERREFERENCE/디바이스 제거가 발생한다. flip-model에서 매 프레임 정확히
	// 1회 Present하므로 current back buffer == (Present 횟수) % N == frameIdx_ % N 이며,
	// 이는 render() 전체에서 쓰는 roomIdx와 동일하다.
	const auto backbufIdx = frameIdx_ % backBuffers_.size();

	// 메인 렌더 타겟에 대한 클리어
	transitionResourceState(cmdListClear, backBuffers_[backbufIdx].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	DISPLAY_ERROR_DX_VOID(
		cmdListClear->OMSetRenderTargets(1u, &backBufferRtvs_[backbufIdx], false, &depthBufferDsvs_[backbufIdx]),
		false
	);

	// 백버퍼는 swapchain 리소스라 optimized clear value가 없어 ClearRenderTargetView가
	// 경고(#820)를 낸다. Deferred 경로는 tonemap resolve가 백버퍼 전체를 fullscreen으로
	// 덮어쓰므로 클리어가 불필요 → forward 경로에서만 클리어한다.
	if (renderPath_ == RenderPath::Forward) {
		FLOAT clearColor[4] = { 0.25f, 0.3f, 0.85f, 1.f };
		DISPLAY_ERROR_DX_VOID(
			cmdListClear->ClearRenderTargetView(backBufferRtvs_[backbufIdx], clearColor, 0u, nullptr),
			false
		);
	}

	// D32_FLOAT 깊이 버퍼는 stencil aspect가 없으므로 DEPTH만 클리어한다(STENCIL 플래그 시 경고 #821).
	// Reversed-Z: far plane이 0.0에 매핑되므로 0.0으로 클리어한다.
	DISPLAY_ERROR_DX_VOID(
		cmdListClear->ClearDepthStencilView(depthBufferDsvs_[backbufIdx],
			D3D12_CLEAR_FLAG_DEPTH,
			0.0f, 0u, 0u, nullptr
		), false
	);

	// CSM 그림자맵: 모든 cascade를 Depth Write 상태로 전환한다 (clear CL에 기록)
	{
		const auto csmKey = std::string(SharedResources::ShadowMap::kDefaultKey);
		const auto csmRoomIdx = frameIdx_ % backBuffers_.size();
		if (SharedResources::ShadowMap::csmShadowMapData.contains(csmKey)) {
			const auto& csmData = SharedResources::ShadowMap::csmShadowMapData.at(csmKey)[csmRoomIdx];
			for (u32t ci = 0u; ci < csmData.cascadeCount; ++ci) {
				SharedResources::ShadowMap::getCSMReadyAsDepthWrite(csmKey, csmRoomIdx, ci, cmdListClear);
			}
		}
	}

	// Deferred path: GBuffer를 RENDER_TARGET 상태로 전환하고 클리어
	if (renderPath_ == RenderPath::Deferred && !SharedResources::GBuffer::gBufferData.empty()) {
		const auto gbRoomIdx = frameIdx_ % backBuffers_.size();
		SharedResources::GBuffer::transitionToWrite(gbRoomIdx, cmdListClear);
		SharedResources::GBuffer::clearGBuffer(gbRoomIdx, cmdListClear);

		// HDR scene-color RT 클리어 (deferred lighting/skybox 렌더 타깃)
		if (!SharedResources::SceneColor::sceneColorData.empty()) {
			SharedResources::SceneColor::transitionToWrite(gbRoomIdx, cmdListClear);
			constexpr FLOAT sceneColorClear[4] = { 0.f, 0.f, 0.f, 0.f };
			DISPLAY_ERROR_DX_VOID(
				cmdListClear->ClearRenderTargetView(
					SharedResources::SceneColor::sceneColorData[gbRoomIdx].rtv, sceneColorClear, 0u, nullptr
				), false
			);
		}
	}

	const auto clRect = gClientRect;

	const auto viewport = D3D12_VIEWPORT{
		.TopLeftX = static_cast<FLOAT>(clRect.left),
		.TopLeftY = static_cast<FLOAT>(clRect.top),
		.Width = static_cast<FLOAT>(clRect.right - clRect.left),
		.Height = static_cast<FLOAT>(clRect.bottom - clRect.top),
		.MinDepth = 0.f,
		.MaxDepth = 1.f
	};

	DISPLAY_ERROR_DX_VOID( cmdListClear->RSSetViewports(1u, &viewport), false );
	DISPLAY_ERROR_DX_VOID( cmdListClear->RSSetScissorRects(1u, &clRect), false );

	// 클리어 명령 기록 끝
	// 클리어 명령 리스트는 곧바로 실행한다.
	// Dispatcher들에서 드로우콜들을 수행할 때
	// 명령 큐를 사용해 실행까지 하기 때문에,
	// 클리어 명령 리스트를 Dispatcher들의 드로우콜들 수행 이후
	// 실행하게 되면 그려진 것들이 전부 지워지게 된다.
	DISPLAY_ERROR_DX_VOID( cmdListClear->Close(), false );

	ID3D12CommandList* clearCmdLists[] = { cmdListClear };

	// 클리어 명령 리스트 실행
	DISPLAY_ERROR_DX_VOID( submitter_->submit(1u, clearCmdLists), false );

	// 디스크립터 힙 바인딩 명령을 위한 gpu-visible 디스크립터 힙 모음,
	// Dispatcher들에 전달한다.
	auto tmpDescriptorHeaps = std::vector<ComPtr<ID3D12DescriptorHeap>>{};
	tmpDescriptorHeaps.push_back(srvCbvUavHeap_.heap);
	tmpDescriptorHeaps.push_back(samHeap_.heap);

	auto samplePipelineDispatcher = SamplePipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at("DefaultRootSignature"), shaders_.at("SampleShader"),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesSamplePipeline_, threadPool_,
		&cmdListPool_, std::move(drawEventsSamplePipeline_),
		cameraDataSamplePipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	// Stage light data for terrain pipeline before PBR moves the light list.
	// Terrain uses the same lights as PBR, converted to view-space GPU format.
	{
		const auto& view = cameraDataTerrainPipeline_.view;
		auto gpuLights = std::vector<PBRShader::Light>{};
		gpuLights.reserve(lightDataPBRPipeline_.size());
		for (const auto& ld : lightDataPBRPipeline_) {
			gpuLights.push_back(PBRShader::Light{
				.color     = ld.color.getXmf(),
				.falloff   = ld.falloff,
				.posV      = mu::Vec3(mu::Vec4(ld.pos, 1.f) * view).getXmf(),
				.cosTheta  = ld.cosTheta,
				.dirV      = mu::NVec3(mu::Vec4(ld.dir, 0.f) * view).getXmf(),
				.cosPhi    = ld.cosPhi,
				.atten     = ld.atten.getXmf(),
				.intensity = ld.intensity,
				.type      = static_cast<int>(ld.type),
				.padding   = {}
			});
		}
		const auto roomIdx = frameIdx_ % backBuffers_.size();
		resourcesTerrainPipeline_.mainPass.lightData.stage(roomIdx, gpuLights);
		frameDataTerrainPipeline_.lightCount = static_cast<u32t>(gpuLights.size());
	}

	auto pbrPipelineDispatcher = PBRPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_, &dsvPool_,
		rootSigs_.at("DefaultRootSignature"), csmDebugVisualization_ ? shaders_.at("PBRShaderCSMDebug") : shaders_.at("PBRShader"),
		shaders_.at("ShadowMapCSMShader"), submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesPBRPipeline_, threadPool_, &cmdListPool_,
		std::move(drawEventsPBRPipeline_), std::move(lightDataPBRPipeline_),
		mainDirectionalLightPBRPipeline_, cameraDataPBRPipeline_, frameDataPBRPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto pbrSkinnedPipelineDispatcher = PBRSkinnedPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_, &dsvPool_,
		rootSigs_.at("DefaultRootSignature"), csmDebugVisualization_ ? shaders_.at("PBRSkinnedShaderCSMDebug") : shaders_.at("PBRSkinnedShader"),
		shaders_.at("ShadowMapSkinnedCSMShader"), submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesPBRSkinnedPipeline_, threadPool_, &cmdListPool_,
		std::move(drawEventsPBRSkinnedPipeline_), std::move(lightDataPBRSkinnedPipeline_),
		mainDirectionalLightPBRSkinnedPipeline_, cameraDataPBRSkinnedPipeline_, frameDataPBRSkinnedPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	// 스카이박스 텍스처 임시 저장
	// 로비 등 스카이박스 드로우 이벤트가 없는 씬에서는 빈 벡터를 인덱싱하지 않도록 가드한다.
	auto skyboxIdxSrv = decltype(drawEventsSkyboxPipeline_.front().texSkybox->idxSrv){};
	if (!drawEventsSkyboxPipeline_.empty() && drawEventsSkyboxPipeline_[0].texSkybox) {
		skyboxIdxSrv = drawEventsSkyboxPipeline_[0].texSkybox->idxSrv;
	}

	// HDR scene-color: deferred path의 lit 출력(deferred lighting)만 SceneColorHDR로 보낸다.
	// skybox는 원래대로 톤매핑 없이 raw로, resolve 이후 백버퍼에 직접 그린다(룩 보존 + PSO 포맷 일치).
	const auto sceneColorRoomIdx = frameIdx_ % backBuffers_.size();
	const BindlessIndex sceneColorSrv = SharedResources::SceneColor::sceneColorData.empty()
		? BindlessIndex{}
		: SharedResources::SceneColor::sceneColorData[sceneColorRoomIdx].color.idxSrv;

	// Heat-distortion (boss intimidation): assemble the shared heat block consumed by
	// both the pre-bloom haze pass and the tonemap warp. Empty => sourceCount 0 => no-op.
	HeatDistortionShader::HeatParams heatParams{};
	heatParams.sourceCount = static_cast<u32t>(
		std::min<std::size_t>(heatSources_.size(), HeatDistortionShader::kMaxSources));
	for (u32t hi = 0u; hi < heatParams.sourceCount; ++hi) heatParams.sources[hi] = heatSources_[hi];
	heatParams.time         = heatTimeSec_;
	heatParams.warpStrength = heatWarpStrength_;
	heatParams.glowStrength = heatGlowStrength_;
	const BindlessIndex heatGB4 = SharedResources::GBuffer::gBufferData.empty()
		? BindlessIndex{ -1, -1, -1, -1 }
		: SharedResources::GBuffer::gBufferData[sceneColorRoomIdx].gb4.idxSrv;
	// heatParams now owns a copy; both dispatchers copy from it at construction.
	heatSources_.clear();

	// Deferred path renders the skybox INTO SceneColorHDR (before bloom/heat), so the
	// heat-distortion warp/glow + bloom apply over the sky; the resolve passes background
	// pixels through untonemapped. Forward path (lobby) keeps drawing raw to the backbuffer.
	// The PSO's RTV format must match the bound target, so pick the HDR skybox PSO for the
	// SceneColorHDR target and the LDR one for the backbuffer.
	const bool skyboxToHDR =
		(renderPath_ == RenderPath::Deferred && !SharedResources::SceneColor::sceneColorData.empty());
	const D3D12_CPU_DESCRIPTOR_HANDLE skyboxRtv = skyboxToHDR
		? SharedResources::SceneColor::sceneColorData[sceneColorRoomIdx].rtv
		: backBufferRtvs_[backbufIdx];
	auto skyboxPipelineDispatcher = SkyboxPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at("DefaultRootSignature"),
		shaders_.at(skyboxToHDR ? "SkyboxShaderHDR" : "SkyboxShader"),
		submitter_.get(), viewport, clRect,
		skyboxRtv, depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesSkyboxPipeline_, &cmdListPool_,
		std::move(drawEventsSkyboxPipeline_), cameraDataSkyboxPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	// Color grading LUT bindless index: 로드되지 않았으면(created==false) grading 미적용.
	const BindlessIndex colorGradingLUTSrv = SharedResources::ColorGrading::lutData.created
		? SharedResources::ColorGrading::lutData.lut.idxSrv
		: BindlessIndex{ -1, -1, -1, -1 };

	// Tonemap resolve dispatcher (deferred path에서만 draw; SceneColorHDR -> 백버퍼 LDR)
	auto tonemapPipelineDispatcher = TonemapPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at("DefaultRootSignature"), shaders_.at("TonemapResolveShader"),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx],
		&fenceToSignal, &resourcesTonemapPipeline_, &cmdListPool_,
		sceneColorSrv, sceneColorRoomIdx,
		tonemapExposure_, bloomIntensity_, gBufferDebugMode_,
		SharedResources::Bloom::mip0Srv(sceneColorRoomIdx),
		&srvTex3DPool_, colorGradingLUTSrv,
		heatParams, heatGB4
	);

	// Bloom dispatcher (deferred path only; runs before the resolve composite reads mip 0).
	auto bloomPipelineDispatcher = BloomPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at("DefaultRootSignature"),
		shaders_.at("BloomPrefilterShader"), shaders_.at("BloomDownsampleShader"), shaders_.at("BloomUpsampleShader"),
		submitter_.get(),
		&fenceToSignal, &resourcesBloomPipeline_, &cmdListPool_,
		sceneColorSrv, sceneColorRoomIdx, bloomThreshold_
	);

	auto billboardPipelineDispatcher = BillboardPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ),
		std::array<ComPtr<ID3D12PipelineState>, 4>{
			shaders_.at( "BillboardShader" ),           // BlendMode::Alpha = 0
			shaders_.at( "BillboardShaderAdditive" ),   // BlendMode::Additive = 1
			shaders_.at( "BillboardShaderMultiply" ),   // BlendMode::Multiply = 2
			shaders_.at( "BillboardShaderPremultiplied" ) // BlendMode::PremultipliedAlpha = 3
		},
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesBillboardPipeline_, threadPool_,
		&cmdListPool_, std::move( drawEventsBillboardPipeline_ ),
		cameraDataBillboardPipeline_, frameDataBillboardPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto meshParticleDispatcher = MeshParticlePipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ), shaders_.at( "MeshParticleShader" ),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesMeshParticlePipeline_, threadPool_,
		&cmdListPool_, std::move( drawEventsMeshParticlePipeline_ ),
		cameraDataMeshParticlePipeline_, frameDataMeshParticlePipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	// Energy orbs render into SceneColorHDR (so bloom catches them), not the backbuffer.
	const D3D12_CPU_DESCRIPTOR_HANDLE energyOrbRtv = !SharedResources::SceneColor::sceneColorData.empty()
		? SharedResources::SceneColor::sceneColorData[sceneColorRoomIdx].rtv
		: backBufferRtvs_[backbufIdx];
	auto energyOrbDispatcher = EnergyOrbPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ), shaders_.at( "EnergyOrbShader" ),
		submitter_.get(), viewport, clRect,
		energyOrbRtv, depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesEnergyOrbPipeline_, threadPool_,
		&cmdListPool_, std::move( drawEventsEnergyOrbPipeline_ ),
		cameraDataEnergyOrbPipeline_, frameDataEnergyOrbPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	// Heat-distortion glow: additive tinted halo into SceneColorHDR (before bloom, so
	// the tint glows). Depth-gated via GB4; the matching refraction warp is folded into
	// the tonemap resolve. Self-skips when there are no active heat sources.
	auto heatDistortionDispatcher = HeatDistortionPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ), shaders_.at( "HeatHazeShader" ),
		submitter_.get(), viewport, clRect,
		energyOrbRtv,
		&fenceToSignal, &resourcesHeatDistortion_, &cmdListPool_,
		&srvTex3DPool_, heatParams, heatGB4,
		sceneColorRoomIdx
	);

	// Path-guidance ribbons: additive trail into SceneColorHDR (before bloom, so it
	// glows). Independent resources from the post-resolve trail pass. Always additive,
	// so the HDR PSO is bound for both PSO slots.
	auto trailHDRDispatcher = TrailPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ),
		shaders_.at( "TrailShaderHDR" ),
		shaders_.at( "TrailShaderHDR" ),
		submitter_.get(), viewport, clRect,
		energyOrbRtv, depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesTrailPipelineHDR_, threadPool_,
		&cmdListPool_, std::move( drawEventsTrailPipelineHDR_ ),
		cameraDataTrailPipeline_, frameDataTrailPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto windRingDispatcher = WindRingPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ), shaders_.at( "WindRingShader" ),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesWindRingPipeline_, threadPool_,
		&cmdListPool_, std::move( drawEventsWindRingPipeline_ ),
		cameraDataWindRingPipeline_, frameDataWindRingPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto smokeBlendCGDispatcher = SmokeBlendCGPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ), shaders_.at( "SmokeBlendCGShader" ),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesSmokeBlendCGPipeline_, threadPool_,
		&cmdListPool_, std::move( drawEventsSmokeBlendCGPipeline_ ),
		cameraDataSmokeBlendCGPipeline_, frameDataSmokeBlendCGPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto blendCGMeshDispatcher = BlendCGMeshPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ), shaders_.at( "BlendCGMeshShader" ),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesBlendCGMeshPipeline_, threadPool_,
		&cmdListPool_, std::move( drawEventsBlendCGMeshPipeline_ ),
		cameraDataBlendCGMeshPipeline_, frameDataBlendCGMeshPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto piercingMeshDispatcher = PiercingMeshPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ), shaders_.at( "PiercingMeshShader" ),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesPiercingMeshPipeline_, threadPool_,
		&cmdListPool_, std::move( drawEventsPiercingMeshPipeline_ ),
		cameraDataPiercingMeshPipeline_, frameDataPiercingMeshPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto piercingSlashMeshDispatcher = PiercingSlashMeshPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ), shaders_.at( "PiercingSlashMeshShader" ),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesPiercingSlashMeshPipeline_, threadPool_,
		&cmdListPool_, std::move( drawEventsPiercingSlashMeshPipeline_ ),
		cameraDataPiercingSlashMeshPipeline_, frameDataPiercingSlashMeshPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto swordSlashDispatcher = SwordSlashPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ), shaders_.at( "SwordSlashShader" ),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesSwordSlashPipeline_, threadPool_,
		&cmdListPool_, std::move( drawEventsSwordSlashPipeline_ ),
		cameraDataSwordSlashPipeline_, frameDataSwordSlashPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto twoSidesDispatcher = TwoSidesPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ), shaders_.at( "TwoSidesShader" ),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesTwoSidesPipeline_, threadPool_,
		&cmdListPool_, std::move( drawEventsTwoSidesPipeline_ ),
		cameraDataTwoSidesPipeline_, frameDataTwoSidesPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto trailDispatcher = TrailPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at( "DefaultRootSignature" ),
		shaders_.at( "TrailShader" ),
		shaders_.at( "TrailShaderAdditive" ),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesTrailPipeline_, threadPool_,
		&cmdListPool_, std::move( drawEventsTrailPipeline_ ),
		cameraDataTrailPipeline_, frameDataTrailPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto bvPipelineDispatcher = BVPipeline::Dispatcher(
		tmpDescriptorHeaps, rootSigs_.at("DefaultRootSignature"),
		shaders_.at("BVShader"), submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesBVPipeline_, threadPool_, &cmdListPool_,
		std::move(drawEventsBVPipeline_), cameraDataBVPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto terrainPipelineDispatcher = TerrainPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at("DefaultRootSignature"),
		csmDebugVisualization_ ? shaders_.at("TerrainShaderCSMDebug") : shaders_.at("TerrainShader"),
		shaders_.at("TerrainShadowMapCSMShader"),
		&dsvPool_,
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesTerrainPipeline_,
		threadPool_, &cmdListPool_, std::move(drawEventsTerrainPipeline_),
		std::move(occluderInfosTerrain_), std::move(lightDataTerrainPipeline_),
		mainDirectionalLightTerrainPipeline_,
		cameraDataTerrainPipeline_, frameDataTerrainPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	auto terrainDeferredDispatcher = TerrainDeferredPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at("DefaultRootSignature"),
		shaders_.at("HiZOccluderShader"),
		shaders_.at("TerrainShadowMapCSMShader"),
		shaders_.at("TerrainDeferredGBufferShader"),
		&dsvPool_,
		submitter_.get(), viewport, clRect,
		&fenceToSignal, &resourcesTerrainDeferredPipeline_,
		threadPool_, &cmdListPool_,
		std::move(drawEventsTerrainDeferredPipeline_),
		std::move(occluderInfosTerrainDeferred_), mainDirectionalLightTerrainDeferredPipeline_,
		cameraDataTerrainDeferredPipeline_, frameDataTerrainDeferredPipeline_,
		frameIdx_ % backBuffers_.size()
	);

	auto pbrDeferredDispatcher = PBRDeferredPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_, &dsvPool_,
		rootSigs_.at("DefaultRootSignature"),
		cmdSig_,
		shaders_.at("HiZClearShader"),
		shaders_.at("HiZCullShader"),
		shaders_.at("PrefixSumShader"),
		shaders_.at("HiZCompactShader"),
		shaders_.at("HiZCommandShader"),
		shaders_.at("HiZOccluderShader"),
		shaders_.at("PBRDeferredGBufferShader"),
		shaders_.at("PBRDeferredIndirectGBufferShader"),
		shaders_.at("ShadowMapCSMShader"),
		shaders_.at("ShadowMapCSMMaskedShader"),
		submitter_.get(), viewport, clRect,
		&fenceToSignal, &resourcesPBRDeferredPipeline_, threadPool_, &cmdListPool_,
		std::move(drawEventsPBRDeferredPipeline_), std::move(occluderInfosPBRDeferred_),
		std::move(lightDataPBRDeferredPipeline_),
		mainDirectionalLightPBRDeferredPipeline_, cameraDataPBRDeferredPipeline_,
		frameDataPBRDeferredPipeline_,
		frameIdx_ % backBuffers_.size()
	);

	auto pbrDeferredSkinnedDispatcher = PBRDeferredSkinnedPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_, &dsvPool_,
		rootSigs_.at("DefaultRootSignature"),
		cmdSig_,
		shaders_.at("HiZClearShader"),
		shaders_.at("HiZCullShader"),
		shaders_.at("PrefixSumShader"),
		shaders_.at("HiZCompactShader"),
		shaders_.at("HiZCommandShader"),
		hiZCullEnabled_ ? shaders_.at("PBRDeferredSkinnedIndirectGBufferShader") : shaders_.at("PBRDeferredSkinnedGBufferShader"),
		shaders_.at("ShadowMapSkinnedCSMShader"),
		submitter_.get(), viewport, clRect,
		&fenceToSignal, &resourcesPBRDeferredSkinnedPipeline_, threadPool_, &cmdListPool_,
		std::move(drawEventsPBRDeferredSkinnedPipeline_), std::move(lightDataPBRDeferredSkinnedPipeline_),
		mainDirectionalLightPBRDeferredSkinnedPipeline_, cameraDataPBRDeferredSkinnedPipeline_,
		frameDataPBRDeferredSkinnedPipeline_,
		frameIdx_ % backBuffers_.size(),
		static_cast<u32t>(frameIdx_ % 2u)   // visibility ring slot (2-slot, backbuffer 수와 무관)
	);

	// UI Pipeline의 Dispatch
	auto uiPipelineDispatcher = UIPipeline::Dispatcher(
		tmpDescriptorHeaps,
		&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
		&samPool_, &cmpSamPool_,
		rootSigs_.at("DefaultRootSignature"), shaders_.at("UIShader"),
		submitter_.get(), viewport, clRect,
		backBufferRtvs_[backbufIdx], depthBufferDsvs_[backbufIdx],
		&fenceToSignal, &resourcesUIPipeline_, threadPool_,
		&cmdListPool_, std::move(drawEventsUIPipeline_),
		frameDataUIPipeline_,
		frameIdx_ % backBuffers_.size()	// room index
	);

	pbrDeferredDispatcher.sortDrawEvents();
	pbrDeferredSkinnedDispatcher.sortDrawEvents();

	// 의존 관계가 있는 파이프라인별 함수들 사이의 호출 순서는 중요하다.
	// 예를 들어 그림자를 지원하는 파이프라인들은
	// 각 파이프라인의 모든 그림자 패스를 수행한 후에 동기화되어
	// 각 파이프라인의 모든 메인 패스를 수행해야 한다.

	// ============================================================
	// Deferred rendering path
	// ============================================================
	if (renderPath_ == RenderPath::Deferred) {
		const auto roomIdx = frameIdx_ % backBuffers_.size();

		// Transition to DEPTH_WRITE and clear shadow maps for shadow pass
		// Clear hi-z map for occluder pass
		{
			CommandContext cmdCtxBarrier{};
			DISPLAY_ERROR_STR(cmdListPool_.allocOne(CommandListUsage::RenderingSlave, cmdCtxBarrier),
				"[GFX Error] GFX::render: no command list available.", false);
			if (!cmdCtxBarrier.cmdList) {
				return;
			}
			DISPLAY_ERROR_DX_HR(cmdCtxBarrier.cmdAlloc->Reset(), false);
			DISPLAY_ERROR_DX_HR(cmdCtxBarrier.cmdList->Reset(cmdCtxBarrier.cmdAlloc.Get(), nullptr), false);

			SharedResources::ShadowMap::getCSMAllReadyAsDepthWrite(
				std::string(SharedResources::ShadowMap::kDefaultKey), roomIdx, cmdCtxBarrier.cmdList.Get()
			);
			SharedResources::ShadowMap::clearCSMAllShadowMaps(
				std::string(SharedResources::ShadowMap::kDefaultKey), roomIdx, cmdCtxBarrier.cmdList.Get()
			);
			SharedResources::HiZMap::clearHiZMap(roomIdx, cmdListPool_, submitter_.get(), fenceToSignal);

			DISPLAY_ERROR_DX_HR(cmdCtxBarrier.cmdList->Close(), false);
			ID3D12CommandList* barrierCmds[] = { cmdCtxBarrier.cmdList.Get() };
			DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, barrierCmds), false);
			fenceToSignal.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtxBarrier));
		}

		if (hiZCullEnabled_) {
			// --- Occluder pass ---
			// Terrain + near-camera collidable props write depth into the shared Hi-Z
			// source before the pyramid is built, so both occlude all Hi-Z candidates.
			terrainDeferredDispatcher.occluderPass();
			pbrDeferredDispatcher.occluderPass();

			// --- Hi-Z map build pass ---
			{
				auto& root = rootSigs_.at("DefaultRootSignature");

				CommandContext cmdCtxHiZ{};
				DISPLAY_ERROR_STR(cmdListPool_.allocOne(CommandListUsage::RenderingSlave, cmdCtxHiZ),
					"[GFX Error] GFX::render: no command list available.", false);
				if (!cmdCtxHiZ.cmdList) {
					return;
				}

				auto cmdAlloc = cmdCtxHiZ.cmdAlloc.Get();
				auto cmdList = cmdCtxHiZ.cmdList.Get();

				DISPLAY_ERROR_DX_HR(cmdAlloc->Reset(), false);
				DISPLAY_ERROR_DX_HR(cmdList->Reset(cmdAlloc, nullptr), false);

				// === 명령 기록 ===
				DISPLAY_ERROR_DX_VOID(
					cmdList->SetComputeRootSignature(root->get()),
					false
				);

				auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>(tmpDescriptorHeaps.size());
				std::ranges::transform(tmpDescriptorHeaps, descriptorHeapsRaw.begin(),
					[](ComPtr<ID3D12DescriptorHeap>& comPtrHeap) { return comPtrHeap.Get(); }
				);
				DISPLAY_ERROR_DX_VOID( cmdList->SetDescriptorHeaps(
					static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()
				), false );
				DISPLAY_ERROR_DX_VOID( cmdList->SetPipelineState(shaders_.at("HiZMapShader").Get()), false );

				DISPLAY_ERROR_DX_VOID( cmdList->SetComputeRootDescriptorTable( root->paramIdx("SrcTex"),
					SharedResources::HiZMap::hiZMaps[roomIdx].srvHandle
				), false );


				// level 0 - copy
				copyTextureRegion(cmdList,
					D3D12_TEXTURE_COPY_LOCATION{
						.pResource = SharedResources::HiZMap::hiZMaps[roomIdx].srcTex.res.Get(),
						.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
						.SubresourceIndex = 0u
					},
					D3D12_TEXTURE_COPY_LOCATION{
						.pResource = SharedResources::HiZMap::hiZMaps[roomIdx].mips.res.Get(),
						.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
						.SubresourceIndex = 0u
					},
					D3D12_RESOURCE_STATE_DEPTH_WRITE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS
				);

				uavBarrier(cmdList, SharedResources::HiZMap::hiZMaps[roomIdx].mips.res.Get());

				// level n - compute
				auto prevWidth = SharedResources::HiZMap::hiZMaps[roomIdx].srcWidth;
				auto prevHeight = SharedResources::HiZMap::hiZMaps[roomIdx].srcHeight;

				for (u32t mipLevel = 1u; mipLevel < SharedResources::HiZMap::hiZMaps[roomIdx].mipLevelCnt; ++mipLevel) {
					cmdList->SetComputeRoot32BitConstant(
						root->paramIdx("FirstInstanceOffset"), mipLevel - 1u, 0u
					);

					DISPLAY_ERROR_DX_VOID(
						cmdList->SetComputeRootDescriptorTable( root->paramIdx("DestTex"),
							SharedResources::HiZMap::hiZMaps[roomIdx].uavHandles[mipLevel]
						),
						false
					);

					const auto width  = std::max(1u, prevWidth / 2u);
					const auto height = std::max(1u, prevHeight / 2u);

					DISPLAY_ERROR_DX_VOID(
						cmdList->Dispatch( static_cast<UINT>(ceil(width / 8.0f)), static_cast<UINT>(ceil(height / 8.0f)), 1 ),
						false
					);

					uavBarrier(cmdList, SharedResources::HiZMap::hiZMaps[roomIdx].mips.res.Get());

					prevWidth  = width;
					prevHeight = height;
				}

				// === 명령 기록 끝, 제출 및 실행 ===
				DISPLAY_ERROR_DX_HR(cmdList->Close(), false);
				ID3D12CommandList* cmds[] = { cmdList };
				DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, cmds), false);
				fenceToSignal.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtxHiZ));

				pbrDeferredSkinnedDispatcher.hiZPass();
				pbrDeferredDispatcher.hiZPass();
			}
		}

		// --- Shadow passes ---
		if (!threadPool_) {
			pbrDeferredDispatcher.shadowPass();
			pbrDeferredSkinnedDispatcher.shadowPass();
			terrainDeferredDispatcher.shadowPass();
		} else {
			pbrDeferredDispatcher.shadowPassMT();
			pbrDeferredSkinnedDispatcher.shadowPassMT();
			terrainDeferredDispatcher.shadowPassMT();
		}

		// --- GBuffer passes ---
		// PBRDeferred always draws its direct (non-occludee) events; occludee-candidate
		// props are drawn via the Hi-Z indirect path when culling is enabled.
		if (!threadPool_) {
			pbrDeferredDispatcher.gBufferPass();
			if (hiZCullEnabled_) {
				pbrDeferredDispatcher.gBufferIndirectPass();
				pbrDeferredSkinnedDispatcher.gBufferIndirectPass();
			} else
				pbrDeferredSkinnedDispatcher.gBufferPass();
			terrainDeferredDispatcher.gBufferPass();
		} else {
			pbrDeferredDispatcher.gBufferPassMT();
			if (hiZCullEnabled_) {
				pbrDeferredDispatcher.gBufferIndirectPassMT();
				pbrDeferredSkinnedDispatcher.gBufferIndirectPassMT();
			} else
				pbrDeferredSkinnedDispatcher.gBufferPassMT();
			terrainDeferredDispatcher.gBufferPassMT();
		}

		// --- Deferred Lighting pass ---
		// Transition Shadow maps to PIXEL_SHADER_RESOURCE and
		// transition GBuffer to PIXEL_SHADER_RESOURCE for the lighting pass
		{
			CommandContext cmdCtxBarrier{};
			DISPLAY_ERROR_STR(cmdListPool_.allocOne(CommandListUsage::RenderingSlave, cmdCtxBarrier),
				"[GFX Error] GFX::render deferred: no command list available.", false);
			if (cmdCtxBarrier.cmdList) {
				DISPLAY_ERROR_DX_HR(cmdCtxBarrier.cmdAlloc->Reset(), false);
				DISPLAY_ERROR_DX_HR(cmdCtxBarrier.cmdList->Reset(cmdCtxBarrier.cmdAlloc.Get(), nullptr), false);

				SharedResources::ShadowMap::getCSMAllReadyAsShaderResource(
					std::string(SharedResources::ShadowMap::kDefaultKey), roomIdx, cmdCtxBarrier.cmdList.Get()
				);
				SharedResources::GBuffer::transitionToRead(roomIdx, cmdCtxBarrier.cmdList.Get());

				DISPLAY_ERROR_DX_HR(cmdCtxBarrier.cmdList->Close(), false);
				ID3D12CommandList* barrierCmds[] = { cmdCtxBarrier.cmdList.Get() };
				DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, barrierCmds), false);
				fenceToSignal.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtxBarrier));
			}
		}

		// Lighting pass: fullscreen triangle, reads GBuffer SRVs, writes to backbuffer
		{
			const auto& gbData = SharedResources::GBuffer::gBufferData[roomIdx];
			const auto& csmData = SharedResources::ShadowMap::csmShadowMapData.at(
				std::string(SharedResources::ShadowMap::kDefaultKey))[roomIdx];

			// Build PerFrameData for deferred lighting
			PBRDeferredLightingShader::PerFrameData lpfd{};
			lpfd.globalAmbient = frameDataPBRDeferredPipeline_.globalAmbient.getXmf();
			lpfd.lightCnt      = static_cast<u32t>(lightDataPBRDeferredLighting_.size());
			lpfd.cascadeCount  = mainDirectionalLightPBRDeferredPipeline_.cascadeCount;
			for (u32t ci = 0u; ci < mainDirectionalLightPBRDeferredPipeline_.cascadeCount; ++ci)
				lpfd.idxShadowMap[ci] = csmData.cascades[ci].tex.idxSrv;
			lpfd.cascadeSplitsFarV = mainDirectionalLightPBRDeferredPipeline_.cascadeSplitsFarV;
			for (u32t i = 0u; i < mainDirectionalLightPBRDeferredPipeline_.cascadeCount; ++i)
				lpfd.lightVP[i] = mu::transpose(
					mainDirectionalLightPBRDeferredPipeline_.cascadeViews[i] * mainDirectionalLightPBRDeferredPipeline_.cascadeProjs[i]
				).getXmf();
			{
				const auto& o = mainDirectionalLightPBRDeferredPipeline_.cascadeNormalOffsets;
				lpfd.cascadeNormalOffsets = XMFLOAT4(o[0], o[1], o[2], o[3]);
			}
			// invView, invProj
			{
				const auto view = cameraDataPBRDeferredPipeline_.view;
				const auto proj = cameraDataPBRDeferredPipeline_.proj;
				lpfd.invView = mu::transpose(mu::inverse(view)).getXmf();
				lpfd.invProj = mu::transpose(mu::inverse(proj)).getXmf();
			}
			lpfd.idxGB0   = gbData.gb0.idxSrv;
			lpfd.idxGB1   = gbData.gb1.idxSrv;
			lpfd.idxGB2   = gbData.gb2.idxSrv;
			lpfd.idxGB3   = gbData.gb3.idxSrv;
			lpfd.idxDepth = gbData.depth.idxSrv;
			lpfd.idxGB4   = gbData.gb4.idxSrv;
			lpfd.debugMode = gBufferDebugMode_;
			lpfd.idxSkybox = skyboxIdxSrv;
			// IBL maps (generated at load by precomputeIBL)
			lpfd.idxIrradiance       = SharedResources::IBL::iblData.irradiance.idxSrv;
			lpfd.idxPrefiltered      = SharedResources::IBL::iblData.prefiltered.idxSrv;
			lpfd.idxBRDFLUT          = SharedResources::IBL::iblData.brdfLUT.idxSrv;
			lpfd.prefilteredMipCount = SharedResources::IBL::iblData.prefilteredMipCount;
			lpfd.iblIntensity        = 0.92f;
			lpfd.camPos = cameraDataPBRDeferredPipeline_.pos.getXmf();
			// TODO: 레벨의 특성에 맞게 fog 관련 값들은 런타임 수정이 필요
			lpfd.fogDensity = 0.0008f;
			lpfd.fogBaseHeight = 25.f;
			lpfd.heightFalloff = 0.024f;
			deferredLightingPerFrameData_.stage(roomIdx, &lpfd, 1u);

			// Build light data
			static auto lpLights = std::vector<PBRDeferredGBufferShader::Light>();
			lpLights.resize(lightDataPBRDeferredLighting_.size());
			const auto view = cameraDataPBRDeferredPipeline_.view;
			std::ranges::transform(lightDataPBRDeferredLighting_, lpLights.begin(),
				[view](const PBRDeferredPipeline::LightData& ld) {
					return PBRDeferredGBufferShader::Light{
						.color     = ld.color.getXmf(),
						.falloff   = ld.falloff,
						.posV      = mu::Vec3(mu::Vec4(ld.pos, 1.f) * view).getXmf(),
						.cosTheta  = ld.cosTheta,
						.dirV      = mu::NVec3(mu::Vec4(ld.dir, 0.f) * view).getXmf(),
						.cosPhi    = ld.cosPhi,
						.atten     = ld.atten.getXmf(),
						.intensity = ld.intensity,
						.type      = static_cast<int>(ld.type),
						.padding   = {}
					};
				});
			deferredLightingLightData_.stage(roomIdx, lpLights);
			lpLights.clear();
			lightDataPBRDeferredLighting_.clear();

			// Record lighting pass command list
			CommandContext cmdCtxLight{};
			CommandContext cmdCtxCopy{};
			DISPLAY_ERROR_STR(cmdListPool_.allocOne(CommandListUsage::RenderingSlave, cmdCtxLight),
				"[GFX Error] GFX::render deferred: no command list for lighting pass.", false);
			DISPLAY_ERROR_STR(cmdListPool_.allocOne(CommandListUsage::RenderingSlave, cmdCtxCopy),
				"[GFX Error] GFX::render deferred: no command list for lighting pass copy.", false);
			if (cmdCtxLight.cmdList) {
				auto* cl  = cmdCtxLight.cmdList.Get();
				auto* ca  = cmdCtxLight.cmdAlloc.Get();
				DISPLAY_ERROR_DX_HR(ca->Reset(), false);
				DISPLAY_ERROR_DX_HR(cl->Reset(ca, nullptr), false);

				const auto rootSig = rootSigs_.at("DefaultRootSignature");
				DISPLAY_ERROR_DX_VOID(cl->SetGraphicsRootSignature(rootSig->get()), false);
				DISPLAY_ERROR_DX_VOID(cl->SetPipelineState(shaders_.at("PBRDeferredLightingShader").Get()), false);
				// HDR: deferred lighting writes linear radiance into SceneColorHDR (tonemap resolve가 백버퍼로 합성).
				const D3D12_CPU_DESCRIPTOR_HANDLE deferredLitRtv = !SharedResources::SceneColor::sceneColorData.empty()
					? SharedResources::SceneColor::sceneColorData[roomIdx].rtv
					: backBufferRtvs_[backbufIdx];
				DISPLAY_ERROR_DX_VOID(cl->OMSetRenderTargets(1u, &deferredLitRtv, false, nullptr), false);
				DISPLAY_ERROR_DX_VOID(cl->RSSetViewports(1u, &viewport), false);
				DISPLAY_ERROR_DX_VOID(cl->RSSetScissorRects(1u, &clRect), false);

				auto heapsRaw = std::vector<ID3D12DescriptorHeap*>(tmpDescriptorHeaps.size());
				std::ranges::transform(tmpDescriptorHeaps, heapsRaw.begin(),
					[](ComPtr<ID3D12DescriptorHeap>& h) { return h.Get(); });
				DISPLAY_ERROR_DX_VOID(cl->SetDescriptorHeaps(static_cast<UINT>(heapsRaw.size()), heapsRaw.data()), false);

				const auto rootSigPtr = rootSigs_.at("DefaultRootSignature");
				srvTexPool_.bind(cl, rootSigPtr->paramIdx("TexturePool"));
				srvTexArrayPool_.bind(cl, rootSigPtr->paramIdx("TextureArrayPool"));
				srvTexCubePool_.bind(cl, rootSigPtr->paramIdx("TextureCubePool"));
				samPool_.bind(cl, rootSigPtr->paramIdx("SamplerPool"));
				cmpSamPool_.bind(cl, rootSigPtr->paramIdx("ComparisonSamplerPool"));

				deferredLightingPerFrameData_.bind(cl, rootSigPtr->paramIdx("PerFrameData"), roomIdx);
				deferredLightingLightData_.bind(cl, rootSigPtr->paramIdx("LightData"), roomIdx);

				DISPLAY_ERROR_DX_VOID(cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false);
				DISPLAY_ERROR_DX_VOID(cl->DrawInstanced(3u, 1u, 0u, 0u), false);  // fullscreen triangle

				auto hrClose = cl->Close();
				DISPLAY_ERROR_DX_HR(hrClose, false);

				if (hrClose >= 0) {
					ID3D12CommandList* staged[] = { cl };
					DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, staged), false);
				}

				DISPLAY_ERROR_DX_HR(cmdCtxCopy.cmdAlloc->Reset(), false);
				DISPLAY_ERROR_DX_HR(cmdCtxCopy.cmdList->Reset(cmdCtxCopy.cmdAlloc.Get(), nullptr), false);

				copyResource( cmdCtxCopy.cmdList.Get(),
					SharedResources::GBuffer::gBufferData[roomIdx].depth.res.Get(),
					depthBuffers_[backbufIdx].Get(),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_DEPTH_WRITE
				);

				auto hrClose2 = cmdCtxCopy.cmdList->Close();
				DISPLAY_ERROR_DX_HR(hrClose2, false);

				if (hrClose2 >= 0) {
					ID3D12CommandList* staged[] = { cmdCtxCopy.cmdList.Get() };
					DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, staged), false);
				}
				fenceToSignal.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtxLight));
				fenceToSignal.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtxCopy));
			}
		}

		// Energy orbs: additive HDR glow into SceneColorHDR (still a RENDER_TARGET at
		// this point), depth-tested against scene depth, BEFORE bloom so they glow.
		if (!SharedResources::SceneColor::sceneColorData.empty() && gBufferDebugMode_ == 0u) {
			// Skybox into SceneColorHDR FIRST (background only, depth-tested), so the additive
			// glows + bloom + heat distortion warp all apply over the sky and survive to the
			// resolve. The resolve passes background pixels through untonemapped, preserving the
			// sky's authored look. (Forward/lobby path still draws the skybox to the backbuffer.)
			skyboxPipelineDispatcher.updateGPUDataSingleThreaded();
			skyboxPipelineDispatcher.drawSingleThreaded();
			energyOrbDispatcher.updateGPUDataSingleThreaded();
			energyOrbDispatcher.drawSingleThreaded();
			// Path-guidance ribbons glow: additive HDR trail into SceneColorHDR, before bloom.
			trailHDRDispatcher.updateGPUDataSingleThreaded();
			trailHDRDispatcher.drawSingleThreaded();
			// Boss heat-distortion glow: additive tinted halo into SceneColorHDR, before bloom.
			heatDistortionDispatcher.updateGPUDataSingleThreaded();
			heatDistortionDispatcher.drawSingleThreaded();
		}

		// HDR tonemap resolve: SceneColorHDR(deferred lighting 출력) -> 백버퍼(LDR).
		// deferred lighting 직후 수행. 이후 skybox/오버레이는 백버퍼에 그린다.
		if (!SharedResources::SceneColor::sceneColorData.empty()) {
			CommandContext cmdCtxResolveBarrier{};
			if (cmdListPool_.allocOne(CommandListUsage::RenderingSlave, cmdCtxResolveBarrier) && cmdCtxResolveBarrier.cmdList) {
				DISPLAY_ERROR_DX_HR(cmdCtxResolveBarrier.cmdAlloc->Reset(), false);
				DISPLAY_ERROR_DX_HR(cmdCtxResolveBarrier.cmdList->Reset(cmdCtxResolveBarrier.cmdAlloc.Get(), nullptr), false);
				SharedResources::SceneColor::transitionToRead(roomIdx, cmdCtxResolveBarrier.cmdList.Get());
				DISPLAY_ERROR_DX_HR(cmdCtxResolveBarrier.cmdList->Close(), false);
				ID3D12CommandList* resolveBarrierCmds[] = { cmdCtxResolveBarrier.cmdList.Get() };
				DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, resolveBarrierCmds), false);
				fenceToSignal.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtxResolveBarrier));
			}
			// Bloom: reads SceneColorHDR (now PIXEL_SHADER_RESOURCE), writes the bloom mip
			// chain (mip 0 composited by the resolve). Skipped in debug views (passthrough).
			if (gBufferDebugMode_ == 0u) {
				bloomPipelineDispatcher.render();
			}
			tonemapPipelineDispatcher.updateGPUDataSingleThreaded();
			tonemapPipelineDispatcher.drawSingleThreaded();
		}

		// Forward-always 패스: BV·파티클. resolve 이후 백버퍼에 그린다.
		// (Deferred 경로의 skybox는 위 6b에서 SceneColorHDR에 합성 후 resolve가 배경 패스스루로
		//  내보내므로 여기서 다시 그리지 않는다 — heat distortion/bloom이 하늘에도 적용되게 함.)
		bvPipelineDispatcher.updateGPUDataSingleThreaded();
		bvPipelineDispatcher.drawSingleThreaded();

		billboardPipelineDispatcher.updateGPUDataSingleThreaded();
		billboardPipelineDispatcher.drawSingleThreaded();

		smokeBlendCGDispatcher.updateGPUDataSingleThreaded();
		smokeBlendCGDispatcher.drawSingleThreaded();

		blendCGMeshDispatcher.updateGPUDataSingleThreaded();
		blendCGMeshDispatcher.drawSingleThreaded();

		piercingMeshDispatcher.updateGPUDataSingleThreaded();
		piercingMeshDispatcher.drawSingleThreaded();

		piercingSlashMeshDispatcher.updateGPUDataSingleThreaded();
		piercingSlashMeshDispatcher.drawSingleThreaded();

		meshParticleDispatcher.updateGPUDataSingleThreaded();
		meshParticleDispatcher.drawSingleThreaded();

		windRingDispatcher.updateGPUDataSingleThreaded();
		windRingDispatcher.drawSingleThreaded();

		swordSlashDispatcher.updateGPUDataSingleThreaded();
		swordSlashDispatcher.drawSingleThreaded();

		twoSidesDispatcher.updateGPUDataSingleThreaded();
		twoSidesDispatcher.drawSingleThreaded();

		trailDispatcher.updateGPUDataSingleThreaded();
		trailDispatcher.drawSingleThreaded();
		dumpLog();
	}

	// ============================================================
	// Forward rendering path
	// ============================================================
	if (renderPath_ == RenderPath::Forward) {
		const auto roomIdx = frameIdx_ % backBuffers_.size();

		// Transition to DEPTH_WRITE and clear shadow maps for shadow pass
		// Clear hi-z map for occluder pass
		{
			CommandContext cmdCtxBarrier{};
			DISPLAY_ERROR_STR(cmdListPool_.allocOne(CommandListUsage::RenderingSlave, cmdCtxBarrier),
				"[GFX Error] GFX::render forward: no command list available.", false);
			if (!cmdCtxBarrier.cmdList) {
				return;
			}
			DISPLAY_ERROR_DX_HR(cmdCtxBarrier.cmdAlloc->Reset(), false);
			DISPLAY_ERROR_DX_HR(cmdCtxBarrier.cmdList->Reset(cmdCtxBarrier.cmdAlloc.Get(), nullptr), false);

			SharedResources::ShadowMap::getCSMAllReadyAsDepthWrite(
				std::string(SharedResources::ShadowMap::kDefaultKey), roomIdx, cmdCtxBarrier.cmdList.Get()
			);
			SharedResources::ShadowMap::clearCSMAllShadowMaps(
				std::string(SharedResources::ShadowMap::kDefaultKey), roomIdx, cmdCtxBarrier.cmdList.Get()
			);
			SharedResources::HiZMap::clearHiZMap(roomIdx, cmdListPool_, submitter_.get(), fenceToSignal);

			DISPLAY_ERROR_DX_HR(cmdCtxBarrier.cmdList->Close(), false);
			ID3D12CommandList* barrierCmds[] = { cmdCtxBarrier.cmdList.Get() };
			DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, barrierCmds), false);
			fenceToSignal.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cmdCtxBarrier));
		}

		// threadPool_이 활성화된 경우엔 멀티스레드로 처리한다.
		if (!threadPool_) {
			// == occluder 패스 ==
			terrainPipelineDispatcher.occluderPass();

			samplePipelineDispatcher.updateGPUDataSingleThreaded();
			samplePipelineDispatcher.drawSingleThreaded();
			dumpLog();

			// == 그림자 패스들 ==
			pbrPipelineDispatcher.sortDrawEvents();
			pbrPipelineDispatcher.shadowPass();
			dumpLog();

			pbrSkinnedPipelineDispatcher.sortDrawEvents();
			pbrSkinnedPipelineDispatcher.shadowPass();
			dumpLog();

			terrainPipelineDispatcher.shadowPass();
			dumpLog();

			// == 메인 패스들 ==
			SharedResources::ShadowMap::getCSMAllReadyAsShaderResource(
				std::string(SharedResources::ShadowMap::kDefaultKey), frameIdx_ % backBuffers_.size(), cmdListPool_, submitter_.get(), fenceToSignal
			);

			pbrPipelineDispatcher.mainPass();
			dumpLog();
			pbrSkinnedPipelineDispatcher.mainPass();
			dumpLog();

			terrainPipelineDispatcher.mainPass();
			dumpLog();

			bvPipelineDispatcher.updateGPUDataSingleThreaded();
			bvPipelineDispatcher.drawSingleThreaded();
			dumpLog();

			skyboxPipelineDispatcher.updateGPUDataSingleThreaded();
			skyboxPipelineDispatcher.drawSingleThreaded();

			billboardPipelineDispatcher.updateGPUDataSingleThreaded();
			billboardPipelineDispatcher.drawSingleThreaded();

			smokeBlendCGDispatcher.updateGPUDataMultiThreaded();
			smokeBlendCGDispatcher.drawMultiThreaded();

			meshParticleDispatcher.updateGPUDataMultiThreaded();
			meshParticleDispatcher.drawMultiThreaded();

			windRingDispatcher.updateGPUDataMultiThreaded();
			windRingDispatcher.drawMultiThreaded();

			swordSlashDispatcher.updateGPUDataMultiThreaded();
			swordSlashDispatcher.drawMultiThreaded();

			blendCGMeshDispatcher.updateGPUDataMultiThreaded();
			blendCGMeshDispatcher.drawMultiThreaded();

			piercingMeshDispatcher.updateGPUDataMultiThreaded();
			piercingMeshDispatcher.drawMultiThreaded();

			piercingSlashMeshDispatcher.updateGPUDataMultiThreaded();
			piercingSlashMeshDispatcher.drawMultiThreaded();

			twoSidesDispatcher.updateGPUDataMultiThreaded();
			twoSidesDispatcher.drawMultiThreaded();

			trailDispatcher.updateGPUDataMultiThreaded();
			trailDispatcher.drawMultiThreaded();
			dumpLog();
		}
		else {
			// == occluder 패스 ==
			terrainPipelineDispatcher.occluderPass();

			samplePipelineDispatcher.updateGPUDataMultiThreaded();
			samplePipelineDispatcher.drawMultiThreaded();
			dumpLog();

			// == 그림자 패스들 ==
			pbrPipelineDispatcher.sortDrawEvents();
			pbrPipelineDispatcher.shadowPassMT();
			dumpLog();

			pbrSkinnedPipelineDispatcher.sortDrawEvents();
			pbrSkinnedPipelineDispatcher.shadowPassMT();
			dumpLog();

			terrainPipelineDispatcher.shadowPassMT();
			dumpLog();

			// == 메인 패스들 ==
			SharedResources::ShadowMap::getCSMAllReadyAsShaderResource(
				std::string(SharedResources::ShadowMap::kDefaultKey), frameIdx_ % backBuffers_.size(), cmdListPool_, submitter_.get(), fenceToSignal
			);

			pbrPipelineDispatcher.mainPassMT();
			dumpLog();
			pbrSkinnedPipelineDispatcher.mainPassMT();
			dumpLog();

			terrainPipelineDispatcher.mainPassMT();
			dumpLog();

			bvPipelineDispatcher.updateGPUDataMultiThreaded();
			bvPipelineDispatcher.drawMultiThreaded();
			dumpLog();

			skyboxPipelineDispatcher.updateGPUDataSingleThreaded();
			skyboxPipelineDispatcher.drawSingleThreaded();

			billboardPipelineDispatcher.updateGPUDataMultiThreaded();
			billboardPipelineDispatcher.drawMultiThreaded();

			smokeBlendCGDispatcher.updateGPUDataMultiThreaded();
			smokeBlendCGDispatcher.drawMultiThreaded();

			meshParticleDispatcher.updateGPUDataMultiThreaded();
			meshParticleDispatcher.drawMultiThreaded();

			windRingDispatcher.updateGPUDataMultiThreaded();
			windRingDispatcher.drawMultiThreaded();

			swordSlashDispatcher.updateGPUDataMultiThreaded();
			swordSlashDispatcher.drawMultiThreaded();

			blendCGMeshDispatcher.updateGPUDataMultiThreaded();
			blendCGMeshDispatcher.drawMultiThreaded();

			piercingMeshDispatcher.updateGPUDataMultiThreaded();
			piercingMeshDispatcher.drawMultiThreaded();

			piercingSlashMeshDispatcher.updateGPUDataMultiThreaded();
			piercingSlashMeshDispatcher.drawMultiThreaded();

			twoSidesDispatcher.updateGPUDataMultiThreaded();
			twoSidesDispatcher.drawMultiThreaded();

			trailDispatcher.updateGPUDataMultiThreaded();
			trailDispatcher.drawMultiThreaded();

			dumpLog();
		}
	}

	// ===== 로비 대기실 슬롯 포트레이트 패스 (오프스크린 RT → UI 합성) =====
	// 대기실 활성 동안 수행. CSM 상태와 무관(cascadeCount=0 + 셰이더 가드). UI 디스패치 직전 배치.
	// 채워진 슬롯이 0개여도 clear+transitionToRead는 수행 → 전 슬롯 비는 전환 프레임 잔상 방지.
	if (lobbyPortraitActive_ && !SharedResources::Portrait::portraitData.empty()) {
		const auto roomIdx = frameIdx_ % backBuffers_.size();
		const auto& pData = SharedResources::Portrait::portraitData[roomIdx];

		// (1) color RT를 RENDER_TARGET으로 전환 + 투명 클리어 (depth=1)
		{
			CommandContext cc{};
			if (cmdListPool_.allocOne(CommandListUsage::RenderingSlave, cc) && cc.cmdList) {
				DISPLAY_ERROR_DX_HR(cc.cmdAlloc->Reset(), false);
				DISPLAY_ERROR_DX_HR(cc.cmdList->Reset(cc.cmdAlloc.Get(), nullptr), false);
				SharedResources::Portrait::transitionToWrite(roomIdx, cc.cmdList.Get());
				SharedResources::Portrait::clearPortraitRT(roomIdx, cc.cmdList.Get());
				DISPLAY_ERROR_DX_HR(cc.cmdList->Close(), false);
				ID3D12CommandList* cls[] = { cc.cmdList.Get() };
				submitter_->submit(1u, cls);
				fenceToSignal.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cc));
			}
		}

		// (2) 슬롯별 캐릭터 그리기 (셀 viewport + 전용 카메라/Resources). shadow off.
		mainDirectionalLightLobbyPortrait_.cascadeCount = 0u;
		for (u32t s = 0u; s < kMaxPortraitSlots; ++s) {
			if (drawEventsLobbyPortrait_[s].empty() && drawEventsLobbyPortraitStatic_[s].empty()) continue;

			const auto cellViewport = D3D12_VIEWPORT{
				.TopLeftX = static_cast<FLOAT>(s * kPortraitCellW),
				.TopLeftY = 0.f,
				.Width    = static_cast<FLOAT>(kPortraitCellW),
				.Height   = static_cast<FLOAT>(kPortraitCellH),
				.MinDepth = 0.f,
				.MaxDepth = 1.f
			};
			const auto cellScissor = D3D12_RECT{
				.left   = static_cast<LONG>(s * kPortraitCellW),
				.top    = 0,
				.right  = static_cast<LONG>((s + 1u) * kPortraitCellW),
				.bottom = static_cast<LONG>(kPortraitCellH)
			};

			if (!drawEventsLobbyPortrait_[s].empty()) {
				auto portraitDispatcher = PBRSkinnedPipeline::Dispatcher(
					tmpDescriptorHeaps,
					&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
					&samPool_, &cmpSamPool_, &dsvPool_,
					rootSigs_.at("DefaultRootSignature"), shaders_.at("PBRSkinnedShader"),
					shaders_.at("ShadowMapSkinnedCSMShader"), submitter_.get(), cellViewport, cellScissor,
					pData.rtv, pData.dsv,
					&fenceToSignal, &resourcesLobbyPortrait_[s], threadPool_, &cmdListPool_,
					std::move(drawEventsLobbyPortrait_[s]),
					std::vector<PBRSkinnedPipeline::LightData>(lightDataLobbyPortrait_),	// 슬롯별 복사
					mainDirectionalLightLobbyPortrait_, cameraDataLobbyPortrait_[s], frameDataLobbyPortrait_,
					roomIdx
				);
				portraitDispatcher.sortDrawEvents();
				portraitDispatcher.mainPass();	// shadowPass 미호출
			}

			// 장착 무기(non-skinned). 카메라/조명/프레임 데이터는 스킨드 포트레이트 값을
			// PBRPipeline 타입으로 변환해 그대로 재사용한다(필드 구성 동일).
			if (!drawEventsLobbyPortraitStatic_[s].empty()) {
				const auto toStaticLight = [](const PBRSkinnedPipeline::LightData& l) {
					return PBRPipeline::LightData{
						.pos = l.pos, .dir = l.dir, .color = l.color, .intensity = l.intensity,
						.cosTheta = l.cosTheta, .cosPhi = l.cosPhi, .falloff = l.falloff, .atten = l.atten,
						.type = static_cast<PBRPipeline::LightData::Type>(l.type),
						.isMainDirectionalLight = l.isMainDirectionalLight,
						.cascadeViews = l.cascadeViews, .cascadeProjs = l.cascadeProjs,
						.cascadeSplitsFarV = l.cascadeSplitsFarV, .cascadeCount = l.cascadeCount,
						.cascadeNormalOffsets = l.cascadeNormalOffsets, .cascadeCameraPos = l.cascadeCameraPos
					};
				};
				std::vector<PBRPipeline::LightData> staticLights{};
				staticLights.reserve(lightDataLobbyPortrait_.size());
				for (const auto& l : lightDataLobbyPortrait_) staticLights.push_back(toStaticLight(l));

				const auto staticCamera = PBRPipeline::CameraData{
					.view = cameraDataLobbyPortrait_[s].view,
					.proj = cameraDataLobbyPortrait_[s].proj,
					.pos  = cameraDataLobbyPortrait_[s].pos
				};
				const auto staticFrame = PBRPipeline::FrameData{
					.globalAmbient = frameDataLobbyPortrait_.globalAmbient,
					.iblIntensity  = frameDataLobbyPortrait_.iblIntensity
				};

				auto portraitStaticDispatcher = PBRPipeline::Dispatcher(
					tmpDescriptorHeaps,
					&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
					&samPool_, &cmpSamPool_, &dsvPool_,
					rootSigs_.at("DefaultRootSignature"), shaders_.at("PBRShader"),
					shaders_.at("ShadowMapCSMShader"), submitter_.get(), cellViewport, cellScissor,
					pData.rtv, pData.dsv,
					&fenceToSignal, &resourcesLobbyPortraitStatic_[s], threadPool_, &cmdListPool_,
					std::move(drawEventsLobbyPortraitStatic_[s]),
					std::move(staticLights),
					toStaticLight(mainDirectionalLightLobbyPortrait_), staticCamera, staticFrame,
					roomIdx
				);
				portraitStaticDispatcher.sortDrawEvents();
				portraitStaticDispatcher.mainPass();	// shadowPass 미호출
			}
		}
		dumpLog();

		// (3) color RT를 PIXEL_SHADER_RESOURCE로 전환 (UI 샘플용)
		{
			CommandContext cc{};
			if (cmdListPool_.allocOne(CommandListUsage::RenderingSlave, cc) && cc.cmdList) {
				DISPLAY_ERROR_DX_HR(cc.cmdAlloc->Reset(), false);
				DISPLAY_ERROR_DX_HR(cc.cmdList->Reset(cc.cmdAlloc.Get(), nullptr), false);
				SharedResources::Portrait::transitionToRead(roomIdx, cc.cmdList.Get());
				DISPLAY_ERROR_DX_HR(cc.cmdList->Close(), false);
				ID3D12CommandList* cls[] = { cc.cmdList.Get() };
				submitter_->submit(1u, cls);
				fenceToSignal.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cc));
			}
		}

		// (4) per-frame 채널 clear (매 프레임 add → 누적 방지). draw event는 move로 비워졌다.
		lightDataLobbyPortrait_.clear();
		for (auto& dq : drawEventsLobbyPortrait_) dq.clear();
		for (auto& dq : drawEventsLobbyPortraitStatic_) dq.clear();
	}

	// ===== 미니맵 배경 캐시 재굽기 (재굽기 요청 프레임에만; 단일 RT라 1프레임으로 충분) =====
	if (minimapRebakeRequested_ && SharedResources::Minimap::created) {
		// per-room CB용 인덱스(RT는 단일). 재굽기는 드물어 CB는 cyclic 재사용으로 충분.
		const auto roomIdx = frameIdx_ % backBuffers_.size();
		auto& m = SharedResources::Minimap::minimapData;
		const auto vp = D3D12_VIEWPORT{ 0.f, 0.f,
			static_cast<float>(kMinimapRTSize), static_cast<float>(kMinimapRTSize), 0.f, 1.f };
		const auto sc = D3D12_RECT{ 0, 0,
			static_cast<LONG>(kMinimapRTSize), static_cast<LONG>(kMinimapRTSize) };

		// (1) texA를 RENDER_TARGET으로 전환 + 투명 클리어.
		{
			CommandContext cc{};
			if (cmdListPool_.allocOne(CommandListUsage::RenderingSlave, cc) && cc.cmdList) {
				DISPLAY_ERROR_DX_HR(cc.cmdAlloc->Reset(), false);
				DISPLAY_ERROR_DX_HR(cc.cmdList->Reset(cc.cmdAlloc.Get(), nullptr), false);
				SharedResources::Minimap::transitionToWrite(/*useA=*/true, cc.cmdList.Get());
				SharedResources::Minimap::clearMinimapRT(/*useA=*/true, cc.cmdList.Get());
				DISPLAY_ERROR_DX_HR(cc.cmdList->Close(), false);
				ID3D12CommandList* cls[] = { cc.cmdList.Get() };
				submitter_->submit(1u, cls);
				fenceToSignal.associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].push_back(std::move(cc));
			}
		}

		// (2) 로드된 청크들을 texA에 그린다 (diffuse splat-blend, alpha=1 = 로드된 영역 마스크).
		{
			auto minimapTerrainDispatcher = MinimapTerrainPipeline::Dispatcher(
				tmpDescriptorHeaps,
				&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
				&samPool_, &cmpSamPool_,
				rootSigs_.at("DefaultRootSignature"), shaders_.at("MinimapTerrainShader"),
				submitter_.get(), vp, sc, m.rtvA,
				&fenceToSignal, &resourcesMinimapTerrain_, &cmdListPool_,
				std::move(drawEventsMinimap_), cameraDataMinimap_, roomIdx
			);
			minimapTerrainDispatcher.render();
		}
		dumpLog();

		// (2b) scatter prop(나무/바위)을 같은 texA에 top-down albedo로 굽힘(지형 위에 겹쳐 그림).
		if (!drawEventsMinimapProp_.empty()) {
			auto minimapPropDispatcher = MinimapPropPipeline::Dispatcher(
				tmpDescriptorHeaps,
				&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
				&samPool_, &cmpSamPool_,
				rootSigs_.at("DefaultRootSignature"), shaders_.at("MinimapPropShader"),
				submitter_.get(), vp, sc, m.rtvA,
				&fenceToSignal, &resourcesMinimapProp_, &cmdListPool_,
				std::move(drawEventsMinimapProp_),
				cameraDataMinimap_.view * cameraDataMinimap_.proj, roomIdx
			);
			minimapPropDispatcher.render();
			dumpLog();
		}

		// (3) fog-of-war 블러 2-pass + 합성 (texA<->texB 핑퐁, 최종 결과는 texA에 남는다).
		{
			auto minimapFogBlurDispatcher = MinimapFogBlurPipeline::Dispatcher(
				tmpDescriptorHeaps,
				&srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_,
				&samPool_, &cmpSamPool_,
				rootSigs_.at("DefaultRootSignature"), shaders_.at("MinimapFogBlurShader"),
				submitter_.get(), &fenceToSignal, &resourcesMinimapFogBlur_, &cmdListPool_,
				roomIdx, kMinimapFogBlurRadiusTexels
			);
			minimapFogBlurDispatcher.render();
		}
		dumpLog();

		minimapRebakeRequested_ = false;
		drawEventsMinimap_.clear();
		drawEventsMinimapProp_.clear();
	}

	// Ui Pipeline의 rendering
	if ( !threadPool_ ) {
		uiPipelineDispatcher.updateGPUDataSingleThreaded();
		uiPipelineDispatcher.drawSingleThreaded();
		dumpLog();
	}
	else {
		uiPipelineDispatcher.updateGPUDataMultiThreaded();
		uiPipelineDispatcher.drawMultiThreaded();
		dumpLog();
	}

	// 출력 명령 컨텍스트 할당
	CommandContext cmdCtxPresent{};
	DISPLAY_ERROR_STR(
		cmdListPool_.allocOne(CommandListUsage::RenderingMaster, cmdCtxPresent),
		"[GFX Error] GFX::render: 사용 가능한 명령 리스트가 없습니다. "
		"CommandListPool::init 호출이 이루어지지 않았거나, 할당받은 명령 리스트가 반납되지 않았습니다.",
		false
	);
	auto cmdListPresent = cmdCtxPresent.cmdList.Get();
	auto cmdAllocPresent = cmdCtxPresent.cmdAlloc.Get();

	if (!cmdListPresent) {
		return;
	}

	DISPLAY_ERROR_DX_HR( cmdAllocPresent->Reset(), false );
	DISPLAY_ERROR_DX_HR( cmdListPresent->Reset(cmdAllocPresent, nullptr), false );

	// 출력 명령 기록 시작
	transitionResourceState(cmdListPresent, backBuffers_[backbufIdx].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT
	);

	// 출력 명령 기록 끝
	DISPLAY_ERROR_DX_VOID( cmdListPresent->Close(), false );

	// 출력 명령 리스트 실행
	ID3D12CommandList* presentCmdLists[] = { cmdListPresent };
	DISPLAY_ERROR_DX_VOID( submitter_->submit(1u, presentCmdLists), false );

	
	DISPLAY_ERROR_DX_VOID( submitter_->present(swapChain_.Get(), vsyncEnabled_ ? 1u : 0u), false );

	
	fences_.at(fenceNameToSignal).associatedCmdCtxs_[etoi(CommandListUsage::RenderingMaster)]
		.push_back(std::move(cmdCtxClear));
	fences_.at(fenceNameToSignal).associatedCmdCtxs_[etoi(CommandListUsage::RenderingMaster)]
		.push_back(std::move(cmdCtxPresent));
	signalFence("FrameFence" + std::to_string(idxFenceToSignal));

	// 프레임 인덱스 갱신
	++frameIdx_;
}

bool GFX::WriteTextToBitmap( TextImage* pDestImage, UINT DestWidth, UINT DestHeight, UINT DestPitch, int* piOutWidth, int* piOutHeight, void* pFontObjHandle, const WCHAR* wchString, DWORD dwLen, D2D1_COLOR_F color )
{
	FontHandle* pFont = pFontObjHandle ? static_cast<FontHandle*>( pFontObjHandle ) : &tahomaFont_;
	return font_.WriteTextToBitmap( pDestImage, DestWidth, DestHeight, DestPitch, piOutWidth, piOutHeight, pFont, wchString, dwLen, color );
}

FontHandle GFX::createFont( float fontSize )
{
	return font_.CreateFontObject( L"Tahoma", fontSize );
}

FontHandle GFX::createFont( const WCHAR* fontFamilyName, float fontSize )
{
	return font_.CreateFontObject(
		(fontFamilyName && fontFamilyName[0] != L'\0') ? fontFamilyName : L"Tahoma",
		fontSize
	);
}

void GFX::measureText( FontHandle* pFont, const WCHAR* str, DWORD len, float maxW, float maxH, int* outW, int* outH )
{
	font_.measureText( pFont, str, len, maxW, maxH, outW, outH );
}

void GFX::UpdateTextureWithTextImage( TextImage* srcImage, UINT srcWidth, UINT srcHeight )
{
	ID3D12Resource* pDestTexResource = srcImage->texture.res.Get();
	ID3D12Resource* pUploadBuffer = srcImage->textureUpload.res.Get();

	D3D12_RESOURCE_DESC Desc = pDestTexResource->GetDesc();
	// 크기 불일치/Map 실패는 이 업데이트만 건너뛴다(텍스처는 이전 내용 유지).
	// 종료시키면 std::exit의 정적 소멸 순서 문제로 2차 크래시가 난다.
	if ( srcWidth > Desc.Width )
	{
		DISPLAY_ERROR_STR( false, "[GFX Error] GFX::UpdateTextureWithTextImage: 소스 이미지의 너비가 대상 텍스처의 너비보다 큽니다.", false );
		return;
	}
	if ( srcHeight > Desc.Height )
	{
		DISPLAY_ERROR_STR( false, "[GFX Error] GFX::UpdateTextureWithTextImage: 소스 이미지의 높이가 대상 텍스처의 높이보다 큽니다.", false );
		return;
	}
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
	UINT	Rows = 0;
	UINT64	RowSize = 0;
	UINT64	TotalBytes = 0;

	device_->GetCopyableFootprints( &Desc, 0, 1, 0, &Footprint, &Rows, &RowSize, &TotalBytes );

	BYTE* pMappedPtr = nullptr;
	CD3DX12_RANGE readRange( 0, 0 );

	HRESULT hrMap = pUploadBuffer->Map( 0, &readRange, reinterpret_cast<void**>(&pMappedPtr) );
	DISPLAY_ERROR_DX_HR( hrMap, false );
	if ( FAILED( hrMap ) || !pMappedPtr )
	{
		return;
	}

	const BYTE* pSrc = srcImage->pData.data();

	BYTE* pDest = pMappedPtr;
	for ( UINT y = 0; y < srcHeight; y++ )
	{
		memcpy( pDest, pSrc, srcWidth * 4 );
		pSrc += (srcWidth * 4);
		pDest += Footprint.Footprint.RowPitch;
	}
	// Unmap
	pUploadBuffer->Unmap( 0, nullptr );
}

void GFX::createTextImageImmediate(UINT width, UINT height, TextImage* pDest) {
	*pDest = TextImage(device_.Get(), width, height, srvTexPool_);
}

// 공용 샘플러들 생성
// gfxUtil.hpp의 Samplers enum과 인덱스를 맞춰주어야 한다.
void GFX::createSamplers() {
	// 0 - Nearest Wrap
	auto samDesc = D3D12_SAMPLER_DESC{
		.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .MipLODBias = 0.f,
        .MaxAnisotropy = 0u,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
        .BorderColor = { 0.f, 0.f, 0.f, 1.f },
        .MinLOD = 0.f,
        .MaxLOD = std::numeric_limits<float>::max()
	};
	auto idx = samPool_.alloc();
	auto handle = samPool_.cpuHandle(idx);
	device_->CreateSampler(&samDesc, handle);

	// 1 - Bilinear Wrap
	samDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	idx = samPool_.alloc();
	handle = samPool_.cpuHandle(idx);
	device_->CreateSampler(&samDesc, handle);

	// 2 - Trilinear Wrap
	samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	idx = samPool_.alloc();
	handle = samPool_.cpuHandle(idx);
	device_->CreateSampler(&samDesc, handle);

	// 3 - Nearest Border
	samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	idx = samPool_.alloc();
	handle = samPool_.cpuHandle(idx);
	device_->CreateSampler(&samDesc, handle);

	// 4 - Bilinear Border
	samDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	idx = samPool_.alloc();
	handle = samPool_.cpuHandle(idx);
	device_->CreateSampler(&samDesc, handle);

	// 5 - Trilinear Border
	samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	idx = samPool_.alloc();
	handle = samPool_.cpuHandle(idx);
	device_->CreateSampler(&samDesc, handle);

	// 6 - Nearest Clamp
	samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	idx = samPool_.alloc();
	handle = samPool_.cpuHandle(idx);
	device_->CreateSampler(&samDesc, handle);

	// 7 - Bilinear Clamp
	samDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	idx = samPool_.alloc();
	handle = samPool_.cpuHandle(idx);
	device_->CreateSampler(&samDesc, handle);

	// 8 - Trilinear Clamp
	samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	idx = samPool_.alloc();
	handle = samPool_.cpuHandle(idx);
	device_->CreateSampler(&samDesc, handle);

	// 9 - Nearest Comparison
	samDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
	samDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	samDesc.BorderColor[0] = 1.f;
	samDesc.BorderColor[1] = 1.f;
	samDesc.BorderColor[2] = 1.f;
	samDesc.BorderColor[3] = 1.f;
	idx = cmpSamPool_.alloc();
	handle = cmpSamPool_.cpuHandle(idx);
	device_->CreateSampler(&samDesc, handle);

	// 10 - Bilinear Comparison
	samDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	idx = cmpSamPool_.alloc();
	handle = cmpSamPool_.cpuHandle(idx);
	device_->CreateSampler(&samDesc, handle);
}

// fenceName을 갖는 Fence의 desiredValue 값을 1 증가시키고
// GPU 큐에 그 갱신 명령을 삽입한다.
void GFX::signalFence(const std::string& fenceName) {
	auto validFenceName = fences_.contains(fenceName);
	DISPLAY_ERROR_STR(validFenceName, "[GFX Error] GFX::signalFence: 펜스 "s + fenceName + "를 찾을 수 없습니다.\n", false);
	if (!validFenceName) {
		return;
	}

	auto& fence = fences_.at(fenceName);
	// desiredValue는 메인 스레드 전용(증가/대기). 제출 스레드는 전달받은 값으로 Signal만 한다.
	++fence.desiredValue;
	submitter_->signal(fence.fence.Get(), fence.desiredValue);
}

// fenceName을 갖는 Fence에 대해서 wait하고,
// 사용이 끝난 명령 컨텍스트, 업로드 버퍼 등을 반환한다.
void GFX::waitOnFence(const std::string& fenceName) {
	auto validFenceName = fences_.contains(fenceName);
	DISPLAY_ERROR_STR(validFenceName, "[GFX Error] GFX::waitOnFence: 펜스 "s + fenceName + "를 찾을 수 없습니다.\n", false);
	if (!validFenceName) {
		return;
	}

	auto& fence = fences_.at(fenceName);
	if (fence.fence->GetCompletedValue() == fence.desiredValue) {
		// GPU에서 사용이 끝난 명령 컨텍스트와 리소스를 반환한다.
		// 비어있는 usage 리스트는 건너뛴다. (백그라운드 LoadFence 대기가 메인 렌더의
		//  RenderingMaster/Slave 풀 리스트를 건드리지 않도록 — 동시 접근 방지)
		for (int idxUsage = 0; idxUsage < etoi(CommandListUsage::SIZE); ++idxUsage) {
			if (!fence.associatedCmdCtxs_[idxUsage].empty())
				cmdListPool_.free(idxUsage, std::move(fence.associatedCmdCtxs_[idxUsage]));
		}
		fence.associatedResources_.clear();
		return;
	}

	fence.fence->SetEventOnCompletion(fence.desiredValue, nullptr);

	// GPU에서 사용이 끝난 명령 컨텍스트와 리소스를 반환한다.
	for (int idxUsage = 0; idxUsage < etoi(CommandListUsage::SIZE); ++idxUsage) {
		if (!fence.associatedCmdCtxs_[idxUsage].empty())
			cmdListPool_.free(idxUsage, std::move(fence.associatedCmdCtxs_[idxUsage]));
	}
	fence.associatedResources_.clear();
}
