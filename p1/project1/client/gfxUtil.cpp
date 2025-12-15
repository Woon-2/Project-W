#include "pch.hpp"
#include "gfxUtil.hpp"
#include "errorHandling.hpp"
#include "binaryImport.hpp"

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
		DISPLAY_ERROR_STR(false, "[GFX Error] createBufferResource: creationType이 알 수 없는 값입니다: "s +
			std::to_string(etoi(creationType)) + "\n"s, false
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

// 특정 용도 usage의 명령 컨텍스트 cnt개를 풀 내부에 확충해놓는다.
void CommandListPool::init(ID3D12Device* device, CommandListUsage usage, std::size_t cnt) {
	// 디버깅을 위해 usage enum을 문자열로 매핑한다.
	// 추후 D3D12 객체 이름 지정 및 오류 메시지 출력에 쓰인다.
	std::string usageName{};

	switch (usage) {
	case CommandListUsage::RenderingMaster:
		usageName = "RenderingMaster_";
		break;

	case CommandListUsage::RenderingSlave:
		usageName = "RenderingSlave_";
		break;

	case CommandListUsage::ResourceLoading:
		usageName = "ResourceLoading_";
		break;

	default:
		DISPLAY_ERROR_STR( false, "[GFX Error]: CommandListPool::init: 존재하지 않는 CommandListUsage 값"s
			+ std::to_string(etoi(usage)) + "이(가) 전달되었습니다.", true
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
		setD3DName(ctx.cmdAlloc.Get(), std::string("CommandAllocator_") + usageName + std::to_string(i));

		DISPLAY_ERROR_DX_HR(
			device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, ctx.cmdAlloc.Get(), nullptr,
				__uuidof(ID3D12GraphicsCommandList), &ctx.cmdList
			), true
		);
		setD3DName(ctx.cmdList.Get(), std::string("CommandList_") + usageName + std::to_string(i));
		// 생성됐을 땐 open 상태이므로,
		// render 호출 시 이미 open 상태인 Command List를 open할 수 있다.
		// 따라서 여기서 close 해준다.
		ctx.cmdList->Close();
	}
}

// 특정 용도 usage의 명령 컨텍스트를 cnt개 할당해 output에 채워넣는다.
// @return 실제로 할당한 명령 컨텍스트 수, 요청 받은 개수 cnt와 다를 수 있다.
std::size_t CommandListPool::alloc( std::size_t cnt,
	CommandListUsage usage, std::list<CommandContext>& output
) {
	// 할당 가능한 만큼만 할당한다.
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

// 특정 용도 usage의 명령 컨텍스트 1개를 output에 채워넣는다.
// @return 명령 컨텍스트의 할당 성공 여부, 실패할 수도 있다.
bool CommandListPool::allocOne(CommandListUsage usage, CommandContext& output) {
	auto& pool = cmdCtxs_[etoi(usage)];
	if (pool.empty()) {
		return false;
	}

	output = std::move(pool.front());
	pool.pop_front();
	return true;
}

// 특정 용도 usage의 명령 컨텍스트들 expired를 풀에 반납한다.
// 반드시 GPU에서도 사용이 끝난 명령 컨텍스트들임을 확인하자.
void CommandListPool::free(CommandListUsage usage, std::list<CommandContext>&& expired) {
	free(etoi(usage), std::move(expired));
}

// 특정 용도 usage의 명령 컨텍스트들 expired를 풀에 반납한다.
// 반드시 GPU에서도 사용이 끝난 명령 컨텍스트들임을 확인하자.
// * 반복문에서의 쓰임을 상정해 enum 대신 int를 받는 버전을 마련하였다.
//   범위 점검을 따로 하지 않으므로 주의해서 쓰자.
void CommandListPool::free(int usage, std::list<CommandContext>&& expired) {
	auto& pool = cmdCtxs_[usage];
	pool.splice(pool.end(), std::move(expired));
}

// 특정 용도 usage의 명령 컨텍스트 expired를 풀에 반납한다.
// 반드시 GPU에서도 사용이 끝난 명령 컨텍스트임을 확인하자.
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
		// gpuVisible 플래그가 활성화되어있을 때에만
		// gpuStart 값을 유효한 값으로 채운다.
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

// 루트 시그너처의 idxRootParam 번째 루트 파라미터에
// SetGraphicsRootDescriptorTable 함수를 통해 풀을 바인드한다.
void DescriptorPool::bind(ID3D12GraphicsCommandList* cmdList, UINT idxRootParam) const {
	DISPLAY_ERROR_DX_VOID(
		cmdList->SetGraphicsRootDescriptorTable(idxRootParam, gpuStart_), false
	);
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
	std::size_t addressOffset, std::size_t allowedByteWidth, const std::string& name
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
	std::size_t roomCnt, const std::string& name
) : ShaderInputBuffer() {
	init(device, byteWidth, roomCnt, name);
}

// byteWidth 크기의 리소스를 roomCnt 개 만큼 만든다.
void ShaderInputBuffer::init( ID3D12Device* device, UINT64 byteWidth,
	std::size_t roomCnt, const std::string& name
) {
	for (std::size_t i = 0u; i < roomCnt; ++i) {
		auto res = createBufferResource(device, nullptr, byteWidth, BufferCreationType::UploadBuffer);
		setD3DName(res.Get(), name + std::to_string(i));
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
		DISPLAY_ERROR_STR(byteWidth > byteWidth_, "[GFX Error] ShaderInputBuffer::stage: "s +
			"버퍼에 할당된 "s + std::to_string(byteWidth_) + " 바이트를 넘어 "s
			+ std::to_string(byteWidth) + " 바이트를 복사하려고 시도했습니다.\n", false
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
	std::size_t elemCnt, std::size_t roomCnt, const std::string& name
) {
	// ConstantBuffer의 주소는 256바이트 단위로 정렬되어 있어야 한다.
	elemByteWidth = (elemByteWidth + 255ull) & ~255ull;

	ConstantBufferArray ret{};

	for (std::size_t i = 0u ; i < roomCnt; ++i) {
		auto res = createBufferResource( device, nullptr, 
			static_cast<UINT64>(elemByteWidth * elemCnt), BufferCreationType::UploadBuffer
		);
		setD3DName(res.Get(), name + std::to_string(i));
		ret.sharedResources.push_back(std::move(res));
	}

	ret.cbuffers.reserve(elemCnt);
	for (std::size_t i = 0u; i < elemCnt; ++i) {
		ret.cbuffers.emplace_back( ret.sharedResources, elemByteWidth * i, static_cast<UINT64>(elemByteWidth),
			name + std::to_string(i)
		);
	}

	return ret;
}

// dds 포맷의 이미지 파일을 로드한다.
// UpdateSubresources 함수에서 쓰인 임시 업로드 버퍼가 펜스에 연관되므로,
// 펜스에서 gpu 작업 완료를 확인하고 난 뒤에 임시 업로드 버퍼들을 해제할 필요가 있다.
LoadDDSReturnType loadDDS(
    ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
    const std::filesystem::path& path, Fence& fenceToAssociate
) {
	auto ret = LoadDDSReturnType{};

    auto alphaMode = DirectX::DDS_ALPHA_MODE_UNKNOWN;
    ret.isCubemap = false;

	DISPLAY_ERROR_DX_HR(
		DirectX::LoadDDSTextureFromFileEx(
            device,
            path.wstring().c_str(),
            0,
            D3D12_RESOURCE_FLAG_NONE,
            DirectX::DDS_LOADER_DEFAULT,
            &ret.res,
            ret.ddsData,
            ret.subresources,
            &alphaMode,
            &ret.isCubemap
        ), false
	);

	if (ret.subresources.empty()) {
		return ret;
	}

    auto requiredBytes = GetRequiredIntermediateSize(ret.res.Get(), 0, static_cast<UINT>(ret.subresources.size()));

	auto uploadBuffer = createBufferResource(device, nullptr, requiredBytes, BufferCreationType::UploadBuffer);

	DISPLAY_ERROR_DX_VOID(
		UpdateSubresources(cmdList, ret.res.Get(), uploadBuffer.Get(),
			0, 0, static_cast<UINT>(ret.subresources.size()), ret.subresources.data()
		), false
	);

    fenceToAssociate.associatedResources_.push_back(std::move(uploadBuffer));

    return ret;
}

// 인자로 전달받은 샘플러 속성에 맞는 샘플러의 bindless 인덱스를 계산한다.
// 위의 Samplers enum 중 하나의 값이 리턴된다.
// 만약, 해당하는 샘플러가 없다면 0이 리턴되고 오류 메시지를 출력한다.
UINT calcIdxBindlessSampler( D3D12_FILTER filterMode,
	D3D12_TEXTURE_ADDRESS_MODE addrModeU, D3D12_TEXTURE_ADDRESS_MODE addrModeV,
	D3D12_TEXTURE_ADDRESS_MODE addrModeW, UINT anisoLevel
) {
	// 0 - NearestWrap
	if (filterMode == D3D12_FILTER_MIN_MAG_MIP_POINT
		&& addrModeU == D3D12_TEXTURE_ADDRESS_MODE_WRAP
		&& addrModeV == D3D12_TEXTURE_ADDRESS_MODE_WRAP
		&& addrModeW == D3D12_TEXTURE_ADDRESS_MODE_WRAP
	) {
		return 0;
	}
	// 1 - BilinearWrap
	else if (filterMode == D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT
		&& addrModeU == D3D12_TEXTURE_ADDRESS_MODE_WRAP
		&& addrModeV == D3D12_TEXTURE_ADDRESS_MODE_WRAP
		&& addrModeW == D3D12_TEXTURE_ADDRESS_MODE_WRAP
	) {
		return 1;
	}
	// 2 - TrilinearWrap
	else if (filterMode == D3D12_FILTER_MIN_MAG_MIP_LINEAR
		&& addrModeU == D3D12_TEXTURE_ADDRESS_MODE_WRAP
		&& addrModeV == D3D12_TEXTURE_ADDRESS_MODE_WRAP
		&& addrModeW == D3D12_TEXTURE_ADDRESS_MODE_WRAP
	) {
		return 2;
	}
	// 3 - NearestBorder
	else if (filterMode == D3D12_FILTER_MIN_MAG_MIP_POINT
		&& addrModeU == D3D12_TEXTURE_ADDRESS_MODE_BORDER
		&& addrModeV == D3D12_TEXTURE_ADDRESS_MODE_BORDER
		&& addrModeW == D3D12_TEXTURE_ADDRESS_MODE_BORDER
	) {
		return 3;
	}
	// 4 - BilinearBorder
	else if (filterMode == D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT
		&& addrModeU == D3D12_TEXTURE_ADDRESS_MODE_BORDER
		&& addrModeV == D3D12_TEXTURE_ADDRESS_MODE_BORDER
		&& addrModeW == D3D12_TEXTURE_ADDRESS_MODE_BORDER
	) {
		return 4;
	}
	// 5 - TrilinerBorder
	else if (filterMode == D3D12_FILTER_MIN_MAG_MIP_LINEAR
		&& addrModeU == D3D12_TEXTURE_ADDRESS_MODE_BORDER
		&& addrModeV == D3D12_TEXTURE_ADDRESS_MODE_BORDER
		&& addrModeW == D3D12_TEXTURE_ADDRESS_MODE_BORDER
	) {
		return 5;
	}
	// 6 - NearestClamp
	else if (filterMode == D3D12_FILTER_MIN_MAG_MIP_POINT
		&& addrModeU == D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		&& addrModeV == D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		&& addrModeW == D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	) {
		return 6;
	}
	// 7 - BilinearClamp
	else if (filterMode == D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT
		&& addrModeU == D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		&& addrModeV == D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		&& addrModeW == D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	) {
		return 7;
	}
	// 8 - TrilinearClamp
	else if (filterMode == D3D12_FILTER_MIN_MAG_MIP_LINEAR
		&& addrModeU == D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		&& addrModeV == D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		&& addrModeW == D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	) {
		return 8;
	}
	// 0 - NearestComparison
	else if (filterMode == D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT
		&& addrModeU == D3D12_TEXTURE_ADDRESS_MODE_BORDER
		&& addrModeV == D3D12_TEXTURE_ADDRESS_MODE_BORDER
		&& addrModeW == D3D12_TEXTURE_ADDRESS_MODE_BORDER
	) {
		return 0;
	}
	// 1 - BilinearComparison
	else if (filterMode == D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT
		&& addrModeU == D3D12_TEXTURE_ADDRESS_MODE_BORDER
		&& addrModeV == D3D12_TEXTURE_ADDRESS_MODE_BORDER
		&& addrModeW == D3D12_TEXTURE_ADDRESS_MODE_BORDER
	) {
		return 1;
	}
	else {
		DISPLAY_ERROR_STR(false, "[GFX Error] calcIdxBindlessSampler: 요구된 샘플링 방식에 대응되는 샘플러가 없습니다.\n"
			"Samplers enum과 calcIdxBindlessSampler 함수, 그리고 createSampler 함수를 업데이트하세요.", false
		);
		return 0;
	}
}

// dds 포맷의 파일로부터 텍스처를 로드한다.
// UpdateSubresources 함수에서 쓰인 임시 업로드 버퍼가 펜스에 연관되므로,
// 펜스에서 gpu 작업 완료를 확인하고 난 뒤에 임시 업로드 버퍼들을 해제할 필요가 있다.
Texture loadTexture( ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	const std::filesystem::path& path, Fence& fenceToAssociate, Texture::Type& texType
) {
	Texture ret{};

	auto tmp = loadDDS(device, cmdList, path, fenceToAssociate);
	ret.res = std::move(tmp.res);

	if (tmp.subresources.size() == 1u) {
		texType = Texture::Type::Tex2D;
	}
	else if (tmp.isCubemap) {
		texType = Texture::Type::TexCube;
	}
	else {
		texType = Texture::Type::Tex2DArray;
	}

	gSharedLog << "[Resource Load] File I/O: 텍스처 " << path << " 로드 완료\n";

	return ret;
}

// 커스텀 텍스처(Texture2D)를 생성한다.
// bindless index 관련 설정은 하지 않으므로
// createSRV, calcIdxBindlessSampler와 같은 함수를 적절히 활용하여
// bindless index들을 설정하자.
Texture createTexture( ID3D12Device* device, std::uint32_t width, std::uint32_t height,
	DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState,
	const D3D12_CLEAR_VALUE& optimizedClearValue
) {
	Texture ret{};

	// 결정된 인자들을 바탕으로 리소스 생성
	auto heapProperties = D3D12_HEAP_PROPERTIES{
		.Type = D3D12_HEAP_TYPE_DEFAULT,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
		.CreationNodeMask = 0u,
		.VisibleNodeMask = 0u
	};

	auto desc = D3D12_RESOURCE_DESC{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0u,
		.Width = width,
		.Height = height,
		.DepthOrArraySize = 1u,
		.MipLevels = 1u,
		.Format = format,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = flags
	};

	DISPLAY_ERROR_DX_HR(
		device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, initialState, &optimizedClearValue,
			__uuidof(ID3D12Resource), &ret.res
		), false
	);

	return ret;
}

// 텍스처의 gpu 리소스를 담는 ComPtr 부분은 제외하고,
// Bindless 셰이더에서 인덱싱하기 위한 인덱스들만 복사한 텍스처를 반환한다.
// 리소스 자체에 접근할 일은 거의 없기 때문에, 인덱스들만 복사하는 것이
// 대부분의 상황에서 용이하다.
Texture cloneTextureIdxOnly(const Texture& tex) {
	Texture ret{};

	ret.idxRtv = tex.idxRtv;
	ret.idxDsv = tex.idxDsv;
	ret.idxSrv = tex.idxSrv;
	ret.idxUav = tex.idxUav;

	return ret;
}

void createResourcePair( ID3D12Resource** outDefaultTex, ID3D12Resource** outUploadTex, ID3D12Device* device, std::uint32_t width, std::uint32_t height, DXGI_FORMAT format )
{
	ID3D12Resource* pTexResource = nullptr;
	ID3D12Resource* pUploadBuffer = nullptr;
	
	auto heapProperties = D3D12_HEAP_PROPERTIES{
		.Type = D3D12_HEAP_TYPE_DEFAULT,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
		.CreationNodeMask = 1u,
		.VisibleNodeMask = 1u
	};

	auto desc = D3D12_RESOURCE_DESC{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0u,
		.Width = width,
		.Height = height,
		.DepthOrArraySize = 1u,
		.MipLevels = 1u,
		.Format = format,
		.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1u, .Quality = 0u },
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_NONE
	};

	DISPLAY_ERROR_DX_HR(
		device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nullptr,
			IID_PPV_ARGS( &pTexResource )
		), true
	);

	UINT64 uploadBufferSize = GetRequiredIntermediateSize( pTexResource, 0, 1 );
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = 0;                          
	desc.Width = uploadBufferSize;           // 버퍼 바이트 크기
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;        // 버퍼는 UNKNOWN
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // 버퍼는 ROW_MAJOR
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	DISPLAY_ERROR_DX_HR(
		device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS( &pUploadBuffer )
		), false
	);

	*outDefaultTex = pTexResource;
	*outUploadTex = pUploadBuffer;
}

