#ifndef __gfxUtil_HPP
#define __gfxUtil_HPP

#include "pch.hpp"

extern RECT gWndRect;
extern RECT gClientRect;

// createBufferResource 함수에서
// 리소스 힙 타입과 리소스 초기 상태를 설정하는데 참고하는 정보
enum class BufferCreationType {
	VertexBuffer,
	IndexBuffer,
	UploadBuffer
};

// 버퍼 리소스를 생성하는 함수
// creationType이 UploadBuffer이고 pSrc != nullptr인 경우에만
// pSrc의 내용이 리소스에 복사된다.
// creationType에 관계없이 srcByteWidth는 버퍼의 크기를 나타내므로,
// 반드시 srcByteWidth에 유효한 값을 전달하여야 한다.
// creationType에 따라 힙 타입과 리소스 초기 상태를 다르게 설정한다.
ComPtr<ID3D12Resource> createBufferResource(
	ID3D12Device* device, const void* pSrc, UINT64 srcByteWidth,
	BufferCreationType creationType
);

// CommandListUsage는 명령 리스트의 쓰임새를 나타낸다.
// CommandListPool을 만들 때, 명령의 쓰임새에 따라
// 다른 개수의 명령 리스트들을 만들어놓기 위해 쓰인다.
enum class CommandListUsage {
	ResourceLoading,
	RenderingMaster,
	RenderingSlave,
	SIZE
};

// Command List와 그 Command List의 Command Allocator를 페어링한다.
// CommandListPool은 Command List를 CommandContext 단위로 관리한다.
struct CommandContext {
	ComPtr<ID3D12GraphicsCommandList> cmdList;
	ComPtr<ID3D12CommandAllocator> cmdAlloc;
};

// 용도별로 Command List들을 저장하고, 분배하는 클래스
// 용도별로 구분하는 이유는, 할당에 반드시 성공해야 하는 명령 컨텍스트가 있고
// 다른 용도에 비해 우선순위가 밀리는 명령 컨텍스트도 있기 때문이다.
//
// thread-unsafe하니 작업 분배자가 한꺼번에 명령 리스트를 할당하여
// 각 스레드에 분배하도록 해야 한다.
//
// Reset이나 Close, ExecuteCommandLists 함수들을 호출하지 않으므로,
// 명령 컨텍스트를 할당했을 때 가장 먼저 Reset을 하고,
// 명령 기록이 끝났을 때 Close와 ExecuteCommandLists 함수들을 호출한다.
// * 한편, 명령의 기록이 끝났다고 해서 GPU에서의 사용이 끝난 것이 아니므로,
// 반납은 반드시 Fence를 통해 GPU의 작업 종료를 확인한 후 이루어지도록 주의하자.
class CommandListPool {
public:
	// 특정 용도 usage의 명령 컨텍스트 cnt개를 풀 내부에 확충해놓는다.
	void init(ID3D12Device* device, CommandListUsage usage, std::size_t cnt);
	// 특정 용도 usage의 명령 컨텍스트를 cnt개 할당해 output에 채워넣는다.
	// @return 실제로 할당한 명령 컨텍스트 수, 요청 받은 개수 cnt와 다를 수 있다.
	std::size_t alloc(std::size_t cnt, CommandListUsage usage, std::list<CommandContext>& output);
	// 특정 용도 usage의 명령 컨텍스트 1개를 output에 채워넣는다.
	// @return 명령 컨텍스트의 할당 성공 여부, 실패할 수도 있다.
	bool allocOne(CommandListUsage usage, CommandContext& output);
	// 특정 용도 usage의 명령 컨텍스트들 expired를 풀에 반납한다.
	// 반드시 GPU에서도 사용이 끝난 명령 컨텍스트들임을 확인하자.
	void free(CommandListUsage usage, std::list<CommandContext>&& expired);
	// 특정 용도 usage의 명령 컨텍스트들 expired를 풀에 반납한다.
	// 반드시 GPU에서도 사용이 끝난 명령 컨텍스트들임을 확인하자.
	// * 반복문에서의 쓰임을 상정해 enum 대신 int를 받는 버전을 마련하였다.
	//   범위 점검을 따로 하지 않으므로 주의해서 쓰자.
	void free(int usage, std::list<CommandContext>&& expired);
	// 특정 용도 usage의 명령 컨텍스트 expired를 풀에 반납한다.
	// 반드시 GPU에서도 사용이 끝난 명령 컨텍스트임을 확인하자.
	void freeOne(CommandListUsage usage, CommandContext&& expired);

private:
	std::array< std::list<CommandContext>, etoi(CommandListUsage::SIZE) > cmdCtxs_{};
};

