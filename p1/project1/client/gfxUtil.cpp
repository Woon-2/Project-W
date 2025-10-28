#include "gfxUtil.hpp"
#include "errorHandling.hpp"

// 버퍼 리소스를 생성하는 함수
// creationType이 UploadBuffer이고 pSrc != nullptr인 경우에만
// pSrc의 내용이 리소스에 복사된다.
// creationType에 관계없이 srcByteWidth는 버퍼의 크기를 나타내므로,
// 반드시 srcByteWidth에 유효한 값을 전달하여야 한다.
// creationType에 따라 힙 타입과 리소스 초기 상태를 다르게 설정한다.
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

void CommandListPool::init(ID3D12Device* device, CommandListUsage usage, std::size_t cnt) {
	std::wstring usageName{};

	switch (usage) {
	case CommandListUsage::RenderingMaster:
		usageName = L"RenderingMaster_";
		break;

	case CommandListUsage::RenderingSlave:
		usageName = L"RenderingSlave_";
		break;

	case CommandListUsage::ResourceLoading:
		usageName = L"ResourceLoading_";
		break;

	default:
		DISPLAY_ERROR_STR( false, L"[GFX Error]: CommandListPool::init: 존재하지 않는 CommandListUsage 값"s
			+ std::to_wstring(etoi(usage)) + L"이(가) 전달되었습니다.", true
		);
		break;
	}

	for (std::size_t i = 0; i < cnt; ++i) {
		auto& ctx = cmdCtxs_[etoi(usage)].emplace_back();
		DISPLAY_ERROR_DX_HR(
			device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &ctx.cmdAlloc
			), true
		);
		setD3DName(ctx.cmdAlloc.Get(), std::wstring(L"CommandAllocator_") + usageName + std::to_wstring(i));

		DISPLAY_ERROR_DX_HR(
			device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, ctx.cmdAlloc.Get(), nullptr,
				__uuidof(ID3D12GraphicsCommandList), &ctx.cmdList
			), true
		);
		setD3DName(ctx.cmdAlloc.Get(), std::wstring(L"CommandList_") + usageName + std::to_wstring(i));
		// 생성됐을 땐 open 상태이므로,
		// render 호출 시 이미 open 상태인 Command List를 open할 수 있다.
		// 따라서 여기서 close 해준다.
		ctx.cmdList->Close();
	}
}

std::size_t CommandListPool::alloc( std::size_t cnt,
	CommandListUsage usage, std::list<CommandContext>& output
) {
	std::size_t allocCnt = 0u;

	auto& pool = cmdCtxs_[etoi(usage)];

	auto pLast = pool.end();
	if (pool.size() >= cnt) {
		pLast = std::next(pool.begin(), cnt);
		allocCnt = cnt;
	}
	else {
		allocCnt = std::distance(pool.begin(), pool.end());
	}

	output.splice(output.end(), pool, pool.begin(), pLast);

	return allocCnt;
}

bool CommandListPool::allocOne(CommandListUsage usage, CommandContext& output) {
	auto& pool = cmdCtxs_[etoi(usage)];
	if (pool.empty()) {
		return false;
	}

	output = std::move(pool.front());
	pool.pop_front();
	return true;
}

void CommandListPool::free(CommandListUsage usage, std::list<CommandContext>&& expired) {
	free(etoi(usage), std::move(expired));
}

void CommandListPool::free(int usage, std::list<CommandContext>&& expired) {
	auto& pool = cmdCtxs_[usage];
	pool.splice(pool.end(), std::move(expired));
}