void UpdateTexture( ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12Resource* pDestTexResource, ID3D12Resource* pSrcTexResource )
{
	const DWORD MAX_SUB_RESOURCE_NUM = 32;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint[MAX_SUB_RESOURCE_NUM] = {};
	UINT	Rows[MAX_SUB_RESOURCE_NUM] = {};
	UINT64	RowSize[MAX_SUB_RESOURCE_NUM] = {};
	UINT64	TotalBytes = 0;

	D3D12_RESOURCE_DESC Desc = pDestTexResource->GetDesc();
	if ( Desc.MipLevels > (UINT)_countof( Footprint ) )
		__debugbreak();

	device->GetCopyableFootprints( &Desc, 0, Desc.MipLevels, 0, Footprint, Rows, RowSize, &TotalBytes );

	transitionResourceState( cmdList, pDestTexResource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST );
	for ( DWORD i = 0; i < Desc.MipLevels; i++ )
	{
		D3D12_TEXTURE_COPY_LOCATION	destLocation = {};
		destLocation.PlacedFootprint = Footprint[i];
		destLocation.pResource = pDestTexResource;
		destLocation.SubresourceIndex = i;
		destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		D3D12_TEXTURE_COPY_LOCATION	srcLocation = {};
		srcLocation.PlacedFootprint = Footprint[i];
		srcLocation.pResource = pSrcTexResource;
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

		cmdList->CopyTextureRegion( &destLocation, 0, 0, 0, &srcLocation, nullptr );
	}
	transitionResourceState( cmdList, pDestTexResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE );
}

