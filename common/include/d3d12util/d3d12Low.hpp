#ifndef __d3d12Low_HPP
#define __d3d12Low_HPP

#include "dxutil/dxLow.hpp"
#include <directx/d3dx12.h>
#include <directx/d3d12.h>

#include "dxutil/dxexcept.hpp"

#include "config.hpp"
#include "TMP.hpp"

#include <vector>
#include <ranges>
#include <algorithm>
#include <any>

namespace gfx {

namespace d3d12 {

dx::DXGIAdapter getAvailableAdapter(dx::DXGIFactory& factory, D3D_FEATURE_LEVEL featureLevel);

class D3D12Device : public dx::DXWrapper<ID3D12Device> {
public:
	D3D12Device(dx::DXGIAdapter& adapter, D3D_FEATURE_LEVEL featureLevel);
	D3D12Device(dx::DXGIAdapter&& adapter, D3D_FEATURE_LEVEL featureLevel);
};

class D3D12GfxCmdList;

class D3D12CmdQueue : public dx::DXWrapper<ID3D12CommandQueue> {
public:
	D3D12CmdQueue(D3D12Device& device);

	template <std::ranges::range R>
		requires std::same_as<std::ranges::range_value_t<R>, D3D12GfxCmdList>
	void execute(R&& cmdLists);
	void execute(D3D12GfxCmdList& cmdList);
};

class D3D12GfxCmdList : public dx::DXWrapper<ID3D12GraphicsCommandList> {
public:
	using Allocator = ID3D12CommandAllocator;

	D3D12GfxCmdList(D3D12Device& device);

	void copyResource(class D3D12Resource& srcRes, D3D12Resource& destRes);
	void reset();
	void close();

	std::size_t addXResource(std::any&& xRes) {
		xResources_.push_back(std::move(xRes));
		return xResources_.size() - 1;
	}

	template <class T, class ... Args>
	std::size_t emplaceXResource(Args&& ... args) {
		xResources_.emplace_back(std::make_any<T>(std::forward<Args>(args)...));
		return xResources_.size() - 1;
	}

	template <class T>
	T& getXResource(std::size_t idx) {
		return std::any_cast<T&>(xResources_.at(idx));
	}

	template <class T>
	const T& getXResource(std::size_t idx) const {
		return std::any_cast<const T&>(xResources_.at(idx));
	}

private:
	wrl::ComPtr<Allocator> alloc_;
	std::vector<std::any> xResources_;
};

template <std::ranges::range R>
	requires std::same_as<std::ranges::range_value_t<R>, D3D12GfxCmdList>
void D3D12CmdQueue::execute(R&& cmdLists) {
	std::vector<ID3D12CommandList*> tmp;
	reserve_if_possible(tmp, std::ranges::size(cmdLists));
	std::ranges::transform(cmdLists, std::back_inserter(tmp),
		[](auto& cmdList) { return cmdList.get().Get(); }
	);

	get()->ExecuteCommandLists(static_cast<UINT>(tmp.size()), tmp.data());
}

inline void D3D12CmdQueue::execute(D3D12GfxCmdList& cmdList) {
	ID3D12CommandList* tmp[]{ cmdList.get().Get() };
	get()->ExecuteCommandLists(1u, tmp);
}

class DescriptorGPU;

class DescriptorCPU {
public:
	friend class D3D12Resource;
	template <class TDescHeap>
	friend class DescriptorRange;

	enum class Type {
		CBV, SRV, UAV, RTV, DSV, SAM, INVALID
	};

	DescriptorCPU()
		: cpuHandle_{}, gpuHandle_{}, offsetFromRange_(std::size_t(-1)), type_(Type::INVALID) {}

	// TODO: implement this
	DescriptorCPU(const DescriptorGPU& other);

	DescriptorCPU( const D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
		const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle, Type viewType
	) : cpuHandle_(cpuHandle), gpuHandle_(gpuHandle), offsetFromRange_(std::size_t(-1)), type_(viewType) {}

	std::size_t offset() const NOEXCEPT { return offsetFromRange_; }

	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle() const NOEXCEPT {
		return cpuHandle_;
	}

