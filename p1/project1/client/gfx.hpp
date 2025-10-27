#ifndef __GFX_HPP
#define __GFX_HPP

#include "pch.hpp"
#include "gfxUtil.hpp"
#include "shader.hpp"
#include "mesh.hpp"

// 큐브 메시 그리기 (버퍼, 텍스처 리소스 소유자 및 중첩 갯수 등 결정 필요)
// 유니티로 메시 추출 스크립트 작성 (유니티에서 생성한 파일을 읽기위한 API 구현)
// 셰이딩 (조명 구현, 셰이더 구현)
// WASD 이동 (GetAsyncKeyState로 임시 구현, 입력과 반응 분리해 이후 네트워크 대응)
// 멀티스레드 렌더링 (커맨드 리스트 풀, 스레드 연관)
// 텍스처링
// 모델 로드
// 1인칭 카메라 구현


// Batching & Instancing 구현으로 큐브 여러 개 그려보기
// 1000개의 큐브 렌더링
// 1000개의 큐브 멀티스레드 렌더링
// 큐브에 면별 재질 입혀보기
// Constant Buffer와 메시 관리 주체 정하기
// 카메라 구현, WASD 이동
// 멀티스레드 로드 & 렌더링

extern HWND ghWnd;


// ID3D12Fence 객체와 연관된 변수들을 모아놓기 위한 구조체
struct Fence {
	// Command List와 Command Allocator의 반납은 gpu에서의 사용이 끝난 후 이루어져야 한다.
	// 즉, Fence의 Wait이 끝났을 때 반납되어야 한다.
	// 따라서 Fence와 함께 그 Fence의 Wait이 끝날 때 반납할 Command List와 Command Allocator들을
	// 같이 보관하면 용이하다.
	// Copy 등에 사용되는 일회용 리소스들도 Fence의 Wait이 끝난 후 폐기한다.
	std::list<ComPtr<ID3D12GraphicsCommandList>> associatedCmdLists_;
	std::list<ComPtr<ID3D12CommandAllocator>> associatedCmdAllocators_;
	std::vector<ComPtr<ID3D12Resource>> associatedResources_;
	ComPtr<ID3D12Fence> fence;
	UINT64 desiredValue;
	HANDLE event;
};

class GFX {
public:
	GFX() = default;
	GFX(const GFX&) = delete;
	GFX& operator=(const GFX&) = delete;
	GFX(GFX&&) noexcept = delete;
	GFX& operator=(GFX&&) noexcept = delete;
	// GFX가 소멸할 때, 제출된 모든 GPU작업이 완료되고 나서 소멸하도록 한다.
	~GFX();

	// 장치 초기화: setupDXGI, init, createSwapChain 순으로 호출한다.

	// DXGI Factory를 초기화하고, DXGI Adapter들을 열거한다.
	// 그리고 그 중 하나를 선택하여 curAdapter_에 저장한다.
	void setupDXGI(D3D_FEATURE_LEVEL d3dFeatureLevel);
	// D3D12 Device와 Command Queue, Descriptor Heap들을 만든다.
	// 인자로 전달받은 개수만큼 CommandList와 Command Allocator를 만든다.
	// Root Signature와 Shader(PSO)들을 만든다.
	// Load Fence를 만든다.
	// 그리고 DrawEvent들을 저장하기 위한 메모리를 예약한다.
	void init(std::size_t cmdListPoolSize);
	// 윈도우와 연결된 SwapChain을 만든다.
	// Back Buffer 개수 만큼의 room을 가지는 Constant Buffer들을 만든다.
	// 그리고 Back Buffer 개수 만큼의 Frame Fence들을 만든다.
	void createSwapChain();

	void addDrawEvent(const SamplePipeline::DrawEvent& drawEvent);

	void loadMeshes();
	const Mesh* cubeMesh() const { return &meshCube_; }

	void render();


private:
	void renderSampleShader(ID3D12GraphicsCommandList* cmdList);

	// fenceName을 갖는 Fence의 desiredValue 값을 1 증가시키고
	// GPU 큐에 그 갱신 명령을 삽입한다.
	void signalFence(const std::wstring& fenceName);
	// fenceName을 갖는 Fence에 대해서 wait하고,
	// 사용이 끝난 명령 리스트, 명령 할당자, 업로드 버퍼 등을 반환한다.
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

	std::vector<SamplePipeline::DrawEvent> drawEventsSamplePipeline_{};
	SamplePipeline::Resources resourcesSamplePipeline_{};

	std::size_t frameIdx = 0u;

	Mesh meshCube_{};
};

#endif	// __GFX_HPP