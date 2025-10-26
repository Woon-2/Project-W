#include "gfxUtil.hpp"
#include "errorHandling.hpp"

// 버퍼 리소스를 생성하는 함수
// creationType이 UploadBuffer이고 pSrc != nullptr인 경우에만
// pSrc의 내용이 리소스에 복사된다.
// creationType에 관계없이 srcByteWidth는 버퍼의 크기를 나타내므로,
// 반드시 srcByteWidth에 유효한 값을 전달하여야 한다.
ComPtr<ID3D12Resource> createBufferResource(
	ID3D12Device* device, const void* pSrc, UINT64 srcByteWidth,
	BufferCreationType creationType
) {
	auto ret = ComPtr<ID3D12Resource>{};

	// creationType에 따라 heapType, initialState, format이 달라진다.
	auto heapType = D3D12_HEAP_TYPE_DEFAULT;
	auto initialState = D3D12_RESOURCE_STATE_COMMON;

	switch (creationType) {
	case BufferCreationType::VertexBuffer:
		heapType = D3D12_HEAP_TYPE_DEFAULT;
		initialState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		break;

	case BufferCreationType::IndexBuffer:
		heapType = D3D12_HEAP_TYPE_DEFAULT;
		initialState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
		break;

	case BufferCreationType::UploadBuffer:
		heapType = D3D12_HEAP_TYPE_UPLOAD;
		initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
		break;

	default:
		DISPLAY_ERROR_STR(false, L"[GFX Error] createBufferResource: creationType이 알 수 없는 값입니다: "s +
			std::to_wstring(etoi(creationType)) + L"\n"s, false
		);
		return ret;
	}

	// 결정된 인자들을 바탕으로 리소스 생성
	auto heapProperties = D3D12_HEAP_PROPERTIES{
		.Type = heapType,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
		.CreationNodeMask = 0u,
		.VisibleNodeMask = 0u
	};

	auto desc = D3D12_RESOURCE_DESC{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = 0u,
		.Width = srcByteWidth,
		.Height = 1u,
		.DepthOrArraySize = 1u,
		.MipLevels = 1u,
		.Format = DXGI_FORMAT_UNKNOWN,	// D3D12_RESOURCE_DIMENSION_BUFFER를 갖는 버퍼에 대해서는
										// 무조건 Format이 DXGI_FORMAT_UNKNOWN이어야 한다.
		.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1u, .Quality = 0u },
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_NONE
	};

	DISPLAY_ERROR_DX_HR(
		device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr,
			__uuidof(ID3D12Resource), &ret
		), false
	);

	// 버퍼 타입이 업로드 UploadBuffer인 경우 pSrc의 데이터를 리소스에 매핑한다.
	if (creationType == BufferCreationType::UploadBuffer && pSrc) {
		void* mem = nullptr;
		DISPLAY_ERROR_DX_HR( ret->Map(0, nullptr, reinterpret_cast<void**>(&mem)), false );
		std::memcpy(mem, pSrc, static_cast<std::size_t>(srcByteWidth));
		ret->Unmap(0, nullptr);
	}

	return ret;
}

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
void transitionResourceState(ID3D12GraphicsCommandList* cmdList,
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
	DISPLAY_ERROR_DX_VOID(cmdList->ResourceBarrier(1u, &barrier), false);
}

// 리소스 상태 전환과 관련된 사항을 간략화하는 리소스 복사 함수
void copyResource(ID3D12GraphicsCommandList* cmdList,
	ID3D12Resource* srcRes, ID3D12Resource* destRes,
	D3D12_RESOURCE_STATES srcResState, D3D12_RESOURCE_STATES destResState
) {
	transitionResourceState(cmdList, destRes, destResState, D3D12_RESOURCE_STATE_COPY_DEST);
	transitionResourceState(cmdList, srcRes, srcResState, D3D12_RESOURCE_STATE_COPY_SOURCE);
	DISPLAY_ERROR_DX_VOID( cmdList->CopyResource(destRes, srcRes), false );
	transitionResourceState(cmdList, destRes, D3D12_RESOURCE_STATE_COPY_DEST, destResState);
	transitionResourceState(cmdList, srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE, srcResState);
}