void __createRTV( ID3D12Device* device, Texture& tex,
	const D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc, DescriptorPool& pool
) {
	tex.idxRtv = pool.alloc();
	auto rtvHandle = pool.cpuHandle(tex.idxRtv);
	DISPLAY_ERROR_DX_VOID(
		device->CreateRenderTargetView(tex.res.Get(), rtvDesc, rtvHandle), false
	);
}

// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 RTV를 만든다.
// 텍스처의 idxRtv에 풀에서의 인덱스가 저장된다.
void createRTV(ID3D12Device* device, Texture& tex, DescriptorPool& pool) {
	__createRTV(device, tex, nullptr, pool);
}

// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 RTV를 만든다.
// 텍스처의 idxRtv에 풀에서의 인덱스가 저장된다.
void createRTV( ID3D12Device* device, Texture& tex,
	const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc, DescriptorPool& pool
) {
	__createRTV(device, tex, &rtvDesc, pool);
}

void __createDSV( ID3D12Device* device, Texture& tex,
	const D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc, DescriptorPool& pool
) {
	tex.idxDsv = pool.alloc();
	auto dsvHandle = pool.cpuHandle(tex.idxDsv);
	
	DISPLAY_ERROR_DX_VOID(
		device->CreateDepthStencilView(tex.res.Get(), dsvDesc, dsvHandle), false
	);
}

