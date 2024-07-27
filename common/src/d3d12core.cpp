#include "d3d12core.hpp"

namespace gfx {

namespace d3d12 {

void Core::init() {
    auto adapter = enumAdapters();
    createDevice(adapter.Get());
    createCommandQueueAndList(pDevice_.Get());
    buildRtvAndDsvHeaps(pDevice_.Get());
}

wrl::ComPtr<IDXGIAdapter1> enumAdapters() {
    auto pAdapter = wrl::ComPtr<IDXGIAdapter1>();
    auto pFactory = DXFactory::get();

    for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        pAdapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // When the nullptr is passed as the last parameter,
        // the function only checks whether the device can be created.
        if (D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr) >= 0) {
            break;
        }
    }

    if (!pAdapter) {
        DX_THROW_FAILED( pFactory->EnumWarpAdapter(
            __uuidof(IDXGIAdapter1), &pAdapter
        ) );

        DX_THROW_FAILED( D3D12CreateDevice(
            pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr
        ) );
    }

    return pAdapter;
}

void Core::createDevice(IDXGIAdapter1* pAdapter) {
#ifdef ENABLE_DXGI_INFO
    auto pDebug = wrl::ComPtr<ID3D12Debug>();
    DX_THROW_FAILED(
        DXGIGetDebugInterface(__uuidof(ID3D12Debug), &pDebug)
    );

    pDebug->EnableDebugLayer();
#endif  // ENABLE_DXGI_INFO

    DX_THROW_FAILED( D3D12CreateDevice(
        pAdapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), &pDevice_
    ) );
}

void Core::createCommandQueueAndList(ID3D12Device* pDevice) {
    auto qd = D3D12_COMMAND_QUEUE_DESC{
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0
    };

    DX_THROW_FAILED( pDevice->CreateCommandQueue(
        &qd, __uuidof(ID3D12CommandQueue), &pCmdQ_
    ) );

    DX_THROW_FAILED( pDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &pCmdAlloc_
    ) );

    DX_THROW_FAILED( pDevice->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCmdAlloc_.Get(), nullptr, __uuidof(ID3D12GraphicsCommandList), &pCmdList_
    ) );

    DX_THROW_FAILED( pCmdList_->Close() );
}

void Core::buildRtvAndDsvHeaps(ID3D12Device* pDevice) {
    auto rhd = D3D12_DESCRIPTOR_HEAP_DESC{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = static_cast<UINT>(sRtvHeapSize),
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0
    };

    DX_THROW_FAILED( pDevice->CreateDescriptorHeap(
        &rhd, __uuidof(ID3D12DescriptorHeap), &pRtvHeap_
    ) );
    
    auto dhd = D3D12_DESCRIPTOR_HEAP_DESC{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        .NumDescriptors = static_cast<UINT>(sDsvHeapSize),
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0
    };

    DX_THROW_FAILED( pDevice->CreateDescriptorHeap(
        &dhd, __uuidof(ID3D12DescriptorHeap), &pDsvHeap_
    ) );
}

void Core::render(const IScene& scene, const IRenderer& renderer, IRenderTarget& target) {
    auto context = D3D12RenderContext(*this);
    renderer.render(scene, context, target);
}

std::unique_ptr<IRenderContext> Core::createContext() {
    return std::make_unique<D3D12RenderContext>(*this);
}

void Core::cleanup() {
    // the order of reset should be reversed from the order of creation. (the member layout order)
    pDsvHeap_.Reset();
    pRtvHeap_.Reset();
    pCmdList_.Reset();
    pCmdAlloc_.Reset();
    pCmdQ_.Reset();
    pDevice_.Reset();
}

bool D3D12RenderContext::castableTo(RenderContextType contextType) const {
    return contextType == RenderContextType::D3D12;
}

std::any D3D12RenderContext::cast(RenderContextType contextType) {
    return pCmdList_;
}

wrl::ComPtr<IDXGIFactory4> Core::spFactory = nullptr;
std::size_t Core::sRtvHeapSize = 0;
std::size_t Core::sDsvHeapSize = 0;

}   // namespace d3d12

}   // namespace gfx