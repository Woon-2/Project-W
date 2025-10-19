#include "gfx.hpp"

// DXGI Factory를 초기화하고, DXGI Adapter들을 열거한다.
// 그리고 그 중 하나를 선택하여 curAdapter_에 저장한다.
void GFX::setupDXGI(D3D_FEATURE_LEVEL d3dFeatureLevel) {
	d3dFeatureLevel_ = d3dFeatureLevel;

	CreateDXGIFactory2(
#ifdef DXGI_DEBUG_INFO
		DXGI_CREATE_FACTORY_DEBUG,
#else
		0,
#endif
		__uuidof(IDXGIFactory4), &dxgiFactory_
	);

	// 출력 인자가 nullptr인 D3D12CreateDevice의 호출은 해당 그래픽 어댑터가
	// d3d12를 지원하는지 검사할 수 있다.
	// 이를 통해 d3d12를 지원하는 그래픽 어댑터들을 adapters_에 열거하자.
	auto pAdapter = ComPtr<IDXGIAdapter1>{};
	DXGI_ADAPTER_DESC1 desc{};

	for (UINT i = 0; dxgiFactory_->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		pAdapter->GetDesc1(&desc);

		if (D3D12CreateDevice(pAdapter.Get(), d3dFeatureLevel,
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
	ComPtr<ID3D12Debug> pDebug = nullptr;
	D3D12GetDebugInterface(__uuidof(ID3D12Debug), &pDebug);
	pDebug->EnableDebugLayer();
#endif

	D3D12CreateDevice(curAdapter_.Get(), d3dFeatureLevel_, __uuidof(ID3D12Device), &device_);

	auto qDesc = D3D12_COMMAND_QUEUE_DESC{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0
	};

	device_->CreateCommandQueue(&qDesc, __uuidof(ID3D12CommandQueue), &cmdQ_);

	for (std::size_t i = 0; i < cmdListPoolSize; ++i) {
		auto& alloc = freeCmdAllocators_.emplace_back();
		device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &alloc);

		auto& cmdList = freeCmdLists_.emplace_back();
		device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr,
			__uuidof(ID3D12GraphicsCommandList), &cmdList
		);
	}
}

// 윈도우와 연결된 SwapChain을 만든다.
void GFX::createSwapChain(HWND hWnd) {
	scd_ = DXGI_SWAP_CHAIN_DESC1{
		.Width = static_cast<UINT>(1024),
		.Height = static_cast<UINT>(768),
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.Stereo = false,
		.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1u, .Quality = 0u },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = static_cast<UINT>(3),
		.Scaling = DXGI_SCALING_STRETCH,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
		.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	};

	scfd_ = DXGI_SWAP_CHAIN_FULLSCREEN_DESC{
		.RefreshRate = DXGI_RATIONAL{.Numerator = 60, .Denominator = 1 },
		.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
		.Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
		.Windowed = true
	};

	ComPtr<IDXGISwapChain1> tmp = nullptr;

	dxgiFactory_->CreateSwapChainForHwnd(cmdQ_.Get(), hWnd, &scd_, &scfd_, nullptr, &tmp);
	tmp.As(&swapChain_);
	dxgiFactory_->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
}