void createDSV(ID3D12Device* device, Texture& tex, DescriptorPool& pool) {
	__createDSV(device, tex, nullptr, pool);
}

// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 DSV를 만든다.
// 텍스처의 idxDsv에 풀에서의 인덱스가 저장된다.
void createDSV( ID3D12Device* device, Texture& tex,
	const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc, DescriptorPool& pool
) {
	__createDSV(device, tex, &dsvDesc, pool);
}

// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 DSV를 만든다.
// 텍스처의 idxDsv에 풀에서의 인덱스가 저장된다.
void __createSRV( ID3D12Device* device, Texture& tex,
	const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, DescriptorPool& pool
) {
	tex.idxSrv.idxResource = pool.alloc();
	auto srvHandle = pool.cpuHandle(tex.idxSrv.idxResource);

	DISPLAY_ERROR_DX_VOID(
		device->CreateShaderResourceView(tex.res.Get(), srvDesc, srvHandle), false
	);
}

// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 SRV를 만든다.
// 텍스처의 idxSrv.idxResource에 풀에서의 인덱스가 저장된다.
void createSRV(ID3D12Device* device, Texture& tex, DescriptorPool& pool) {
	__createSRV(device, tex, nullptr, pool);
}

// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 SRV를 만든다.
// 텍스처의 idxSrv.idxResource에 풀에서의 인덱스가 저장된다.
void createSRV( ID3D12Device* device, Texture& tex,
	const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc, DescriptorPool& pool
) {
	__createSRV(device, tex, &srvDesc, pool);
}