void CommandListPool::freeOne(CommandListUsage usage, CommandContext&& expired) {
	auto& pool = cmdCtxs_[etoi(usage)];
	pool.push_back(std::move(expired));
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

// 이미 만들어진 리소스를 가져와서, 메모리 영역을 분배받아 사용하는 경우
// 이 생성자를 호출한다.
// 리소스의 [addressOffset, addressOffset + allowedByteWidth) 영역을 분배받는다.
// CreateCommittedResource 호출을 줄이고 gpu 메모리 사용량을 줄이는 이점이 있다.
ShaderInputBuffer::ShaderInputBuffer( const std::vector<ComPtr<ID3D12Resource>>& premadeResources,
	std::size_t addressOffset, std::size_t allowedByteWidth, const std::wstring& name
) : ShaderInputBuffer() {
	for (const auto& premadeRes : premadeResources) {
		auto address = premadeRes->GetGPUVirtualAddress();
		address += addressOffset;
		u8t* mappedRegion = nullptr;
		DISPLAY_ERROR_DX_HR( premadeRes->Map(0u, nullptr, reinterpret_cast<void**>(&mappedRegion)), false );
		mappedRegion += addressOffset;

		resources_.push_back(premadeRes);
		addresses_.push_back(address);
		mappedRegions_.push_back(mappedRegion);
	}
	byteWidth_ = allowedByteWidth;
	name_ = name;
}

// 기본 생성자를 호출하고 init을 호출하는 것과 같다.
ShaderInputBuffer::ShaderInputBuffer( ID3D12Device* device, UINT64 byteWidth,
	std::size_t roomCnt, const std::wstring& name
) : ShaderInputBuffer() {
	init(device, byteWidth, roomCnt, name);
}

// byteWidth 크기의 리소스를 roomCnt 개 만큼 만든다.
void ShaderInputBuffer::init( ID3D12Device* device, UINT64 byteWidth,
	std::size_t roomCnt, const std::wstring& name
) {
	for (std::size_t i = 0u; i < roomCnt; ++i) {
		auto res = createBufferResource(device, nullptr, byteWidth, BufferCreationType::UploadBuffer);
		setD3DName(res.Get(), name + std::to_wstring(i));
		auto address = res->GetGPUVirtualAddress();
		void* mappedRegion = nullptr;
		DISPLAY_ERROR_DX_HR( res->Map(0u, nullptr, &mappedRegion), false );

		resources_.push_back(std::move(res));
		addresses_.push_back(address);
		mappedRegions_.push_back(mappedRegion);
	}
	byteWidth_ = byteWidth;
	name_ = name;
}

// roomIdx 리소스의 gpu 데이터를 data와 동기화한다.
void ShaderInputBuffer::stage(std::size_t roomIdx, const void* data, std::size_t byteWidth) {
	if (byteWidth > byteWidth_) {
		DISPLAY_ERROR_STR(byteWidth > byteWidth_, L"[GFX Error] ShaderInputBuffer::stage: "s +
			L"버퍼에 할당된 "s + std::to_wstring(byteWidth_) + L" 바이트를 넘어 "s
			+ std::to_wstring(byteWidth) + L" 바이트를 복사하려고 시도했습니다.\n", false
		);
		return;
	}
	std::memcpy(mappedRegions_.at(roomIdx), data, byteWidth);
}

// roomIdx의 리소스를 루트 시그너처에 cbv로 연결한다.
void ConstantBuffer::bind(ID3D12GraphicsCommandList* cmdList, UINT rootParamIdx, std::size_t roomIdx) {
	DISPLAY_ERROR_DX_VOID(
		cmdList->SetGraphicsRootConstantBufferView(rootParamIdx, addresses_.at(roomIdx)),
		false
	);
}

// roomIdx의 리소스를 루트 시그너처에 srv로 연결한다.
void StructuredBuffer::bind(ID3D12GraphicsCommandList* cmdList, UINT rootParamIdx, std::size_t roomIdx) {
	DISPLAY_ERROR_DX_VOID(
		cmdList->SetGraphicsRootShaderResourceView(rootParamIdx, addresses_.at(roomIdx)),
		false
	);
}

// ConstantBufferArray 객체를 생성한다.
// room 개수만큼의 큰 리소스들을 생성하고
// 그 리소스들을 기반으로 ConstantBuffer들을 생성해 담는다.
ConstantBufferArray createConstantBufferArray( ID3D12Device* device, UINT64 elemByteWidth,
	std::size_t elemCnt, std::size_t roomCnt, const std::wstring& name
) {
	// ConstantBuffer의 주소는 256바이트 단위로 정렬되어 있어야 한다.
	elemByteWidth = (elemByteWidth + 255ull) & ~255ull;

	ConstantBufferArray ret{};

	for (std::size_t i = 0u ; i < roomCnt; ++i) {
		auto res = createBufferResource( device, nullptr, 
			static_cast<UINT64>(elemByteWidth * elemCnt), BufferCreationType::UploadBuffer
		);
		setD3DName(res.Get(), name + std::to_wstring(i));
		ret.sharedResources.push_back(std::move(res));
	}

	ret.cbuffers.reserve(elemCnt);
	for (std::size_t i = 0u; i < elemCnt; ++i) {
		ret.cbuffers.emplace_back( ret.sharedResources, elemByteWidth * i, static_cast<UINT64>(elemByteWidth),
			name + std::to_wstring(i)
		);
	}

	return ret;
}