// ID3D12Fence 객체와 연관된 변수들을 모아놓기 위한 구조체
struct Fence {
	// Command List와 Command Allocator의 반납은 gpu에서의 사용이 끝난 후 이루어져야 한다.
	// 즉, Fence의 Wait이 끝났을 때 반납되어야 한다.
	// 따라서 Fence와 함께 그 Fence의 Wait이 끝날 때 반납할
	// 명령 컨텍스트들을 보관하면 용이하다.
	// CommandListUsage별 CommandContext의 리스트로 저장한다.
	std::array< std::list<CommandContext>, etoi(CommandListUsage::SIZE) > associatedCmdCtxs_;
	// Copy 등에 사용되는 일회용 리소스들도 Fence의 Wait이 끝난 후 폐기한다.
	std::vector<ComPtr<ID3D12Resource>> associatedResources_;
	ComPtr<ID3D12Fence> fence;
	UINT64 desiredValue;
	HANDLE event;
};

// ComPtr<ID3D12DescriptorHeap> 객체와 부가 정보들을 한꺼번에 저장하기 위한 구조체
struct DescriptorHeap {
	DescriptorHeap() = default;
	DescriptorHeap(ID3D12Device* device, const D3D12_DESCRIPTOR_HEAP_DESC& desc);

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	ComPtr<ID3D12DescriptorHeap> heap = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuStart{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpuStart{};
	bool gpuVisible = false;
};

// 뷰들을 할당하기 위한 클래스
// free list를 통해 할당 가능한 인덱스들을 관리한다.
// 인덱스로부터 D3D12_CPU_DESCRIPTOR_HANDLE이나 D3D12_GPU_DESCRIPTOR_HANDLE을
// 계산해서 얻어낼 수 있는 유틸리티를 제공한다.
// 이때 뷰의 인덱스는, 그것을 할당한 DescriptorPool 객체 내에서 해당 뷰의 순번이다. (0-based)
class DescriptorPool {
public:
	DescriptorPool() = default;
	// @param viewCnt 풀에서 관리할 인덱스(뷰)의 수
	// @param cpuStart 해당 풀의 Descriptor Heap에서의 시작 cpu 영역
	// @param gpuStart 해당 풀의 Descriptor Heap에서의 시작 gpu 영역
	// @param type 해당 풀이 관리하는 뷰 타입
	// @param gpuVisible 셰이더에서 사용 가능하도록 할 건지 여부
	// @param incrementSize D3D12 Device로부터 얻어지는, 뷰의 메모리 크기
	// @brief DescriptorPool 객체끼리 관리하는 힙 영역이 겹치지 않도록 주의한다.
	//		cpuStart와 gpuStart, gpuVisible은 Descriptor Heap으로부터,
	//		incrementSize는 device로부터 얻어오도록 한다.
	DescriptorPool(std::size_t viewCnt, D3D12_CPU_DESCRIPTOR_HANDLE cpuStart,
		D3D12_GPU_DESCRIPTOR_HANDLE gpuStart, D3D12_DESCRIPTOR_HEAP_TYPE type,
		bool gpuVisible, UINT incrementSize
	);

	// 인덱스를 할당한다.
	int alloc();
	// 인덱스를 반납한다.
	void free(int idx);
	// 인덱스로부터 D3D12_CPU_DESCRIPTOR_HANDLE을 계산해낸다.
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle(int idx) const;
	// 인덱스로부터 D3D12_GPU_DESCRIPTOR_HANDLE을 계산해낸다.
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle(int idx) const;