void __createUAV( ID3D12Device* device, Texture& tex,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc, DescriptorPool& pool
) {
	tex.idxUav.idxResource = pool.alloc();
	auto uavHandle = pool.cpuHandle(tex.idxUav.idxResource);

	DISPLAY_ERROR_DX_VOID(
		device->CreateUnorderedAccessView(tex.res.Get(), nullptr, uavDesc, uavHandle), false
	);
}

// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 UAV를 만든다.
// 텍스처의 idxUav.idxResource에 풀에서의 인덱스가 저장된다.
void createUAV(ID3D12Device* device, Texture& tex, DescriptorPool& pool){ 
	__createUAV(device, tex, nullptr, pool);
}

// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 UAV를 만든다.
// 텍스처의 idxUav.idxResource에 풀에서의 인덱스가 저장된다.
void createUAV( ID3D12Device* device, Texture& tex,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc, DescriptorPool& pool
) {
	__createUAV(device, tex, &uavDesc, pool);
}

// 텍스처의 RTV 용도로 DescriptorPool 객체에서 할당받았던 디스크립터를 반납한다.
// 텍스처 내에 저장되었던 인덱스에 변화가 없어야 하고, 할당한 풀과 반납한 풀이 같아야 한다.
// 이 함수는 그것을 검사하지 않는다.
void freeRTV(const Texture& tex, DescriptorPool& pool) {
	pool.free(tex.idxRtv);
}

