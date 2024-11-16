#include "d3d12Low.hpp"

#include <cassert>

namespace gfx {

namespace d3d12 {

using D3D12DebugInterfaceType = ID3D12Debug;

D3D12Device::D3D12Device(DXGIAdapter& adapter, D3D_FEATURE_LEVEL featureLevel)
	: D3DWrapper<ID3D12Device>() {
#ifdef ENABLE_DXGI_INFO
	wrl::ComPtr<D3D12DebugInterfaceType> pDebug{};
	D3D12GetDebugInterface(__uuidof(D3D12DebugInterfaceType), &pDebug);
	pDebug->EnableDebugLayer();
#endif

	D3D12CreateDevice(adapter.get().Get(), featureLevel,
		__uuidof(InterfaceType), &src_
	);
}

D3D12Device::D3D12Device(DXGIAdapter&& adapter, D3D_FEATURE_LEVEL featureLevel)
	: D3D12Device(adapter, featureLevel) {}

DXGIFactory::DXGIFactory()
	: D3DWrapper<IDXGIFactory4>() {
#ifdef ENABLE_DXGI_INFO
	CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(InterfaceType), &src_);
#else
	CreateDXGIFactory2(0, __uuidof(InterfaceType), &src_);
#endif
}

DXGIAdapter DXGIFactory::getAvailableAdapter(D3D_FEATURE_LEVEL featureLevel) {
	wrl::ComPtr<DXGIAdapter::InterfaceType> pAdapter{};

	for (UINT i = 0; src_->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC1 desc{};
		pAdapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		if ( D3D12CreateDevice(pAdapter.Get(), featureLevel,
			__uuidof(D3D12Device::InterfaceType), nullptr
		) >= 0 ) {
			return DXGIAdapter( std::move(pAdapter) );
		}
	}

	src_->EnumWarpAdapter(__uuidof(DXGIAdapter::InterfaceType), &pAdapter);
	return DXGIAdapter( std::move(pAdapter) );
}

D3D12CmdQueue::D3D12CmdQueue(D3D12Device& device)
	: D3DWrapper<ID3D12CommandQueue>() {
	auto desc = D3D12_COMMAND_QUEUE_DESC{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0
	};
	device.get()->CreateCommandQueue(&desc, __uuidof(InterfaceType), &src_);
}

D3D12GfxCmdList::D3D12GfxCmdList(D3D12Device& device)
	: D3DWrapper<ID3D12GraphicsCommandList>() {
	device.get()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(Allocator), &alloc_);
	device.get()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc_.Get(), nullptr,
		__uuidof(InterfaceType), &src_
	);
}

void D3D12GfxCmdList::copyResource(D3D12Resource& srcRes, D3D12Resource& destRes) {
	auto srcOldState = srcRes.state();
	auto destOldState = destRes.state();

	srcRes.commitState(*this, D3D12_RESOURCE_STATE_COPY_SOURCE);
	destRes.commitState(*this, D3D12_RESOURCE_STATE_COPY_DEST);

	get()->CopyResource(destRes.get().Get(), srcRes.get().Get());

	srcRes.commitState(*this, srcOldState);
	destRes.commitState(*this, destOldState);
}

// D3D12Window::D3D12Window(const RECT& clientRect, LPCWSTR wndName, DXGIFactory& factory, D3D12CmdQueue& cmdQueue)
// 	: Window(clientRect, wndName), D3DWrapper<IDXGISwapChain3>() {
// 	auto desc = DXGI_SWAP_CHAIN_DESC1{
// 		.Width = static_cast<UINT>(clientRect.right - clientRect.left),
// 		.Height = static_cast<UINT>(clientRect.bottom - clientRect.top),
// 		.Format = D3DConfig::backBufferFormat,
// 		.Stereo = false,
// 		.SampleDesc = DXGI_SAMPLE_DESC{
// 			.Count = 1,
// 			.Quality = 0
// 		},
// 		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
// 		.BufferCount = 3u,
// 		.Scaling = DXGI_SCALING_NONE,
// 		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
// 		.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
// 		.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
// 	};

// 	auto fsDesc = DXGI_SWAP_CHAIN_FULLSCREEN_DESC{
// 		.RefreshRate = DXGI_RATIONAL {
// 			.Numerator = 60,
// 			.Denominator = 1
// 		},
// 		.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
// 		.Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
// 		.Windowed = true
// 	};

// 	auto tmp = wrl::ComPtr<IDXGISwapChain1>{};

// 	factory.get()->CreateSwapChainForHwnd(cmdQueue.get().Get(), hWnd(), &desc,
// 		&fsDesc, nullptr, &tmp
// 	);

// 	tmp.As<InterfaceType>(&src_);

// 	factory.get()->MakeWindowAssociation(hWnd(), DXGI_MWA_NO_ALT_ENTER);
// }

