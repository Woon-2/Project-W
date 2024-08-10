#include "d3d12core.hpp"

namespace gfx {

namespace d3d12 {

void Core::init() {
    auto adapter = enumAdapters();
    createDevice(adapter.Get());
    createCommandQueueAndList(pDevice_.Get());
    buildRtvAndDsvHeaps(pDevice_.Get());
    createFenceAndEvent(pDevice_.Get());
}

wrl::ComPtr<IDXGIAdapter1> Core::enumAdapters() {
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
        D3D12GetDebugInterface(__uuidof(ID3D12Debug), &pDebug)
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

void Core::createFenceAndEvent(ID3D12Device* pDevice) {
    DX_THROW_FAILED( pDevice->CreateFence(
        0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &pFence_
    ) );

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) {
        DX_THROW_FAILED( HRESULT_FROM_WIN32(GetLastError()) );
    }
}

void Core::render(const IScene& scene, const IRenderer& renderer, IRenderTarget& target) {
    if (auto rootSig = root(&renderer)) {
        pCmdList_->SetGraphicsRootSignature(rootSig.Get());
    }

    auto context = D3D12RenderContext(*this);
    renderer.render(scene, context, target);
}

void Core::preRender() {
    DX_THROW_FAILED( pCmdAlloc_->Reset() );
    DX_THROW_FAILED( pCmdList_->Reset(pCmdAlloc_.Get(), nullptr) );
}

void Core::postRender() {
    DX_THROW_FAILED( pCmdList_->Close() );

    ID3D12CommandList* ppCmdLists[] = { pCmdList_.Get() };
    DX_THROW_FAILED_VOID( pCmdQ_->ExecuteCommandLists(1, ppCmdLists) );
}

void Core::waitForGpu() {
    ++fenceValues_[fenceIdx_];
    DX_THROW_FAILED( pCmdQ_->Signal(pFence_.Get(), fenceValues_[fenceIdx_]) );

    if (pFence_->GetCompletedValue() < fenceValues_[fenceIdx_]) {
        DX_THROW_FAILED( pFence_->SetEventOnCompletion(fenceValues_[fenceIdx_], fenceEvent_) );
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void Core::alterFence() {
    fenceIdx_ = (fenceIdx_ + 1) % fenceValues_.size();
}

void Core::alterFence(std::size_t idx) {
    if (idx >= fenceValues_.size()) {
        throw std::out_of_range("The index is out of range.");
    }

    fenceIdx_ = idx;
}

std::unique_ptr<IRenderContext> Core::createContext() {
    return std::make_unique<D3D12RenderContext>(*this);
}

void Core::cleanup() {
    // the order of reset should be reversed from the order of creation. (the member layout order)
    pFence_.Reset();
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
    return pCore_->cmdList();
}

wrl::ComPtr<IDXGIFactory4> Core::spFactory = nullptr;
std::size_t Core::sRtvHeapSize = 0;
std::size_t Core::sDsvHeapSize = 0;

}   // namespace d3d12

}   // namespace gfx