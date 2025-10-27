#include "gfx.hpp"
#include "errorHandling.hpp"

// GFX가 소멸할 때, 제출된 모든 GPU작업이 완료되고 나서 소멸하도록 한다.
GFX::~GFX() {
	for (std::size_t i = 0u; i < backBuffers_.size(); ++i) {
		waitOnFence(L"FrameFence"s + std::to_wstring(i));
	}
	waitOnFence(L"LoadFence");
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


	setDXName(dxgiFactory_.Get(), L"DXGIFactory");

	// 출력 인자가 nullptr인 D3D12CreateDevice의 호출은 해당 그래픽 어댑터가
	// d3d12를 지원하는지 검사할 수 있다.
	// 이를 통해 d3d12를 지원하는 그래픽 어댑터들을 adapters_에 열거하자.
	auto pAdapter = ComPtr<IDXGIAdapter1>{};
	DXGI_ADAPTER_DESC1 desc{};

	for (UINT i = 0; dxgiFactory_->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		pAdapter->GetDesc1(&desc);

		setDXName(pAdapter.Get(), desc.Description);

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
		std::wcout << i << L" - " << adapterDescs_[i].Description << L'\n';
	}
	std::cout << "선택: ";
	int idx{};
	std::cin >> idx;
	std::wcout << idx << L" - " << adapterDescs_[idx].Description;
	std::cout << "이 선택되었습니다.\n";
	std::cout << "----------------------------------------\n";

	curAdapter_ = adapters_[idx];
}

// D3D12 Device와 Command Queue, Descriptor Heap들을 만든다.
// 그리고 인자로 전달받은 개수만큼 CommandList와 Command Allocator를 만든다.
// Fence들을 만든다. 그리고 Root Signature와 Shader(PSO)들을 만든다.
void GFX::init(std::size_t cmdListPoolSize) {
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
	setD3DName(device_.Get(), L"Device");

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
	setD3DName(cmdQ_.Get(), L"CommandQueue");

	// Command Allocator와 Command List들 생성
	for (std::size_t i = 0; i < cmdListPoolSize; ++i) {
		auto& alloc = freeCmdAllocators_.emplace_back();
		DISPLAY_ERROR_DX_HR(
			device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &alloc),
			true
		);
		setD3DName(alloc.Get(), std::wstring(L"CommandAllocator") + std::to_wstring(i));

		auto& cmdList = freeCmdLists_.emplace_back();
		DISPLAY_ERROR_DX_HR(
			device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr,
				__uuidof(ID3D12GraphicsCommandList), &cmdList
			),
			true
		);
		setD3DName(cmdList.Get(), std::wstring(L"CommandList") + std::to_wstring(i));
		// 생성됐을 땐 open 상태이므로,
		// render 호출 시 이미 open 상태인 Command List를 open할 수 있다.
		// 따라서 여기서 close 해준다.
		cmdList->Close();
	}

	// Descriptor Heap 및 Pool들 생성
	rtvHeap_ = DescriptorHeap( device_.Get(), D3D12_DESCRIPTOR_HEAP_DESC{
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		.NumDescriptors = 4u,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask = 0
	} );

	rtvPool_ = DescriptorPool( 4u, rtvHeap_.cpuStart, rtvHeap_.gpuStart, rtvHeap_.desc.Type,
		rtvHeap_.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
	);

	dsvHeap_ = DescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_DESC{
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		.NumDescriptors = 4u,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask = 0
	} );

	dsvPool_ = DescriptorPool( 4u, dsvHeap_.cpuStart, dsvHeap_.gpuStart, dsvHeap_.desc.Type,
		dsvHeap_.desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
	);

	// Root Signatures & Shaders 생성
	auto defaultRootSig = DefaultRootSig{};
	defaultRootSig.build(device_.Get());

	shaders_.try_emplace(L"SampleShader", createSampleShader(device_.Get(), defaultRootSig.get()));

	rootSigs_.try_emplace(L"DefaultRootSignature", std::make_unique<DefaultRootSig>(std::move(defaultRootSig)));

	// Load Fence 추가
	const auto fenceName = L"LoadFence"s;
	fences_.try_emplace(fenceName, Fence{});
	DISPLAY_ERROR_DX_HR(
		device_->CreateFence(0u, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &fences_.at(fenceName).fence),
		false
	);
	setD3DName(fences_.at(fenceName).fence.Get(), fenceName);
	fences_.at(fenceName).event = CreateEvent(
		nullptr, false, false, fenceName.c_str()
	);

	// Draw Event들을 저장할 메모리 예약
	drawEventsSamplePipeline_.reserve(1000u);
}

