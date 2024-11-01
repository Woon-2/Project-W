#ifndef __d3d12Low_HPP
#define __d3d12Low_HPP

#include "config.hpp"

#include "dxtarget.hpp"
#include <directx/d3dx12.h>
#include <directx/d3d12.h>

#include "dxexcept.hpp"

#include "window.hpp"

#include <vector>

namespace gfx {

namespace d3d12 {

template <class TInterface>
class D3DWrapper {
public:
	using InterfaceType = TInterface;

	D3DWrapper() = default;

	D3DWrapper(const wrl::ComPtr<InterfaceType>& src) NOEXCEPT
		: src_(src) {}

	D3DWrapper(wrl::ComPtr<InterfaceType>&& src) NOEXCEPT
		: src_(std::move(src)) {}

	template <class T>
	wrl::ComPtr<T> as() {
		wrl::ComPtr<T> ret{};
		src_.As<T>(&ret);
		return ret;
	}

	auto& get() NOEXCEPT {
		return src_;
	}

	const auto& get() const NOEXCEPT {
		return src_;
	}

protected:
	wrl::ComPtr<InterfaceType> src_;
};

class DXGIAdapter : public D3DWrapper<IDXGIAdapter1> {
public:
	using D3DWrapper<IDXGIAdapter1>::D3DWrapper;
};

class D3D12Device : public D3DWrapper<ID3D12Device> {
public:
	D3D12Device(DXGIAdapter& adapter, D3D_FEATURE_LEVEL featureLevel);
	D3D12Device(DXGIAdapter&& adapter, D3D_FEATURE_LEVEL featureLevel);
};

class DXGIFactory : public D3DWrapper<IDXGIFactory4> {
public:
	DXGIFactory();
	DXGIAdapter getAvailableAdapter(D3D_FEATURE_LEVEL featureLevel);
};

class D3D12CmdQueue : public D3DWrapper<ID3D12CommandQueue> {
public:
	D3D12CmdQueue(D3D12Device& device);
};

class D3D12GfxCmdList : public D3DWrapper<ID3D12GraphicsCommandList> {
public:
	using Allocator = ID3D12CommandAllocator;

	D3D12GfxCmdList(D3D12Device& device);

	void copyResource(class D3D12Resource& srcRes, D3D12Resource& destRes);

private:
	wrl::ComPtr<Allocator> alloc_;
};

// class D3D12Window : public Win32::Window, public D3DWrapper<IDXGISwapChain3> {
// public:
// 	D3D12Window(const RECT& clientRect, LPCWSTR wndName, DXGIFactory& factory, D3D12CmdQueue& cmdQueue);
// };

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

	const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle() const NOEXCEPT {
		return gpuHandle_;
	}
};

template <class TDescriptor>
class DescriptorHeap : public D3DWrapper<ID3D12DescriptorHeap> {
public:
	using DescriptorType = TDescriptor;

	DescriptorHeap()
		: D3DWrapper(), descriptors_(), stride_(0),
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
	auto gpuHandle = src_->GetGPUDescriptorHandleForHeapStart();

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

	DescriptorRange(const TDescHeap& srcHeap, std::size_t beginIdx, std::size_t endIdx, Type type)
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
		    cmdList.get()->SetGraphicsRootDescriptorTable(rootParamIdx, (*pDescHeap_)[beginIdx_])
        );
	}

private:
	TDescHeap* pDescHeap_;
	std::size_t beginIdx_;
	std::size_t endIdx_;
	std::size_t size_;
	Type type_;
};

class D3D12Resource : public D3DWrapper<ID3D12Resource> {
public:
	enum class Views {
		CBV, SRV, UAV, RTV, SAM, VBV, IBV
	};

	D3D12Resource()
		: D3DWrapper<InterfaceType>(),
		views_(), vbview_{}, desc_{}, gpuAddr_{},
		stride_(0u), state_(D3D12_RESOURCE_STATE_COMMON) {}

	D3D12Resource( const wrl::ComPtr<InterfaceType>& src,
		D3D12_RESOURCE_STATES initialState
	) : D3DWrapper<InterfaceType>(src),
		views_(), vbview_{}, desc_(src->GetDesc()),
		gpuAddr_(src->GetGPUVirtualAddress()), stride_(0u),
		state_(initialState) {}

	D3D12Resource(const wrl::ComPtr<InterfaceType>& src)
		: D3D12Resource(src, D3D12_RESOURCE_STATE_COMMON) {}

	D3D12Resource( wrl::ComPtr<InterfaceType>&& src,
		D3D12_RESOURCE_STATES initialState
	) : D3DWrapper<InterfaceType>(std::move(src)),
		views_(), vbview_{}, desc_(src->GetDesc()),
		gpuAddr_(src->GetGPUVirtualAddress()), stride_(0u),
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

	D3D12Resource(const D3D12Resource& other) = delete;
	D3D12Resource& operator=(const D3D12Resource& other) = delete;

	void init( const wrl::ComPtr<InterfaceType>& src,
		D3D12_RESOURCE_STATES initialState
	) {
		get() = src;
		desc_ = src->GetDesc();
		gpuAddr_ = src->GetGPUVirtualAddress();
		state_ = initialState;
	}

	void init(const wrl::ComPtr<InterfaceType>& src) {
		init(src, D3D12_RESOURCE_STATE_COMMON);
	}

	void init( wrl::ComPtr<InterfaceType>&& src,
		D3D12_RESOURCE_STATES initialState
	) {
		get() = std::move(src);
		desc_ = src->GetDesc();
		gpuAddr_ = src->GetGPUVirtualAddress();
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

	std::size_t makeDefCbv(D3D12Device& device, Descriptor& destView);
	std::size_t makeDefSrv(D3D12Device& device, Descriptor& destView);
	std::size_t makeDefUav(D3D12Device& device, Descriptor& destView);
	std::size_t makeDefRtv(D3D12Device& device, Descriptor& destView);
	std::size_t makeDefDsv(D3D12Device& device, Descriptor& destView);
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

class RootSignature : public D3DWrapper<ID3D12RootSignature> {
public:
	using D3DWrapper<ID3D12RootSignature>::D3DWrapper;

	void bind(D3D12GfxCmdList& cmdList) {
        DX_THROW_FAILED_VOID(
		    cmdList.get()->SetGraphicsRootSignature(get().Get())
        );
	}
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif	// __d3d12Low_HPP