// 텍스처의 DSV 용도로 DescriptorPool 객체에서 할당받았던 디스크립터를 반납한다.
// 텍스처 내에 저장되었던 인덱스에 변화가 없어야 하고, 할당한 풀과 반납한 풀이 같아야 한다.
// 이 함수는 그것을 검사하지 않는다.
void freeDSV(const Texture& tex, DescriptorPool& pool) {
	pool.free(tex.idxDsv);
}

// 텍스처의 SRV 용도로 DescriptorPool 객체에서 할당받았던 디스크립터를 반납한다.
// 텍스처 내에 저장되었던 인덱스에 변화가 없어야 하고, 할당한 풀과 반납한 풀이 같아야 한다.
// 이 함수는 그것을 검사하지 않는다.
void freeSRV(const Texture& tex, DescriptorPool& pool) {
	pool.free(tex.idxSrv.idxResource);
}

// 텍스처의 UAV 용도로 DescriptorPool 객체에서 할당받았던 디스크립터를 반납한다.
// 텍스처 내에 저장되었던 인덱스에 변화가 없어야 하고, 할당한 풀과 반납한 풀이 같아야 한다.
// 이 함수는 그것을 검사하지 않는다.
void freeUAV(const Texture& tex, DescriptorPool& pool) {
	pool.free(tex.idxUav.idxResource);
}