	Type type() const NOEXCEPT { return type_; }

private:
	void setType(Type viewType) NOEXCEPT { type_ = viewType; }
	void setOffset(std::size_t offsetFromRange) NOEXCEPT {
		offsetFromRange_ = offsetFromRange;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_;
protected:
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_;
private:
	std::size_t offsetFromRange_;
	Type type_;
};

using Descriptor = DescriptorCPU;

class DescriptorGPU : public DescriptorCPU {
public:
	using DescriptorCPU::DescriptorCPU;

	DescriptorGPU(const DescriptorCPU& other)
		: DescriptorCPU(other) {}

	const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle() const NOEXCEPT {
		return gpuHandle_;
	}
};

inline DescriptorCPU::DescriptorCPU(const DescriptorGPU& other)
	: DescriptorCPU(reinterpret_cast<const DescriptorCPU&>(other)) {}

template <class TDescriptor>
class DescriptorHeap : public dx::DXWrapper<ID3D12DescriptorHeap> {
public:
	using DescriptorType = TDescriptor;

	DescriptorHeap()
		: dx::DXWrapper(), descriptors_(), stride_(0),
		type_(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES), shaderVisible_(false) {}

	DescriptorHeap( D3D12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType,
		std::size_t capacity, bool bShaderVisible
	);

	std::size_t capacity() const NOEXCEPT { return descriptors_.size(); }

	TDescriptor& at(std::size_t idx) {
		return descriptors_.at(idx);
	}

	const TDescriptor& at(std::size_t idx) const {
		return descriptors_.at(idx);
	}

	TDescriptor& operator[](std::size_t idx) NOEXCEPT {
		return descriptors_[idx];
	}

	const TDescriptor& operator[](std::size_t idx) const NOEXCEPT {
		return descriptors_[idx];
	}

	void set(ID3D12GraphicsCommandList* pCmdList) {
		pCmdList->SetDescriptorHeaps(1u, src_.GetAddressOf());
	}

	std::size_t stride() const NOEXCEPT {
		return stride_;
	}