D3D12Resource::D3D12Resource(D3D12Device& device, const D3D12_RESOURCE_DESC& resDesc,
	D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
	const D3D12_CLEAR_VALUE* optimizedClearValue
) : views_(), vbview_{}, desc_{}, gpuAddr_{}, stride_(0u), state_(initialState) {
	auto heapProps = D3D12_HEAP_PROPERTIES{
		.Type = heapType
	};

	device.get()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		state_, optimizedClearValue, __uuidof(InterfaceType), &src_
	);

	desc_ = src_->GetDesc();
	gpuAddr_ = src_->GetGPUVirtualAddress();
}

D3D12Resource::D3D12Resource(D3D12Device& device, const D3D12_RESOURCE_DESC& resDesc,
	D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
	std::size_t stride
) : D3D12Resource(device, resDesc, heapType, initialState) {
	stride_ = stride;
}

D3D12Resource::D3D12Resource(D3D12Device& device, const D3D12_RESOURCE_DESC& resDesc,
	D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState
) : D3D12Resource(device, resDesc, heapType, initialState, nullptr) {}

D3D12Resource::D3D12Resource(D3D12Device& device, const D3D12_RESOURCE_DESC& resDesc,
	D3D12_HEAP_TYPE heapType
) : D3D12Resource(device, resDesc, heapType, D3D12_RESOURCE_STATE_COMMON) {}

std::size_t D3D12Resource::makeCbv(const D3D12_CONSTANT_BUFFER_VIEW_DESC& cbvDesc,
	D3D12Device& device, Descriptor& destView
) {
	destView.setType(Descriptor::Type::CBV);
	device.get()->CreateConstantBufferView(&cbvDesc, destView.cpuHandle());
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeSrv(const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
	D3D12Device& device, Descriptor& destView
) {
	destView.setType(Descriptor::Type::SRV);
	device.get()->CreateShaderResourceView(src_.Get(), &srvDesc, destView.cpuHandle());
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeUav(const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc,
	D3D12Device& device, Descriptor& destView
) {
	destView.setType(Descriptor::Type::UAV);
	device.get()->CreateUnorderedAccessView(src_.Get(), nullptr, &uavDesc, destView.cpuHandle());
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeRtv(const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc,
	D3D12Device& device, Descriptor& destView
) {
	destView.setType(Descriptor::Type::RTV);
	device.get()->CreateRenderTargetView(src_.Get(), &rtvDesc, destView.cpuHandle());
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeDsv(const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
	D3D12Device& device, Descriptor& destView
) {
	destView.setType(Descriptor::Type::DSV);
	device.get()->CreateDepthStencilView(src_.Get(), &dsvDesc, destView.cpuHandle());
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeDefCbv(D3D12Device& device, Descriptor& destView) {
	destView.setType(Descriptor::Type::CBV);
	auto desc = D3D12_CONSTANT_BUFFER_VIEW_DESC{
		.BufferLocation = src_->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>( desc_.Width * desc_.Height )
	};
	device.get()->CreateConstantBufferView(&desc, destView.cpuHandle());

	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeDefSrv(D3D12Device& device, Descriptor& destView) {
	destView.setType(Descriptor::Type::SRV);
	device.get()->CreateShaderResourceView(src_.Get(), nullptr, destView.cpuHandle());
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeDefUav(D3D12Device& device, Descriptor& destView) {
	destView.setType(Descriptor::Type::UAV);
	device.get()->CreateUnorderedAccessView(src_.Get(), nullptr, nullptr, destView.cpuHandle());
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeDefRtv(D3D12Device& device, Descriptor& destView) {
	destView.setType(Descriptor::Type::RTV);
	device.get()->CreateRenderTargetView(src_.Get(), nullptr, destView.cpuHandle());
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeDefDsv(D3D12Device& device, Descriptor& destView) {
	destView.setType(Descriptor::Type::DSV);
	device.get()->CreateDepthStencilView(src_.Get(), nullptr, destView.cpuHandle());
	views_.push_back(destView);
	return views_.size() - 1u;
}

void D3D12Resource::makeDefVbv(D3D12Device& device) {
	vbview_ = D3D12_VERTEX_BUFFER_VIEW{
		.BufferLocation = src_->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(desc_.Width * desc_.Height),
		.StrideInBytes = static_cast<UINT>(stride_)
	};
}

void D3D12Resource::makeDefIbv(D3D12Device& device) {
	ibview_ = D3D12_INDEX_BUFFER_VIEW{
		.BufferLocation = src_->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(desc_.Width * desc_.Height),
		.Format = DXGI_FORMAT_R16_UINT
	};
}

void D3D12Resource::commitState(D3D12GfxCmdList& cmdList, D3D12_RESOURCE_STATES resState) {
	auto bar = D3D12_RESOURCE_BARRIER{
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
			.pResource = get().Get(),
			.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			.StateBefore = state_,
			.StateAfter = resState
		}
	};

	cmdList.get()->ResourceBarrier(1u, &bar);
	state_ = resState;
}

}   // namespace gfx::d3d12

}   // namespace gfx