// 윈도우와 연결된 SwapChain을 만든다.
// Back Buffer 개수 만큼의 room을 가지는 Constant Buffer들을 만든다.
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

	setDXName(swapChain_.Get(), L"SwapChain");

	// 백버퍼 및 깊이버퍼 리소스와 뷰 생성
	backBuffers_.resize(3u);

	// 백버퍼는 스왑체인에서 이미 만들었으므로,
	// swapChain_->GetBuffer를 통해 가져와 디스크립터만 만든다.
	for (int i = 0; i < 3; ++i) {
		DISPLAY_ERROR_DX_HR(
			swapChain_->GetBuffer(i, __uuidof(ID3D12Resource), &backBuffers_[i]),
			true
		);
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
		depthBuffers_.push_back(
			createDepthBuffer( device_.Get(), DXGI_FORMAT_D24_UNORM_S8_UINT,
				DXGI_SAMPLE_DESC{
					.Count = 1u,
					.Quality = 0u
				}
			)
		);
	}

	for (int i = 0; i < 3; ++i) {
		auto idx = allocatedDsvIndices_.emplace_back(dsvPool_.alloc());
		auto cpuHandle = depthBufferDsvs_.emplace_back(dsvPool_.cpuHandle(idx));
		DISPLAY_ERROR_DX_VOID(
			device_->CreateDepthStencilView(depthBuffers_[i].Get(), nullptr, cpuHandle),
			true
		);
	}

	// 백버퍼 개수만큼의 room을 가지는 파이프라인별 리소스들 생성
	// 1000u를 변수로 대체하기
	resourcesSamplePipeline_.perInstanceData.init(
		device_.Get(), sizeof(SampleShader::PerInstanceData) * 1000u, backBuffers_.size(), L"Sample_PerInstanceData"
	);
	resourcesSamplePipeline_.perDrawcallData = createConstantBufferArray(
		device_.Get(), sizeof(SampleShader::PerDrawcallData), 1000u, backBuffers_.size(), L"Sample_PerDrawcallData"
	);

	// 프레임 펜스 생성
	// i번째 프레임을 렌더링한 후 i-(백버퍼 수 - 1)번째 프레임의 펜스를 기다리도록 한다.
	for (std::size_t i = 0u; i < backBuffers_.size(); ++i) {
		const auto fenceName = L"FrameFence"s + std::to_wstring(i);
		fences_.try_emplace(fenceName, Fence{});
		DISPLAY_ERROR_DX_HR(
			device_->CreateFence(0u, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &fences_.at(fenceName).fence),
			false
		);
		setD3DName(fences_.at(fenceName).fence.Get(), fenceName);
		fences_.at(fenceName).event = CreateEvent(
			nullptr, false, false, (L"FrameFenceEvent"s + std::to_wstring(i)).c_str()
		);
	}
}

// 드로우콜 요청을 제출한다. render() 호출 시 그려진다.
void GFX::addDrawEvent(const SamplePipeline::DrawEvent& drawEvent) {
	drawEventsSamplePipeline_.push_back(drawEvent);
}

