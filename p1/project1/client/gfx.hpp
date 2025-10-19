#ifndef __GFX_HPP
#define __GFX_HPP

#include "pch.hpp"
#include "errorHandling.hpp"

class GFX {
public:
	// 장치 초기화: setupDXGI, init, createSwapChain 순으로 호출한다.

	// DXGI Factory를 초기화하고, DXGI Adapter들을 열거한다.
	// 그리고 그 중 하나를 선택하여 curAdapter_에 저장한다.
	void setupDXGI(D3D_FEATURE_LEVEL d3dFeatureLevel);
	// D3D12 Device와 Command Queue, Descriptor Heap들을 만든다.
	// 그리고 인자로 전달받은 개수만큼 CommandList와 Command Allocator를 만든다.
	void init(std::size_t cmdListPoolSize);
	// 윈도우와 연결된 SwapChain을 만든다.
	void createSwapChain(HWND hWnd);


private:
	ComPtr<IDXGIFactory4> dxgiFactory_ = nullptr;
	ComPtr<IDXGIAdapter1> curAdapter_ = nullptr;
	std::vector< ComPtr<IDXGIAdapter1> > adapters_{};
	std::vector<DXGI_ADAPTER_DESC1> adapterDescs_{};
	D3D_FEATURE_LEVEL d3dFeatureLevel_{};

	ComPtr<ID3D12Device> device_ = nullptr;
	ComPtr<ID3D12CommandQueue> cmdQ_ = nullptr;
	std::list< ComPtr<ID3D12GraphicsCommandList> > freeCmdLists_{};
	std::list< ComPtr<ID3D12CommandAllocator> > freeCmdAllocators_{};

	DXGI_SWAP_CHAIN_DESC1 scd_{};
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC scfd_{};
	ComPtr<IDXGISwapChain3> swapChain_ = nullptr;
};

#endif	// __GFX_HPP