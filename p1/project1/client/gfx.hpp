#ifndef __GFX_HPP
#define __GFX_HPP

#include "pch.hpp"
#include "shader.hpp"

// Fence 정책 결정후 구체적인 주석 필요
// 큐브 메시 그리기 (버퍼, 텍스처 리소스 소유자 및 중첩 갯수 등 결정 필요)
// 유니티로 메시 추출 스크립트 작성 (유니티에서 생성한 파일을 읽기위한 API 구현)
// 셰이딩 (조명 구현, 셰이더 구현)
// WASD 이동 (GetAsyncKeyState로 임시 구현, 입력과 반응 분리해 이후 네트워크 대응)
// 멀티스레드 렌더링 (커맨드 리스트 풀, 스레드 연관)
// 텍스처링
// 모델 로드
// 1인칭 카메라 구현

extern RECT gWndRect;
extern RECT gClientRect;

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
	DescriptorPool( std::size_t viewCnt, D3D12_CPU_DESCRIPTOR_HANDLE cpuStart,
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
ComPtr<ID3D12Resource> createDepthBuffer( ID3D12Device* device, 
	DXGI_FORMAT format, const DXGI_SAMPLE_DESC& sampleDesc
);

// ID3D12GraphicsCommandList::ResourceBarrier 인터페이스를 통해
// 리소스의 상태를 beforeState에서 afterState로 전환한다.
// 사용되는 기본값들은 본문을 참조하자.
void transitionResourceState( ID3D12GraphicsCommandList* cmdList,
	ID3D12Resource* resource, D3D12_RESOURCE_STATES beforeState,
	D3D12_RESOURCE_STATES afterState
);

// ID3D12Fence 객체와 연관된 변수들을 모아놓기 위한 구조체
struct Fence {
	// Command List와 Command Allocator의 반납은 gpu에서의 사용이 끝난 후 이루어져야 한다.
	// 즉, Fence의 Wait이 끝났을 때 반납되어야 한다.
	// 따라서 Fence와 함께 그 Fence의 Wait이 끝날 때 반납할 Command List와 Command Allocator들을
	// 같이 보관하면 용이하다.
	std::list<ComPtr<ID3D12GraphicsCommandList>> associatedCmdLists_;
	std::list<ComPtr<ID3D12CommandAllocator>> associatedCmdAllocators_;
	ComPtr<ID3D12Fence> fence;
	UINT64 desiredValue;
	HANDLE event;
};

class GFX {
public:
	// 장치 초기화: setupDXGI, init, createSwapChain 순으로 호출한다.

	// DXGI Factory를 초기화하고, DXGI Adapter들을 열거한다.
	// 그리고 그 중 하나를 선택하여 curAdapter_에 저장한다.
	void setupDXGI(D3D_FEATURE_LEVEL d3dFeatureLevel);
	// D3D12 Device와 Command Queue, Descriptor Heap들을 만든다.
	// 인자로 전달받은 개수만큼 CommandList와 Command Allocator를 만든다.
	// 그리고 Root Signature와 Shader(PSO)들을 만든다.
	void init(std::size_t cmdListPoolSize);
	// 윈도우와 연결된 SwapChain을 만든다.
	// 그리고 Back Buffer 개수 만큼의 Fence들을 만든다.
	void createSwapChain(HWND hWnd);

	void render();


private:
	void renderSampleShader(ID3D12GraphicsCommandList* cmdList);

	void signalFence(const std::wstring& fenceName);
	void waitOnFence(const std::wstring& fenceName);

	ComPtr<IDXGIFactory4> dxgiFactory_ = nullptr;
	ComPtr<IDXGIAdapter1> curAdapter_ = nullptr;
	std::vector< ComPtr<IDXGIAdapter1> > adapters_{};
	std::vector<DXGI_ADAPTER_DESC1> adapterDescs_{};
	D3D_FEATURE_LEVEL d3dFeatureLevel_{};

	ComPtr<ID3D12Device> device_ = nullptr;
	ComPtr<ID3D12CommandQueue> cmdQ_ = nullptr;

	// Command List와 Command Allocator는 서로 짝을 지어 할당/해제하도록 한다.
	// 꺼낼 땐 앞에서 꺼내고 넣을 땐 뒤에 넣기 (큐처럼 쓰자)
	std::list< ComPtr<ID3D12GraphicsCommandList> > freeCmdLists_{};
	std::list< ComPtr<ID3D12CommandAllocator> > freeCmdAllocators_{};

	DXGI_SWAP_CHAIN_DESC1 scd_{};
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC scfd_{};
	ComPtr<IDXGISwapChain3> swapChain_ = nullptr;
	std::vector<ComPtr<ID3D12Resource>> backBuffers_{};
	std::vector<ComPtr<ID3D12Resource>> depthBuffers_{};
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> backBufferRtvs_{};
	std::vector<int> allocatedRtvIndices_{};
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> depthBufferDsvs_{};
	std::vector<int> allocatedDsvIndices_{};

	DescriptorHeap rtvHeap_{};
	DescriptorPool rtvPool_{};
	DescriptorHeap dsvHeap_{};
	DescriptorPool dsvPool_{};

	std::map<std::wstring, std::unique_ptr<RootSig>> rootSigs_{};
	std::map<std::wstring, ComPtr<ID3D12PipelineState>> shaders_{};

	std::map<std::wstring, Fence> fences_{};

	std::size_t frameIdx = 0u;
};

#endif	// __GFX_HPP