	// 루트 시그너처의 idxRootParam 번째 루트 파라미터에
	// SetGraphicsRootDescriptorTable 함수를 통해 풀을 바인드한다.
	void bind(ID3D12GraphicsCommandList* cmdList, UINT idxRootParam) const;

private:
	std::list<int> freeIndices_{};
	D3D12_CPU_DESCRIPTOR_HANDLE cpuStart_{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpuStart_{};
	D3D12_DESCRIPTOR_HEAP_TYPE type_{};
	UINT incrementSize_{};
	bool gpuVisible_ = false;
};

// 깊이 버퍼를 쉽게 생성하는 유틸리티 함수
ComPtr<ID3D12Resource> createDepthBuffer(ID3D12Device* device,
	DXGI_FORMAT format, const DXGI_SAMPLE_DESC& sampleDesc
);

// ID3D12GraphicsCommandList::ResourceBarrier 인터페이스를 통해
// 리소스의 상태를 beforeState에서 afterState로 전환한다.
// 사용되는 기본값들은 본문을 참조하자.
void transitionResourceState(ID3D12GraphicsCommandList* cmdList,
	ID3D12Resource* resource, D3D12_RESOURCE_STATES beforeState,
	D3D12_RESOURCE_STATES afterState
);

// 리소스 상태 전환과 관련된 사항을 간략화하는 리소스 복사 함수
void copyResource( ID3D12GraphicsCommandList* cmdList,
	ID3D12Resource* srcRes, ID3D12Resource* destRes,
	D3D12_RESOURCE_STATES srcResState, D3D12_RESOURCE_STATES destResState
);

// ConstantBuffer, StructuredBuffer 등
// 셰이더의 입력으로 쓰이는 버퍼들을 대표한다.
// 여러 개의 CommandList에서 쓰이고,
// 각각의 CommandList에서 개별적인 데이터를 사용하고 싶을 때
// roomCnt를 그러한 CommandList의 개수로 설정한다.
// 각각의 room이 반드시 의도된 CommandList에서만 사용되도록 주의하자.
class ShaderInputBuffer {
public:
	ShaderInputBuffer() = default;
	// 이미 만들어진 리소스를 가져와서, 메모리 영역을 분배받아 사용하는 경우
	// 이 생성자를 호출한다.
	// 리소스의 [addressOffset, addressOffset + allowedByteWidth) 영역을 분배받는다.
	// CreateCommittedResource 호출을 줄이고 gpu 메모리 사용량을 줄이는 이점이 있다.
	ShaderInputBuffer( const std::vector<ComPtr<ID3D12Resource>>& premadeResource,
		std::size_t addressOffset, std::size_t allowedByteWidth, const std::wstring& name
	);
	// 기본 생성자를 호출하고 init을 호출하는 것과 같다.
	ShaderInputBuffer(ID3D12Device* device, UINT64 byteWidth, std::size_t roomCnt, const std::wstring& name);

	// byteWidth 크기의 리소스를 roomCnt 개 만큼 만든다.
	void init(ID3D12Device* device, UINT64 byteWidth, std::size_t roomCnt, const std::wstring& name);
	// roomIdx의 리소스를 루트 시그너처에 바인드한다.
	virtual void bind( ID3D12GraphicsCommandList* cmdList,
		UINT rootParamIdx, std::size_t roomIdx
	) = 0;

	// roomIdx 리소스의 gpu 데이터를 range와 동기화한다.
	template <std::ranges::sized_range R>
	void stage(std::size_t roomIdx, const R& range) {
		stage(roomIdx, std::data(range), std::size(range));
	}
	// roomIdx 리소스의 gpu 데이터를 elems와 동기화한다.
	template <class T>
	void stage(std::size_t roomIdx, const T* elems, std::size_t elemCnt) {
		stage(roomIdx, static_cast<const void*>(elems), elemCnt * sizeof(T));
	}
	// roomIdx 리소스의 gpu 데이터를 data와 동기화한다.
	void stage(std::size_t roomIdx, const void* data, std::size_t byteWidth);

protected:
	// 자식 클래스들의 bind 함수에서 접근해야 하므로 protected로 둔다.
	// (SetGraphicsRootConstantBufferView 등의 함수는 D3D12_GPU_VIRTUAL_ADDRESS를 인자로 받는다.)
	std::vector<D3D12_GPU_VIRTUAL_ADDRESS> addresses_{};

private:
	std::vector<ComPtr<ID3D12Resource>> resources_{};
	std::vector<void*> mappedRegions_{};
	std::wstring name_{};
	UINT64 byteWidth_ = 0u;	// stage 함수에서 할당(분배)된 영역 바깥을 참조하는지 검사할 때 사용
};

// 셰이더의 ConstantBuffer와 연동되는 클래스,
// 자세한 내용은 ShaderInputBuffer 클래스를 참조
// bind 호출 시 루트 시그너처에 cbv로 바인딩된다.
class ConstantBuffer : public ShaderInputBuffer {
public:
	using ShaderInputBuffer::ShaderInputBuffer;

