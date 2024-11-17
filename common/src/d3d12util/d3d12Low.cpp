#include "d3d12util/d3d12Low.hpp"

#include <cassert>

namespace gfx {

namespace d3d12 {

dx::DXGIAdapter getAvailableAdapter(dx::DXGIFactory& factory, D3D_FEATURE_LEVEL featureLevel) {
	wrl::ComPtr<dx::DXGIAdapter::InterfaceType> pAdapter{};

	for (UINT i = 0; factory.get()->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC1 desc{};
		pAdapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		if ( D3D12CreateDevice(pAdapter.Get(), featureLevel,
			__uuidof(D3D12Device::InterfaceType), nullptr
		) >= 0 ) {
			return dx::DXGIAdapter( std::move(pAdapter) );
		}
	}

	DX_THROW_FAILED( factory.get()->EnumWarpAdapter(
		__uuidof(dx::DXGIAdapter::InterfaceType), &pAdapter
	) );
	return dx::DXGIAdapter( std::move(pAdapter) );
}

using D3D12DebugInterfaceType = ID3D12Debug;

D3D12Device::D3D12Device(dx::DXGIAdapter& adapter, D3D_FEATURE_LEVEL featureLevel)
	: dx::DXWrapper<ID3D12Device>() {
#ifdef ENABLE_DXGI_INFO
	wrl::ComPtr<D3D12DebugInterfaceType> pDebug{};
	DX_THROW_FAILED( D3D12GetDebugInterface(__uuidof(D3D12DebugInterfaceType), &pDebug) );
	pDebug->EnableDebugLayer();
#endif

	DX_THROW_FAILED( D3D12CreateDevice( adapter.get().Get(), featureLevel,
		__uuidof(InterfaceType), &src_
	) );
}

D3D12Device::D3D12Device(dx::DXGIAdapter&& adapter, D3D_FEATURE_LEVEL featureLevel)
	: D3D12Device(adapter, featureLevel) {}

D3D12CmdQueue::D3D12CmdQueue(D3D12Device& device)
	: dx::DXWrapper<ID3D12CommandQueue>() {
	auto desc = D3D12_COMMAND_QUEUE_DESC{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0
	};
	DX_THROW_FAILED( device.get()->CreateCommandQueue(
		&desc, __uuidof(InterfaceType), &src_
	) );
}

D3D12GfxCmdList::D3D12GfxCmdList(D3D12Device& device)
	: dx::DXWrapper<ID3D12GraphicsCommandList>() {
	DX_THROW_FAILED( device.get()->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(Allocator), &alloc_
	) );
	DX_THROW_FAILED( device.get()->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		alloc_.Get(), nullptr, __uuidof(InterfaceType), &src_
	) );
}

void D3D12GfxCmdList::reset() {
	DX_THROW_FAILED( alloc_->Reset() );
	DX_THROW_FAILED( src_->Reset(alloc_.Get(), nullptr) );
}

void D3D12GfxCmdList::close() {
	DX_THROW_FAILED( src_->Close() );
}

void D3D12GfxCmdList::copyResource(D3D12Resource& srcRes, D3D12Resource& destRes) {
	auto srcOldState = srcRes.state();
	auto destOldState = destRes.state();

	srcRes.commitState(*this, D3D12_RESOURCE_STATE_COPY_SOURCE);
	destRes.commitState(*this, D3D12_RESOURCE_STATE_COPY_DEST);

	DX_THROW_FAILED_VOID( get()->CopyResource(destRes.get().Get(), srcRes.get().Get()) );

	srcRes.commitState(*this, srcOldState);
	destRes.commitState(*this, destOldState);
}