void GFX::loadMeshes() {
	auto& fence = fences_.at(L"LoadFence");

	// 명령 리스트와 명령 할당자 할당
	auto cmdList = freeCmdLists_.front();
	freeCmdLists_.pop_front();

	auto cmdAlloc = freeCmdAllocators_.front();
	freeCmdAllocators_.pop_front();

	// 명령 리스트 초기화
	DISPLAY_ERROR_DX_VOID(cmdAlloc->Reset(), false);
	DISPLAY_ERROR_DX_VOID(cmdList->Reset(cmdAlloc.Get(), nullptr), false);

	// 명령 기록 시작
	auto [mesh, auxUploadBuffers] = buildCubeMesh(device_.Get(), cmdList.Get());
	meshCube_ = std::move(mesh);

	// 명령 기록 끝, 명령 실행
	DISPLAY_ERROR_DX_VOID(cmdList->Close(), false);

	ID3D12CommandList* tmpCmdLists[] = { cmdList.Get() };

	DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, tmpCmdLists), false);

	// 펜스 동기화
	fence.associatedCmdLists_.push_back(std::move(cmdList));
	fence.associatedCmdAllocators_.push_back(std::move(cmdAlloc));
	fence.associatedResources_.append_range(std::move(auxUploadBuffers));
	signalFence(L"LoadFence");
	waitOnFence(L"LoadFence");
}