SpriteAnimationClip loadSpriteAnimationFromFile( const std::filesystem::path& path, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, 
	std::unordered_map<std::string, std::vector<Texture>>& texAnimHashMap, DescriptorPool& texPool, Fence& fenceToAssociate ) {
	SpriteAnimationClip ret{};

	auto ifs = std::ifstream( path, std::ios::binary );
	DISPLAY_ERROR_STR( ifs.good(), "[File I/O Error]: loadSpriteAnimationFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, false );
	if ( !ifs ) {
		return ret;
	}

	ret.name = readText(ifs, "Name");
	ret.type = static_cast<SpriteAnimType>(readInteger(ifs, "Type"));

	const auto frameCount = readInteger(ifs, "FrameCount");
	ret.frames.resize(frameCount + 1u);

	ret.frameTime = Milliseconds( readInteger(ifs, "FrameTime") );

	auto [pPair, emplaced] = texAnimHashMap.try_emplace(ret.name);
	if (emplaced) {
		auto& sprites = pPair->second;
		sprites.reserve(ret.frames.size());
	}

	ret.duration = Milliseconds( readInteger(ifs, "Duration") );

	readHeadTag( ifs, "Frames" );
	for ( std::size_t i = 0u; i < ret.frames.size() - 1u; ++i ) {
		readHeadTag( ifs, "Frame" );
		
		ret.frames[i].name = readText(ifs, "Name");
		ret.frames[i].width = static_cast<u32t>(readInteger(ifs, "Width"));
		ret.frames[i].height = static_cast<u32t>(readInteger(ifs, "Height"));
		if (ret.type == SpriteAnimType::RandomAdvance) {
			ret.frames[i].time = ret.frameTime;
		}
		else {
			ret.frames[i].time = i * ret.frameTime;
		}

		auto& sprites = pPair->second;

		if (emplaced) {
			const auto texturePath = std::filesystem::path("../resources/Sprites/"s + ret.name + '/' + ret.frames[i].name);

			Texture::Type type{};
			auto texture = loadTexture(device, cmdList, texturePath, fenceToAssociate, type);

			createSRV(device, texture, texPool);
			// TODO: 샘플링 방법도 바이너리에 기록
			texture.idxSrv.idxSampler = etoi(Samplers::TrilinearWrap);

			sprites.push_back(std::move(texture));
		}

		ret.frames[i].sprite = cloneTextureIdxOnly(sprites[i]);

		readTailTag( ifs, "Frame" );
	}

	// sentinel frame
	auto& sentinelFrame = ret.frames.back();
	sentinelFrame.name = "Sentinel";
	sentinelFrame.width = ret.frames[ret.frames.size() - 2u].width;
	sentinelFrame.height = ret.frames[ret.frames.size() - 2u].height;
	sentinelFrame.sprite = ret.frames[ret.frames.size() - 2u].sprite;
	sentinelFrame.time = Milliseconds(std::numeric_limits<float>::max());

	readTailTag( ifs, "Frames" );

	gSharedLog << "[Resource Load] File I/O: Sprites " << '(' << path << ") 로드 완료\n";

	return ret;
}