D3D12Resource::D3D12Resource(D3D12Device& device, const D3D12_RESOURCE_DESC& resDesc,
	D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
	const D3D12_CLEAR_VALUE* optimizedClearValue
) : views_(), vbview_{}, desc_{}, gpuAddr_{}, stride_(0u), state_(initialState) {
	auto heapProps = D3D12_HEAP_PROPERTIES{
		.Type = heapType
	};

	DX_THROW_FAILED( device.get()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		state_, optimizedClearValue, __uuidof(InterfaceType), &src_
	) );

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
	DX_THROW_FAILED_VOID( device.get()->CreateConstantBufferView(
		&cbvDesc, destView.cpuHandle()
	) );
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeSrv(const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
	D3D12Device& device, Descriptor& destView
) {
	destView.setType(Descriptor::Type::SRV);
	DX_THROW_FAILED_VOID( device.get()->CreateShaderResourceView(
		src_.Get(), &srvDesc, destView.cpuHandle()
	) );
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeUav(const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc,
	D3D12Device& device, Descriptor& destView
) {
	destView.setType(Descriptor::Type::UAV);
	DX_THROW_FAILED_VOID( device.get()->CreateUnorderedAccessView(
		src_.Get(), nullptr, &uavDesc, destView.cpuHandle()
	) );
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeRtv(const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc,
	D3D12Device& device, Descriptor& destView
) {
	destView.setType(Descriptor::Type::RTV);
	DX_THROW_FAILED_VOID( device.get()->CreateRenderTargetView(
		src_.Get(), &rtvDesc, destView.cpuHandle()
	) );
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeDsv(const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
	D3D12Device& device, Descriptor& destView
) {
	destView.setType(Descriptor::Type::DSV);
	DX_THROW_FAILED_VOID( device.get()->CreateDepthStencilView(
		src_.Get(), &dsvDesc, destView.cpuHandle()
	) );
	views_.push_back(destView);
	return views_.size() - 1u;
}

void D3D12Resource::remakeCbv( std::size_t idx,
	const D3D12_CONSTANT_BUFFER_VIEW_DESC& cbvDesc, D3D12Device& device
) {
	assert(views_.at(idx).type() == Descriptor::Type::CBV);
	DX_THROW_FAILED_VOID( device.get()->CreateConstantBufferView(
		&cbvDesc, views_.at(idx).cpuHandle()
	) );
}

void D3D12Resource::remakeSrv( std::size_t idx,
	const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc, D3D12Device& device
) {
	assert(views_.at(idx).type() == Descriptor::Type::SRV);
	DX_THROW_FAILED_VOID( device.get()->CreateShaderResourceView(
		src_.Get(), &srvDesc, views_.at(idx).cpuHandle()
	) );
}

void D3D12Resource::remakeUav( std::size_t idx,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc, D3D12Device& device
) {
	assert(views_.at(idx).type() == Descriptor::Type::UAV);
	DX_THROW_FAILED_VOID( device.get()->CreateUnorderedAccessView(
		src_.Get(), nullptr, &uavDesc, views_.at(idx).cpuHandle()
	) );
}

void D3D12Resource::remakeRtv( std::size_t idx,
	const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc, D3D12Device& device
) {
	assert(views_.at(idx).type() == Descriptor::Type::RTV);
	DX_THROW_FAILED_VOID( device.get()->CreateRenderTargetView(
		src_.Get(), &rtvDesc, views_.at(idx).cpuHandle()
	) );
}

void D3D12Resource::remakeDsv( std::size_t idx,
	const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc, D3D12Device& device
) {
	assert(views_.at(idx).type() == Descriptor::Type::DSV);
	DX_THROW_FAILED_VOID( device.get()->CreateDepthStencilView(
		src_.Get(), &dsvDesc, views_.at(idx).cpuHandle()
	) );
}

std::size_t D3D12Resource::makeDefCbv(D3D12Device& device, Descriptor& destView) {
	destView.setType(Descriptor::Type::CBV);
	auto desc = D3D12_CONSTANT_BUFFER_VIEW_DESC{
		.BufferLocation = src_->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>( desc_.Width * desc_.Height )
	};
	DX_THROW_FAILED_VOID( device.get()->CreateConstantBufferView(
		&desc, destView.cpuHandle()
	) );

	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeDefSrv(D3D12Device& device, Descriptor& destView) {
	destView.setType(Descriptor::Type::SRV);
	DX_THROW_FAILED_VOID( device.get()->CreateShaderResourceView(
		src_.Get(), nullptr, destView.cpuHandle()
	) );
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeDefUav(D3D12Device& device, Descriptor& destView) {
	destView.setType(Descriptor::Type::UAV);
	DX_THROW_FAILED_VOID( device.get()->CreateUnorderedAccessView(
		src_.Get(), nullptr, nullptr, destView.cpuHandle()
	) );
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeDefRtv(D3D12Device& device, Descriptor& destView) {
	destView.setType(Descriptor::Type::RTV);
	DX_THROW_FAILED_VOID( device.get()->CreateRenderTargetView(
		src_.Get(), nullptr, destView.cpuHandle()
	) );
	views_.push_back(destView);
	return views_.size() - 1u;
}

std::size_t D3D12Resource::makeDefDsv(D3D12Device& device, Descriptor& destView) {
	destView.setType(Descriptor::Type::DSV);
	DX_THROW_FAILED_VOID( device.get()->CreateDepthStencilView(
		src_.Get(), nullptr, destView.cpuHandle()
	) );
	views_.push_back(destView);
	return views_.size() - 1u;
}

void D3D12Resource::remakeDefCbv(std::size_t idx, D3D12Device& device) {
	assert(views_.at(idx).type() == Descriptor::Type::CBV);
	auto desc = D3D12_CONSTANT_BUFFER_VIEW_DESC{
		.BufferLocation = src_->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>( desc_.Width * desc_.Height )
	};
	DX_THROW_FAILED_VOID( device.get()->CreateConstantBufferView(
		&desc, views_.at(idx).cpuHandle()
	) );
}

void D3D12Resource::remakeDefSrv(std::size_t idx, D3D12Device& device) {
	assert(views_.at(idx).type() == Descriptor::Type::SRV);
	DX_THROW_FAILED_VOID( device.get()->CreateShaderResourceView(
		src_.Get(), nullptr, views_.at(idx).cpuHandle()
	) );
}

void D3D12Resource::remakeDefUav(std::size_t idx, D3D12Device& device) {
	assert(views_.at(idx).type() == Descriptor::Type::UAV);
	DX_THROW_FAILED_VOID( device.get()->CreateUnorderedAccessView(
		src_.Get(), nullptr, nullptr, views_.at(idx).cpuHandle()
	) );
}

void D3D12Resource::remakeDefRtv(std::size_t idx, D3D12Device& device) {
	assert(views_.at(idx).type() == Descriptor::Type::RTV);
	DX_THROW_FAILED_VOID( device.get()->CreateRenderTargetView(
		src_.Get(), nullptr, views_.at(idx).cpuHandle()
	) );
}

void D3D12Resource::remakeDefDsv(std::size_t idx, D3D12Device& device) {
	assert(views_.at(idx).type() == Descriptor::Type::DSV);
	DX_THROW_FAILED_VOID( device.get()->CreateDepthStencilView(
		src_.Get(), nullptr, views_.at(idx).cpuHandle()
	) );
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

	DX_THROW_FAILED_VOID( cmdList.get()->ResourceBarrier(1u, &bar) );
	state_ = resState;
}

Fence::Fence(D3D12Device& device, UINT64 initValue = 0u)
	: dx::DXWrapper<ID3D12Fence>(), value_(initValue), event_(nullptr) {
	DX_THROW_FAILED( device.get()->CreateFence(
		initValue, D3D12_FENCE_FLAG_NONE, __uuidof(InterfaceType), &src_
	) );

	event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	if (event_ == nullptr) {
		DX_THROW_FAILED( HRESULT_FROM_WIN32(GetLastError()) );
	}
}

void Fence::signal(UINT64 value) {
	DX_THROW_FAILED( src_->Signal(value) );
	value_ = value;
}

void Fence::signal() {
	signal(value_ + 1);
}

void Fence::wait(UINT64 value) {
	if (src_->GetCompletedValue() >= value) {
		return;
	}

	DX_THROW_FAILED( src_->SetEventOnCompletion(value, event_) );
	WaitForSingleObject(event_, INFINITE);
}

void Fence::wait() {
	wait(value_);
}

}   // namespace gfx::d3d12

}   // namespace gfx