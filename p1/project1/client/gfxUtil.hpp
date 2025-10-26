#ifndef __gfxUtil_HPP
#define __gfxUtil_HPP

#include "pch.hpp"

extern RECT gWndRect;
extern RECT gClientRect;

enum class BufferCreationType {
	VertexBuffer,
	IndexBuffer,
	UploadBuffer
};

// 버퍼 리소스를 생성하는 함수
// creationType이 UploadBuffer인 경우에만 pSrc의 내용을 반영한다.
// creationType에 관계없이 srcByteWidth는 버퍼의 크기를 나타내므로,
// 반드시 srcByteWidth에 유효한 값을 전달하여야 한다.
ComPtr<ID3D12Resource> createBufferResource(
	ID3D12Device* device, const void* pSrc, UINT64 srcByteWidth,
	BufferCreationType creationType
);

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

#endif	// __gfxUtil_HPP