	D3D12_DESCRIPTOR_HEAP_TYPE type() const NOEXCEPT {
		return type_;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE& cpuStart() NOEXCEPT {
		return const_cast<D3D12_CPU_DESCRIPTOR_HANDLE&>(at(0).cpuHandle());
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuStart() const NOEXCEPT {
		return at(0).cpuHandle();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE& gpuStart() NOEXCEPT {
		return const_cast<D3D12_GPU_DESCRIPTOR_HANDLE&>(at(0).gpuHandle());
	}

	const D3D12_GPU_DESCRIPTOR_HANDLE& gpuStart() const NOEXCEPT {
		return at(0).gpuHandle();
	}

	bool shaderVisible() const NOEXCEPT {
		return shaderVisible_;
	}

	auto begin() NOEXCEPT {
		return descriptors_.begin();
	}

	auto end() NOEXCEPT {
		return descriptors_.end();
	}

	auto begin() const NOEXCEPT {
		return descriptors_.begin();
	}

	auto end() const NOEXCEPT {
		return descriptors_.end();
	}

	auto cbegin() const NOEXCEPT {
		return descriptors_.cbegin();
	}

	auto cend() const NOEXCEPT {
		return descriptors_.cend();
	}

private:
	std::vector<TDescriptor> descriptors_;
	std::size_t stride_;
	D3D12_DESCRIPTOR_HEAP_TYPE type_;
	bool shaderVisible_;
};

template <class TDescriptor>
DescriptorHeap<TDescriptor>::DescriptorHeap(D3D12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	std::size_t capacity, bool bShaderVisible
) : descriptors_(), stride_(0), type_(heapType), shaderVisible_(bShaderVisible) {
	descriptors_.reserve(capacity);
	auto heapDesc = D3D12_DESCRIPTOR_HEAP_DESC{
		.Type = heapType,
		.NumDescriptors = static_cast<UINT>(capacity),
		.Flags = bShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
			: D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	};

	DX_THROW_FAILED( device.get()->CreateDescriptorHeap(
        &heapDesc, __uuidof(InterfaceType), &src_
    ) );
	stride_ = device.get()->GetDescriptorHandleIncrementSize(heapType);

	auto cpuHandle = src_->GetCPUDescriptorHandleForHeapStart();
	auto gpuHandle = bShaderVisible ? src_->GetGPUDescriptorHandleForHeapStart()
		: D3D12_GPU_DESCRIPTOR_HANDLE{};

	while (capacity--) {
		descriptors_.emplace_back(cpuHandle, gpuHandle, Descriptor::Type::INVALID);
		cpuHandle.ptr += stride_;
		gpuHandle.ptr += stride_;
	}
}

class DescriptorHeapCPU : public DescriptorHeap<DescriptorCPU> {
public:
	DescriptorHeapCPU() = default;
	DescriptorHeapCPU( D3D12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType,
		std::size_t capacity
	) : DescriptorHeap(device, heapType, capacity, false) {}

	D3D12_GPU_DESCRIPTOR_HANDLE& gpuStart() NOEXCEPT = delete;
	const D3D12_GPU_DESCRIPTOR_HANDLE& gpuStart() const NOEXCEPT = delete;
};

class DescriptorHeapGPU : public DescriptorHeap<DescriptorGPU> {
public:
	DescriptorHeapGPU() = default;
	DescriptorHeapGPU(D3D12Device& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType,
		std::size_t capacity
	) : DescriptorHeap(device, heapType, capacity, true) {}
};

template <class TDescHeap>
class DescriptorRange {
public:
	using DescriptorType = typename TDescHeap::DescriptorType;
	using Type = typename DescriptorType::Type;

	DescriptorRange(TDescHeap& srcHeap, std::size_t beginIdx, std::size_t endIdx, Type type)
		: pDescHeap_(&srcHeap), beginIdx_(beginIdx), endIdx_(endIdx), size_(0), type_(type) {}

	void clear() NOEXCEPT {
		size_ = 0;
	}

	DescriptorType& alloc() {
		assert(beginIdx_ + size_ < endIdx);

		auto& ret = (*pDescHeap_)[beginIdx_ + size_];
		ret.setOffset(size_++);
		ret.setType(type_);
		return ret;
	}

	void bind(D3D12GfxCmdList& cmdList, std::size_t rootParamIdx) {
        DX_THROW_FAILED_VOID(
		    cmdList.get()->SetGraphicsRootDescriptorTable(
				static_cast<UINT>( rootParamIdx ),
				(*pDescHeap_)[beginIdx_].gpuHandle()
			)
        );
	}

private:
	TDescHeap* pDescHeap_;
	std::size_t beginIdx_;
	std::size_t endIdx_;
	std::size_t size_;
	Type type_;
};

class D3D12Resource : public dx::DXWrapper<ID3D12Resource> {
public:
	enum class Views {
		CBV, SRV, UAV, RTV, SAM, VBV, IBV
	};

	D3D12Resource()
		: dx::DXWrapper<InterfaceType>(),
		views_(), vbview_{}, desc_{}, gpuAddr_{},
		stride_(0u), state_(D3D12_RESOURCE_STATE_COMMON) {}

	D3D12Resource( const wrl::ComPtr<InterfaceType>& src,
		D3D12_RESOURCE_STATES initialState
	) : dx::DXWrapper<InterfaceType>(src),
		views_(), vbview_{}, desc_(src->GetDesc()),
		gpuAddr_(), stride_(0u),
		state_(initialState) {}

	D3D12Resource(const wrl::ComPtr<InterfaceType>& src)
		: D3D12Resource(src, D3D12_RESOURCE_STATE_COMMON) {}

	D3D12Resource( wrl::ComPtr<InterfaceType>&& src,
		D3D12_RESOURCE_STATES initialState
	) : dx::DXWrapper<InterfaceType>(std::move(src)),
		views_(), vbview_{}, desc_(get()->GetDesc()),
		gpuAddr_(), stride_(0u),
		state_(initialState) {}

	D3D12Resource(wrl::ComPtr<InterfaceType>&& src)
		: D3D12Resource(std::move(src), D3D12_RESOURCE_STATE_COMMON) {}

	D3D12Resource( D3D12Device& device, const D3D12_RESOURCE_DESC& resDesc,
		D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
		const D3D12_CLEAR_VALUE* optimizedClearValue
	);
	D3D12Resource(D3D12Device& device, const D3D12_RESOURCE_DESC& resDesc,
		D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
		std::size_t stride
	);
	D3D12Resource(D3D12Device& device, const D3D12_RESOURCE_DESC& resDesc,
		D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState
	);
	D3D12Resource(D3D12Device& device, const D3D12_RESOURCE_DESC& resDesc,
		D3D12_HEAP_TYPE heapType
	);

	void init( const wrl::ComPtr<InterfaceType>& src,
		D3D12_RESOURCE_STATES initialState
	) {
		get() = src;
		desc_ = get()->GetDesc();
		state_ = initialState;
	}

	void init(const wrl::ComPtr<InterfaceType>& src) {
		init(src, D3D12_RESOURCE_STATE_COMMON);
	}

	void init( wrl::ComPtr<InterfaceType>&& src,
		D3D12_RESOURCE_STATES initialState
	) {
		get() = std::move(src);
		desc_ = get()->GetDesc();
		state_ = initialState;
	}

	void init(wrl::ComPtr<InterfaceType>&& src) {
		init(std::move(src), D3D12_RESOURCE_STATE_COMMON);
	}

	std::size_t makeCbv(const D3D12_CONSTANT_BUFFER_VIEW_DESC& cbvDesc,
		D3D12Device& device, Descriptor& destView
	);
	std::size_t makeSrv(const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
		D3D12Device& device, Descriptor& destView
	);
	std::size_t makeUav(const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc,
		D3D12Device& device, Descriptor& destView
	);
	std::size_t makeRtv(const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc,
		D3D12Device& device, Descriptor& destView
	);
	std::size_t makeDsv(const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
		D3D12Device& device, Descriptor& destView
	);
	void makeIbv(D3D12Device& device, DXGI_FORMAT format, std::size_t cnt, std::size_t byteOffset = 0u);

	void remakeCbv(std::size_t idx, const D3D12_CONSTANT_BUFFER_VIEW_DESC& cbvDesc,
		D3D12Device& device
	);
	void remakeSrv(std::size_t idx, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
		D3D12Device& device
	);
	void remakeUav(std::size_t idx, const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc,
		D3D12Device& device
	);
	void remakeRtv(std::size_t idx, const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc,
		D3D12Device& device
	);
	void remakeDsv(std::size_t idx, const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
		D3D12Device& device
	);

	std::size_t makeDefCbv(D3D12Device& device, Descriptor& destView);
	std::size_t makeDefSrv(D3D12Device& device, Descriptor& destView);
	std::size_t makeDefUav(D3D12Device& device, Descriptor& destView);
	std::size_t makeDefRtv(D3D12Device& device, Descriptor& destView);
	std::size_t makeDefDsv(D3D12Device& device, Descriptor& destView);
	void remakeDefCbv(std::size_t idx, D3D12Device& device);
	void remakeDefSrv(std::size_t idx, D3D12Device& device);
	void remakeDefUav(std::size_t idx, D3D12Device& device);
	void remakeDefRtv(std::size_t idx, D3D12Device& device);
	void remakeDefDsv(std::size_t idx, D3D12Device& device);
	void makeDefVbv(D3D12Device& device);
	void makeDefIbv(D3D12Device& device);

	Descriptor& view(std::size_t idx) {
		return views_.at(idx);
	}

	const Descriptor& view(std::size_t idx) const {
		return views_.at(idx);
	}

	const auto& desc() const NOEXCEPT {
		return desc_;
	}

	const auto& vbview() const NOEXCEPT {
		return vbview_;
	}

	const auto& ibview() const NOEXCEPT {
		return ibview_;
	}

	D3D12_RESOURCE_STATES state() const NOEXCEPT {
		return state_;
	}

	void commitState(D3D12GfxCmdList& cmdList, D3D12_RESOURCE_STATES resState);
	void updateDesc() NOEXCEPT {
		desc_ = get()->GetDesc();
	}

	void setStride(std::size_t stride) NOEXCEPT {
		stride_ = stride;
	}

	std::size_t stride() const NOEXCEPT {
		return stride_;
	}

	void pullGpuAddr() {
		gpuAddr_ = get()->GetGPUVirtualAddress();
	}

	D3D12_GPU_VIRTUAL_ADDRESS gpuAddr() const NOEXCEPT {
		return gpuAddr_;
	}

private:
	std::vector<Descriptor> views_;
	union {
		D3D12_VERTEX_BUFFER_VIEW vbview_;
		D3D12_INDEX_BUFFER_VIEW ibview_;
	};
	D3D12_RESOURCE_DESC desc_;
	D3D12_GPU_VIRTUAL_ADDRESS gpuAddr_;
	std::size_t stride_;	// valid only if the resource was vertex buffer
	D3D12_RESOURCE_STATES state_;
};

class RootSignature : public dx::DXWrapper<ID3D12RootSignature> {
public:
	using dx::DXWrapper<ID3D12RootSignature>::DXWrapper;

	void bind(D3D12GfxCmdList& cmdList) {
        DX_THROW_FAILED_VOID(
		    cmdList.get()->SetGraphicsRootSignature(get().Get())
        );
	}
};

template <class Traits>
class Window : public dx::DXWindow<Traits> {
public:
	using MyBase = dx::DXWindow<Traits>;
    using MyChar = typename Traits::MyChar;
    using MyString = typename Traits::MyString;
    using MyStringView = typename Traits::MyStringView;
    using MyBase::nativeHandle;
    using MyBase::defWndName;
    using MyBase::defWndFrame;
	using MyBase::get;

	void open( dx::DXGIFactory& factory, D3D12Device& device, D3D12CmdQueue& cmdQueue,
		DescriptorRange<DescriptorHeapCPU>& rtvRangeBackBuf,
		DescriptorRange<DescriptorHeapCPU>& dsvRangeBackBuf,
		std::size_t backBufCnt = MyBase::defBackBufCnt,
		const D3D12_CLEAR_VALUE& rtvClearValue = { .Format = DXGI_FORMAT_R8G8B8A8_UNORM, .Color = { 0.0f, 0.0f, 0.0f, 1.0f } },
		const D3D12_CLEAR_VALUE& dsvClearValue = { .Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = { 1.0f, 0u } }
	) {
		open( factory, device, cmdQueue, defWndName(), rtvRangeBackBuf,
			dsvRangeBackBuf, backBufCnt, rtvClearValue, dsvClearValue
		);
	}

	void open( dx::DXGIFactory& factory, D3D12Device& device, D3D12CmdQueue& cmdQueue,
		DescriptorRange<DescriptorHeapCPU>& rtvRangeBackBuf,
		DescriptorRange<DescriptorHeapCPU>& dsvRangeBackBuf,
		const Win32::WndFrame& wndFrame, std::size_t backBufCnt = MyBase::defBackBufCnt,
		const D3D12_CLEAR_VALUE& rtvClearValue = { .Format = DXGI_FORMAT_R8G8B8A8_UNORM, .Color = { 0.0f, 0.0f, 0.0f, 1.0f } },
		const D3D12_CLEAR_VALUE& dsvClearValue = { .Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = { 1.0f, 0u } }
	) {
		open( factory, device, cmdQueue, defWndName(), wndFrame,
			rtvRangeBackBuf, dsvRangeBackBuf, backBufCnt, rtvClearValue, dsvClearValue
		);
	}

	void open( dx::DXGIFactory& factory, D3D12Device& device, D3D12CmdQueue& cmdQueue,
		MyStringView wndName, DescriptorRange<DescriptorHeapCPU>& rtvRangeBackBuf,
		DescriptorRange<DescriptorHeapCPU>& dsvRangeBackBuf,
		std::size_t backBufCnt = MyBase::defBackBufCnt,
		const D3D12_CLEAR_VALUE& rtvClearValue = { .Format = DXGI_FORMAT_R8G8B8A8_UNORM, .Color = { 0.0f, 0.0f, 0.0f, 1.0f } },
		const D3D12_CLEAR_VALUE& dsvClearValue = { .Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = { 1.0f, 0u } }
	) {
		open( factory, device, cmdQueue, wndName, defWndFrame(),
			rtvRangeBackBuf, dsvRangeBackBuf, backBufCnt, rtvClearValue, dsvClearValue
		);
	}

	void open( dx::DXGIFactory& factory, D3D12Device& device, D3D12CmdQueue& cmdQueue,
		MyStringView wndName, const Win32::WndFrame& wndFrame,
		DescriptorRange<DescriptorHeapCPU>& rtvRangeBackBuf,
		DescriptorRange<DescriptorHeapCPU>& dsvRangeBackBuf,
		std::size_t backBufCnt = MyBase::defBackBufCnt,
		const D3D12_CLEAR_VALUE& rtvClearValue = { .Format = DXGI_FORMAT_R8G8B8A8_UNORM, .Color = { 0.0f, 0.0f, 0.0f, 1.0f } },
		const D3D12_CLEAR_VALUE& dsvClearValue = { .Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = { 1.0f, 0u } }
	) {
		MyBase::open(factory, cmdQueue.get().Get(), wndName, wndFrame, backBufCnt);
		buildBuffers(device, rtvClearValue, dsvClearValue);
		buildViews(device, rtvRangeBackBuf, dsvRangeBackBuf);
	}

	void clearRenderTarget(D3D12GfxCmdList& cmdList) {
		DX_THROW_FAILED_VOID( cmdList.get()->ClearRenderTargetView(
			rtvs_[backBufIdx()].cpuHandle(), rtvClearValue_.Color, 0u, nullptr
		) );
	}

	void clearDepthStencil(D3D12GfxCmdList& cmdList) {
		DX_THROW_FAILED_VOID( cmdList.get()->ClearDepthStencilView(
			dsv_.cpuHandle(), D3D12_CLEAR_FLAG_DEPTH, dsvClearValue_.DepthStencil.Depth,
			dsvClearValue_.DepthStencil.Stencil, 0u, nullptr
		) );
	}

	void setRenderTarget(D3D12GfxCmdList& cmdList) {
		backBuffers_[backBufIdx_].commitState(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		DX_THROW_FAILED_VOID( cmdList.get()->OMSetRenderTargets(
			1u, &rtvs_[backBufIdx()].cpuHandle(), FALSE, &dsv_.cpuHandle()
		) );
	}

	DescriptorCPU& curRtv() NOEXCEPT {
		return rtvs_[backBufIdx_];
	}

	DescriptorCPU& curDsv() NOEXCEPT {
		return dsv_;
	}

	void setPresent(D3D12GfxCmdList& cmdList) {
		backBuffers_[backBufIdx_].commitState(cmdList, D3D12_RESOURCE_STATE_PRESENT);
	}

	std::size_t backBufIdx() const NOEXCEPT {
		return backBufIdx_;
	}

	void present(D3D12GfxCmdList& cmdList) {
        MyBase::present();
		backBufIdx_ = get()->GetCurrentBackBufferIndex();
    }

private:
	void buildBuffers(D3D12Device& device, const D3D12_CLEAR_VALUE& rtvClearValue,
		const D3D12_CLEAR_VALUE& dsvClearValue
	);
	void buildViews( D3D12Device& device,
		DescriptorRange<DescriptorHeapCPU>& rtvRangeBackBuf,
		DescriptorRange<DescriptorHeapCPU>& dsvRangeBackBuf
	);
	void rebuildViews(D3D12Device& device);
	void preResizeBuffers(void* pContext) override;
    void postResizeBuffers(void* pContext) override;

	std::vector<D3D12Resource> backBuffers_;
	D3D12Resource depthBuffer_;
	std::vector<DescriptorCPU> rtvs_;
	D3D12_CLEAR_VALUE rtvClearValue_;
	D3D12_CLEAR_VALUE dsvClearValue_;
	DescriptorCPU dsv_;
	std::size_t backBufIdx_;
};

template <class Traits>
void Window<Traits>::buildBuffers( D3D12Device& device,
	const D3D12_CLEAR_VALUE& rtvClearValue, const D3D12_CLEAR_VALUE& dsvClearValue
) {
	rtvClearValue_ = rtvClearValue;
	dsvClearValue_ = dsvClearValue;

	backBuffers_.resize( this->backBufCnt() );
	for (auto i = 0u; i < backBuffers_.size(); ++i) {
		DX_THROW_FAILED( get()->GetBuffer(i,
			__uuidof(D3D12Resource::InterfaceType), &backBuffers_[i].get()
		) );
	}

	depthBuffer_ = D3D12Resource( device, D3D12_RESOURCE_DESC{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0u,
		.Width = static_cast<UINT64>( this->client().width ),
		.Height = static_cast<UINT>( this->client().height ),
		.DepthOrArraySize = 1u,
		.MipLevels = 1u,
		.Format = DXGI_FORMAT_D32_FLOAT,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	}, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_DEPTH_WRITE, &dsvClearValue_ );
}

template <class Traits>
void Window<Traits>::buildViews( D3D12Device& device,
	DescriptorRange<DescriptorHeapCPU>& rtvRangeBackBuf,
	DescriptorRange<DescriptorHeapCPU>& dsvRangeBackBuf
) {
	rtvs_.reserve( backBuffers_.size() );
	for (auto i = 0u; i < backBuffers_.size(); ++i) {
		rtvs_.push_back( rtvRangeBackBuf.alloc() );
		backBuffers_[i].makeDefRtv(device, rtvs_[i]);
	}

	dsv_ = dsvRangeBackBuf.alloc();
	depthBuffer_.makeDefDsv(device, dsv_);
}

template <class Traits>
void Window<Traits>::rebuildViews(D3D12Device& device) {
	for (auto i = 0u; i < backBuffers_.size(); ++i) {
		backBuffers_[i].remakeDefRtv(i, device);
	}

	depthBuffer_.remakeDefDsv(0u, device);
}

template <class Traits>
void Window<Traits>::preResizeBuffers(void* pContext) {
	backBuffers_.clear();
	depthBuffer_ = D3D12Resource();
}

template <class Traits>
void Window<Traits>::postResizeBuffers(void* pContext) {
	buildBuffers(*static_cast<D3D12Device*>(pContext), rtvClearValue_, dsvClearValue_);
	rebuildViews(*static_cast<D3D12Device*>(pContext));
}

template <Win32::Win32Char T>
struct BasicD3D12WTraits : public dx::BasicDXDWTraits<T> {
    using MyWindow = dx::DXWindow<BasicD3D12WTraits>;
    using MyBase = dx::BasicDXDWTraits<T>;
    using MyChar = T;
    using MyString = std::basic_string<MyChar>;
    using MyStringView = std::basic_string_view<MyChar>;

    static constexpr const MyStringView clsName() NOEXCEPT {
        if constexpr ( std::is_same_v<MyChar, CHAR> ) {
            return "D3D12W";
        }
        else /* WCHAR */ {
            return L"D3D12W";
        }
    }
};

inline D3D12_VIEWPORT convClientToVP(const Win32::WndClient& client) NOEXCEPT {
	return D3D12_VIEWPORT{
		.TopLeftX = 0.0f,
		.TopLeftY = 0.0f,
		.Width = static_cast<FLOAT>( client.width ),
		.Height = static_cast<FLOAT>( client.height ),
		.MinDepth = 0.0f,
		.MaxDepth = 1.0f
	};
}

class Fence : public dx::DXWrapper<ID3D12Fence> {
public:
	Fence() = default;
	Fence(D3D12Device& device, UINT64 initValue = 0u);

	void signal(D3D12CmdQueue& cmdQueue, UINT64 value);
	void signal(D3D12CmdQueue& cmdQueue);
	void wait(UINT64 value);
	void wait();

private:
	UINT64 value_;
	HANDLE event_;
};

DXGI_FORMAT convertToDepthFormat(DXGI_FORMAT colorFormat);
DXGI_FORMAT convertToColorFormat(DXGI_FORMAT depthFormat);

}   // namespace gfx::d3d12

}   // namespace gfx

#endif	// __d3d12Low_HPP