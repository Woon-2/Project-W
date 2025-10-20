#include "gfx.hpp"

DescriptorHeap::DescriptorHeap(ID3D12Device* device, const D3D12_DESCRIPTOR_HEAP_DESC& desc)
	: desc(desc) {

	DISPLAY_ERROR_DX_HR(
		device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &heap),
		true
	);
	cpuStart = heap->GetCPUDescriptorHandleForHeapStart();
	gpuVisible = desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	if (gpuVisible) {
		gpuStart = heap->GetGPUDescriptorHandleForHeapStart();
	}
}

DescriptorPool::DescriptorPool(std::size_t viewCnt,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuStart, D3D12_GPU_DESCRIPTOR_HANDLE gpuStart,
	D3D12_DESCRIPTOR_HEAP_TYPE type, bool gpuVisible, UINT incrementSize
) : freeIndices_(viewCnt), cpuStart_(cpuStart), gpuStart_(gpuStart),
	type_(type), gpuVisible_(gpuVisible), incrementSize_(incrementSize) {

	std::iota(freeIndices_.begin(), freeIndices_.end(), 0);
}

int DescriptorPool::alloc() {
	auto ret = freeIndices_.front();
	freeIndices_.pop_front();
	return ret;
}

void DescriptorPool::free(int idx) {
	freeIndices_.push_back(idx);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorPool::cpuHandle(int idx) const {
	auto ret = cpuStart_;
	ret.ptr += idx * incrementSize_;
	return ret;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorPool::gpuHandle(int idx) const {
	DISPLAY_ERROR_STR(gpuVisible_, "[DescriptorPool] gpuVisible 플래그가 활성화되지 않은"
		"디스크립터 힙에서 gpuHandle 할당을 시도했습니다.", false
	);
	auto ret = gpuStart_;
	ret.ptr += idx * incrementSize_;
	return ret;
}

// 깊이 버퍼를 쉽게 생성하는 유틸리티 함수
ComPtr<ID3D12Resource> createDepthBuffer(
	ID3D12Device* device,
	DXGI_FORMAT format, const DXGI_SAMPLE_DESC& sampleDesc
) {
	ComPtr<ID3D12Resource> ret{};

	auto heapProperties = D3D12_HEAP_PROPERTIES{
		.Type = D3D12_HEAP_TYPE_DEFAULT
	};

	auto desc = D3D12_RESOURCE_DESC{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0u,
		.Width = static_cast<UINT64>(gClientRect.right - gClientRect.left),
		.Height = static_cast<UINT>(gClientRect.bottom - gClientRect.top),
		.DepthOrArraySize = 1u,
		.MipLevels = 1u,
		.Format = format,
		.SampleDesc = sampleDesc,
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	};

	auto cv = D3D12_CLEAR_VALUE{
		.Format = format,
		.DepthStencil = D3D12_DEPTH_STENCIL_VALUE{
			.Depth = 1.0f,
			.Stencil = 0u
		}
	};

	DISPLAY_ERROR_DX_HR(
		device->CreateCommittedResource( &heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, __uuidof(ID3D12Resource), &ret
		), false
	);

	return ret;
}

// ID3D12GraphicsCommandList::ResourceBarrier 인터페이스를 통해
// 리소스의 상태를 beforeState에서 afterState로 전환한다.
// 사용되는 기본값들은 본문을 참조하자.
void transitionResourceState( ID3D12GraphicsCommandList* cmdList,
	ID3D12Resource* resource, D3D12_RESOURCE_STATES beforeState,
	D3D12_RESOURCE_STATES afterState
) {
	auto barrier = D3D12_RESOURCE_BARRIER{
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
			.pResource = resource,
			.Subresource = 0u,
			.StateBefore = beforeState,
			.StateAfter = afterState
		}
	};
	DISPLAY_ERROR_DX_VOID( cmdList->ResourceBarrier(1u, &barrier), false );
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
}

// 윈도우와 연결된 SwapChain을 만든다.
// Scaling, BufferCount는 후에 매개변수로 전달받도록 하자.
void GFX::createSwapChain(HWND hWnd) {
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
		.RefreshRate = DXGI_RATIONAL{ .Numerator = 60, .Denominator = 1 },
		.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
		.Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
		.Windowed = true
	};

	ComPtr<IDXGISwapChain1> tmp = nullptr;

	DISPLAY_ERROR_DX_HR(
		dxgiFactory_->CreateSwapChainForHwnd(cmdQ_.Get(), hWnd, &scd_, &scfd_, nullptr, &tmp),
		true
	);
	tmp.As(&swapChain_);
	DISPLAY_ERROR_DX_HR(dxgiFactory_->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER), false);

	setDXName(swapChain_.Get(), L"SwapChain");

	// 백버퍼 및 깊이버퍼 리소스와 뷰 생성
	backBuffers_.resize(3u);

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
		.MaxDepth = 0.f
	};

	DISPLAY_ERROR_DX_VOID( cmdList->RSSetViewports(1u, &viewport), false );

	const auto scissorRect = gClientRect;

	DISPLAY_ERROR_DX_VOID( cmdList->RSSetScissorRects(1u, &scissorRect), false );

	transitionResourceState(cmdList.Get(), backBuffers_[backbufIdx].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT
	);

	// 명령 기록 끝, 화면에 출력
	DISPLAY_ERROR_DX_VOID( cmdList->Close(), false );

	ID3D12CommandList* tmpCmdLists[] = { cmdList.Get() };

	DISPLAY_ERROR_DX_VOID( cmdQ_->ExecuteCommandLists(1u, tmpCmdLists), false );

	DISPLAY_ERROR_DX_VOID( swapChain_->Present(0, 0), false );
	Sleep(1000);

	freeCmdLists_.push_back(std::move(cmdList));
	freeCmdAllocators_.push_back(std::move(cmdAlloc));
}