void GFX::render() {
	// 예외 검사
	DISPLAY_ERROR_STR(curAdapter_, "[GFX Error] 활성화된 그래픽 어댑터가 없습니다. "
		"GFX::setupDXGI의 호출이 이루어졌는지 확인하세요.", false);

	DISPLAY_ERROR_STR(device_, "[GFX Error] 장치 초기화가 이루어지지 않았습니다. "
		"GFX::init 호출이 이루어졌는지 확인하세요.", false);

	DISPLAY_ERROR_STR(cmdQ_, "[GFX Error] 명령 큐가 활성화되지 않았습니다. "
		"GFX::init 호출이 이루어졌는지 확인하세요.", false);

	DISPLAY_ERROR_STR(swapChain_, "[GFX Error] 스왑 체인이 활성화되지 않았습니다. "
		"GFX::createSwapChain 호출이 이루어졌는지 확인하세요.", false);

	if (!curAdapter_ || !device_ || !cmdQ_ || !swapChain_) {
		return;
	}

	DISPLAY_ERROR_STR(!freeCmdLists_.empty(), "[GFX Error] 사용 가능한 명령 리스트가 없습니다. "
		"GFX::init 호출이 이루어지지 않았거나, 할당받은 명령 리스트를 반납해야 합니다.", false);

	DISPLAY_ERROR_STR(!freeCmdAllocators_.empty(), "[GFX Error] 사용 가능한 명령 할당자가 없습니다. "
		"GFX::init 호출이 이루어지지 않았거나, 할당받은 명령 할당자를 반납해야 합니다.", false);
	if (freeCmdLists_.empty() || freeCmdAllocators_.empty()) {
		return;
	}

	// 명령 리스트와 명령 할당자 할당
	auto cmdList = freeCmdLists_.front();
	freeCmdLists_.pop_front();

	auto cmdAlloc = freeCmdAllocators_.front();
	freeCmdAllocators_.pop_front();

	// 명령 리스트 초기화
	DISPLAY_ERROR_DX_VOID( cmdAlloc->Reset(), false );
	DISPLAY_ERROR_DX_VOID( cmdList->Reset(cmdAlloc.Get(), nullptr), false );

	// 명령 기록 시작
	const auto backbufIdx = swapChain_->GetCurrentBackBufferIndex();

	transitionResourceState(cmdList.Get(), backBuffers_[backbufIdx].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	DISPLAY_ERROR_DX_VOID(
		cmdList->OMSetRenderTargets(1u, &backBufferRtvs_[backbufIdx], false, &depthBufferDsvs_[backbufIdx]),
		false
	);

	FLOAT clearColor[4] = { 0.25f, 0.3f, 0.85f, 1.f };
	DISPLAY_ERROR_DX_VOID(
		cmdList->ClearRenderTargetView(backBufferRtvs_[backbufIdx], clearColor, 0u, nullptr),
		false
	);
	
	DISPLAY_ERROR_DX_VOID(
		cmdList->ClearDepthStencilView(depthBufferDsvs_[backbufIdx],
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			1.0f, 0u, 0u, nullptr
		), false
	);

	const auto viewport = D3D12_VIEWPORT{
		.TopLeftX = static_cast<FLOAT>(gClientRect.left),
		.TopLeftY = static_cast<FLOAT>(gClientRect.top),
		.Width = static_cast<FLOAT>(gClientRect.right - gClientRect.left),
		.Height = static_cast<FLOAT>(gClientRect.bottom - gClientRect.top),
		.MinDepth = 0.f,
		.MaxDepth = 1.f
	};

	DISPLAY_ERROR_DX_VOID( cmdList->RSSetViewports(1u, &viewport), false );

	const auto scissorRect = gClientRect;

	DISPLAY_ERROR_DX_VOID( cmdList->RSSetScissorRects(1u, &scissorRect), false );

	renderSampleShader(cmdList.Get());

	// 명령 기록 끝, 명령 실행 및 화면에 출력
	transitionResourceState(cmdList.Get(), backBuffers_[backbufIdx].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT
	);

	DISPLAY_ERROR_DX_VOID( cmdList->Close(), false );

	ID3D12CommandList* tmpCmdLists[] = { cmdList.Get() };

	DISPLAY_ERROR_DX_VOID( cmdQ_->ExecuteCommandLists(1u, tmpCmdLists), false );

	DISPLAY_ERROR_DX_VOID( swapChain_->Present(0, 0), false );

	// 펜스 동기화
	// 렌더링할 수 있는 백버퍼가 있다면,
	// wait하지 않고 이어서 렌더링을 하도록 한다.
	// 백버퍼는 순차적으로 사용되므로, (백버퍼 개수 - 1) 프레임 전의 렌더링에 대한
	// Fence를 wait하면 이를 구현할 수 있다.
	auto idxFenceToSignal = frameIdx % backBuffers_.size();
	auto idxFenceToWait = (frameIdx - (backBuffers_.size() - 1)) % backBuffers_.size();
	auto fenceNameToSignal = L"FrameFence" + std::to_wstring(idxFenceToSignal);
	fences_.at(fenceNameToSignal).associatedCmdLists_.push_back(std::move(cmdList));
	fences_.at(fenceNameToSignal).associatedCmdAllocators_.push_back(std::move(cmdAlloc));
	signalFence(L"FrameFence" + std::to_wstring(idxFenceToSignal));
	waitOnFence(L"FrameFence" + std::to_wstring(idxFenceToWait));

	// 처리한 DrawEvent들 제거
	drawEventsSamplePipeline_.clear();

	// 프레임 인덱스 갱신
	++frameIdx;
}

void GFX::renderSampleShader(ID3D12GraphicsCommandList* cmdList) {
	auto& pRootSig = rootSigs_.at(L"DefaultRootSignature");
	auto& shader = shaders_.at(L"SampleShader");

	DISPLAY_ERROR_DX_VOID( cmdList->SetGraphicsRootSignature(pRootSig->get()), false );
	DISPLAY_ERROR_DX_VOID( cmdList->SetPipelineState(shader.Get()), false );

	const auto roomIdx = frameIdx % backBuffers_.size();
	
	// 메시 데이터 업로드
	// 정렬을 통해 인스턴싱이 가능하도록 한다.
	std::sort(drawEventsSamplePipeline_.begin(), drawEventsSamplePipeline_.end());
	
	/* thread_local */ auto perInstanceData = std::vector<SampleShader::PerInstanceData>();
	perInstanceData.reserve(drawEventsSamplePipeline_.size());
	for (const auto& drawEvent : drawEventsSamplePipeline_) {
		perInstanceData.push_back(SampleShader::PerInstanceData{
			.wvp = mu::transpose(drawEvent.world).getXmf()
		});
	}

	const auto rootParamIdxPID = pRootSig->paramIdx(L"PerInstanceData");
	const auto rootParamIdxPDD = pRootSig->paramIdx(L"PerDrawcallData");

	resourcesSamplePipeline_.perInstanceData.stage(roomIdx, perInstanceData);
	resourcesSamplePipeline_.perInstanceData.bind( cmdList, 
		rootParamIdxPID, roomIdx
	);

	// 메시 그리기(Draw Call)
	DISPLAY_ERROR_DX_VOID( cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false );

	u32t idxDrawcall = 0u;

	for (const auto& drawEvent : drawEventsSamplePipeline_) {
		resourcesSamplePipeline_.perDrawcallData.cbuffers[idxDrawcall].bind(
			cmdList, rootParamIdxPDD, roomIdx
		);

		auto perDrawcallData = SampleShader::PerDrawcallData{
			.firstInstanceIdx = idxDrawcall
		};
		resourcesSamplePipeline_.perDrawcallData.cbuffers[idxDrawcall].stage(
			roomIdx, &perDrawcallData, 1u
		);

		DISPLAY_ERROR_DX_VOID(
			cmdList->IASetVertexBuffers(0u, static_cast<UINT>(drawEvent.mesh->vbViews.size()),
				drawEvent.mesh->vbViews.data()	
			), false
		);
		DISPLAY_ERROR_DX_VOID( cmdList->IASetIndexBuffer(&drawEvent.subMesh->ibView), false );

		DISPLAY_ERROR_DX_VOID( cmdList->DrawIndexedInstanced(
			static_cast<UINT>(drawEvent.subMesh->ibView.SizeInBytes / sizeof(u16t)),
			1u, 0u, 0, 0u
		), false );

		++idxDrawcall;
	}
}

// fenceName을 갖는 Fence의 desiredValue 값을 1 증가시키고
// GPU 큐에 그 갱신 명령을 삽입한다.
void GFX::signalFence(const std::wstring& fenceName) {
	auto validFenceName = fences_.contains(fenceName);
	DISPLAY_ERROR_STR(validFenceName, L"[GFX Error] GFX::signalFence: 펜스 "s + fenceName + L"를 찾을 수 없습니다.\n", false);
	if (!validFenceName) {
		return;
	}

	auto& fence = fences_.at(fenceName);
	++fence.desiredValue;
	DISPLAY_ERROR_DX_HR(
		cmdQ_->Signal(fence.fence.Get(), fence.desiredValue),
		false
	);
}

// fenceName을 갖는 Fence에 대해서 wait하고,
// 사용이 끝난 명령 리스트, 명령 할당자, 업로드 버퍼 등을 반환한다.
void GFX::waitOnFence(const std::wstring& fenceName) {
	auto validFenceName = fences_.contains(fenceName);
	DISPLAY_ERROR_STR(validFenceName, L"[GFX Error] GFX::waitOnFence: 펜스 "s + fenceName + L"를 찾을 수 없습니다.\n", false);
	if (!validFenceName) {
		return;
	}

	auto& fence = fences_.at(fenceName);
	if (fence.fence->GetCompletedValue() == fence.desiredValue) {
		freeCmdLists_.splice(freeCmdLists_.end(), std::move(fence.associatedCmdLists_));
		freeCmdAllocators_.splice(freeCmdAllocators_.end(), std::move(fence.associatedCmdAllocators_));
		fence.associatedResources_.clear();
		return;
	}

	fence.fence->SetEventOnCompletion(fence.desiredValue, fence.event);
	WaitForSingleObject(fence.event, INFINITE);

	freeCmdLists_.splice(freeCmdLists_.end(), std::move(fence.associatedCmdLists_));
	freeCmdAllocators_.splice(freeCmdAllocators_.end(), std::move(fence.associatedCmdAllocators_));
	fence.associatedResources_.clear();
}