	// roomIdx의 리소스를 루트 시그너처에 cbv로 연결한다.
	void bind( ID3D12GraphicsCommandList* cmdList,
		UINT rootParamIdx, std::size_t roomIdx
	) override;
};

// 셰이더의 StructuredBuffer와 연동되는 클래스,
// 자세한 내용은 ShaderInputBuffer 클래스를 참조
// bind 호출 시 루트 시그너처에 srv로 바인딩된다.
class StructuredBuffer : public ShaderInputBuffer {
public:
	using ShaderInputBuffer::ShaderInputBuffer;

	// roomIdx의 리소스를 루트 시그너처에 srv로 연결한다.
	void bind( ID3D12GraphicsCommandList* cmdList,
		UINT rootParamIdx, std::size_t roomIdx
	) override;
};

// 크기가 작지 않은 ConstantBuffer 여러 개를 사용해야 할 경우,
// 성능 최적화를 위해 ConstantBufferArray를 사용한다.
// room 개수만큼의 리소스만 실제로 생성하고,
// 내부의 ConstantBuffer 객체들은 그 리소스의 메모리 영역을 분할해 사용한다.
// * createConstantBufferrArray 함수를 사용해서 생성한 뒤에,
//	딱히 주의할 점은 없고, cbuffers 멤버에 접근 해 일반 ConstantBuffer 사용하듯이 사용하면 된다.
struct ConstantBufferArray {
	std::vector<ComPtr<ID3D12Resource>> sharedResources;
	std::vector<ConstantBuffer> cbuffers;
};

// ConstantBufferArray 객체를 생성한다.
// room 개수만큼의 큰 리소스들을 생성하고
// 그 리소스들을 기반으로 ConstantBuffer들을 생성해 담는다.
ConstantBufferArray createConstantBufferArray( ID3D12Device* device, UINT64 elemByteWidth,
	std::size_t elemCnt, std::size_t roomCnt, const std::wstring& name
);

struct LoadDDSReturnType {
    ComPtr<ID3D12Resource> res;
    std::unique_ptr<std::uint8_t[]> ddsData;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
};

// dds 포맷의 이미지 파일을 로드한다.
// UpdateSubresources 함수에서 쓰인 임시 업로드 버퍼가 펜스에 연관되므로,
// 펜스에서 gpu 작업 완료를 확인하고 난 뒤에 임시 업로드 버퍼들을 해제할 필요가 있다.
LoadDDSReturnType loadDDS(
    ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
    const std::filesystem::path& path, Fence& fenceToAssociate
);

// Bindless 환경에서 특정한 텍스처를 샘플링하기 위한 종합적 인덱스
struct BindlessIndex {
	i32t idxRange;	// 텍스처의 srv가 존재하는 풀 인덱스(Texture, TextureArray, TextureCube)
	i32t idxResource;	// 풀 내에서 srv의 인덱스
	i32t idxInArray;	// Texture Array에서 특정한 텍스처를 샘플링할 때 사용하는, 배열 인덱스
	i32t idxSampler;	// 사용할 샘플러의 인덱스
};

// 공용 샘플러 종류 목록,
// etoi 함수를 이용해 BindlessIndex 등에서 사용할 샘플러 인덱스를 얻어내자.
enum class Samplers {
	NearestWrap,
	TrilinearWrap,
	NearestBorder,
	TrilinerBorder,
	NearestClamp,
	TrilinearClamp,
	NearestComparison,
	BilinearComparison
};

// 텍스처와 관련된 정보를 담는 구조체
// gpu 리소스를 담는 ComPtr 객체와 Bindless 셰이더에서 인덱싱하기 위한 인덱스들이 저장된다.
struct Texture {
	ComPtr<ID3D12Resource> res;
	int idxRtv;
	int idxDsv;
	BindlessIndex idxSrv;
	BindlessIndex idxUav;
};

// dds 포맷의 파일로부터 텍스처를 로드한다.
// UpdateSubresources 함수에서 쓰인 임시 업로드 버퍼가 펜스에 연관되므로,
// 펜스에서 gpu 작업 완료를 확인하고 난 뒤에 임시 업로드 버퍼들을 해제할 필요가 있다.
Texture loadTexture( ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	const std::filesystem::path& path, Fence& fenceToAssociate
);

// 텍스처의 gpu 리소스를 담는 ComPtr 부분은 제외하고,
// Bindless 셰이더에서 인덱싱하기 위한 인덱스들만 복사한 텍스처를 반환한다.
// 리소스 자체에 접근할 일은 거의 없기 때문에, 인덱스들만 복사하는 것이
// 대부분의 상황에서 용이하다.
Texture cloneTextureIdxOnly(const Texture& tex);

// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 RTV를 만든다.
// 텍스처의 idxRtv에 풀에서의 인덱스가 저장된다.
void createRTV(ID3D12Device* device, Texture& tex, DescriptorPool& pool);
// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 RTV를 만든다.
// 텍스처의 idxRtv에 풀에서의 인덱스가 저장된다.
void createRTV( ID3D12Device* device, Texture& tex,
	const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc, DescriptorPool& pool
);
// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 DSV를 만든다.
// 텍스처의 idxDsv에 풀에서의 인덱스가 저장된다.
void createDSV(ID3D12Device* device, Texture& tex, DescriptorPool& pool);
// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 DSV를 만든다.
// 텍스처의 idxDsv에 풀에서의 인덱스가 저장된다.
void createDSV( ID3D12Device* device, Texture& tex,
	const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc, DescriptorPool& pool
);
// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 SRV를 만든다.
// 텍스처의 idxSrv.idxResource에 풀에서의 인덱스가 저장된다.
void createSRV(ID3D12Device* device, Texture& tex, DescriptorPool& pool);
// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 SRV를 만든다.
// 텍스처의 idxSrv.idxResource에 풀에서의 인덱스가 저장된다.
void createSRV( ID3D12Device* device, Texture& tex,
	const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc, DescriptorPool& pool
);
// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 UAV를 만든다.
// 텍스처의 idxUav.idxResource에 풀에서의 인덱스가 저장된다.
void createUAV(ID3D12Device* device, Texture& tex, DescriptorPool& pool);
// DescriptorPool 객체에서 디스크립터를 할당받아 그 자리에 텍스처의 UAV를 만든다.
// 텍스처의 idxUav.idxResource에 풀에서의 인덱스가 저장된다.
void createUAV( ID3D12Device* device, Texture& tex,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc, DescriptorPool& pool
);
// 텍스처의 RTV 용도로 DescriptorPool 객체에서 할당받았던 디스크립터를 반납한다.
// 텍스처 내에 저장되었던 인덱스에 변화가 없어야 하고, 할당한 풀과 반납한 풀이 같아야 한다.
// 이 함수는 그것을 검사하지 않는다.
void freeRTV(const Texture& tex, DescriptorPool& pool);
// 텍스처의 DSV 용도로 DescriptorPool 객체에서 할당받았던 디스크립터를 반납한다.
// 텍스처 내에 저장되었던 인덱스에 변화가 없어야 하고, 할당한 풀과 반납한 풀이 같아야 한다.
// 이 함수는 그것을 검사하지 않는다.
void freeDSV(const Texture& tex, DescriptorPool& pool);
// 텍스처의 SRV 용도로 DescriptorPool 객체에서 할당받았던 디스크립터를 반납한다.
// 텍스처 내에 저장되었던 인덱스에 변화가 없어야 하고, 할당한 풀과 반납한 풀이 같아야 한다.
// 이 함수는 그것을 검사하지 않는다.
void freeSRV(const Texture& tex, DescriptorPool& pool);
// 텍스처의 UAV 용도로 DescriptorPool 객체에서 할당받았던 디스크립터를 반납한다.
// 텍스처 내에 저장되었던 인덱스에 변화가 없어야 하고, 할당한 풀과 반납한 풀이 같아야 한다.
// 이 함수는 그것을 검사하지 않는다.
void freeUAV(const Texture& tex, DescriptorPool& pool);

#endif	// __gfxUtil_HPP