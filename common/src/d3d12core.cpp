#include "d3d12core.hpp"

namespace gfx {

namespace d3d12 {

CmdListPool::CmdListPool(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type, std::size_t size)
    : cmdLists_(), cmdAllocs_() {
    for (std::size_t i = 0; i < size; ++i) {
        wrl::ComPtr<ID3D12CommandAllocator> pCmdAlloc;
        DX_THROW_FAILED( pDevice->CreateCommandAllocator(
            type, __uuidof(ID3D12CommandAllocator), &pCmdAlloc
        ) );
        cmdAllocs_.push_back(std::move(pCmdAlloc));
    }

    for (auto& cmdAlloc : cmdAllocs_) {
        wrl::ComPtr<ID3D12GraphicsCommandList> pCmdList;
        DX_THROW_FAILED( pDevice->CreateCommandList(
            0, type, cmdAlloc.Get(), nullptr, __uuidof(ID3D12GraphicsCommandList), &pCmdList
        ) );
        pCmdList->Close();
        cmdLists_.push_back(std::move(pCmdList));
    }
}

const CmdListPool::Element CmdListPool::fetch() {
    auto elem = Element{
        .pCmdList = std::move(cmdLists_.front()),
        .pCmdAlloc = std::move(cmdAllocs_.front())
    };

    cmdLists_.pop_front();
    cmdAllocs_.pop_front();

    return elem;
}

void Core::init() {
    auto adapter = enumAdapters();
    createDevice(adapter.Get());
    createCommandQueueAndLists(pDevice_.Get());
    buildDescHeaps(pDevice_.Get());
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

void Core::createCommandQueueAndLists(ID3D12Device* pDevice) {
    auto qd = D3D12_COMMAND_QUEUE_DESC{
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0
    };

    DX_THROW_FAILED( pDevice->CreateCommandQueue(
        &qd, __uuidof(ID3D12CommandQueue), &pCmdQ_
    ) );

    // Currently the maximum command list size is same as the RTV heap size,
    // as the frame pipelining requires command lists for each frame.
    // If the requirement changes, maybe due to multithreading or etc., the command list size should be adjusted.
    gfxCmdListPool_ = CmdListPool(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, rtvHeapSize());
}

void Core::buildDescHeaps(ID3D12Device* pDevice) {
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

    auto chd = D3D12_DESCRIPTOR_HEAP_DESC{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = static_cast<UINT>(sCommonHeapSize),
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0
    };

    DX_THROW_FAILED( pDevice->CreateDescriptorHeap(
        &chd, __uuidof(ID3D12DescriptorHeap), &pCommonHeap_
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
    auto context = D3D12RenderContext(*this);
    renderer.render(scene, context, target);
}

void Core::preRender() {}

void Core::postRender() {}

void Core::waitGpu() {
    ++fenceValues_[fenceIdx_];
    DX_THROW_FAILED( pCmdQ_->Signal(pFence_.Get(), fenceValues_[fenceIdx_]) );

    if (pFence_->GetCompletedValue() < fenceValues_[fenceIdx_]) {
        DX_THROW_FAILED( pFence_->SetEventOnCompletion(fenceValues_[fenceIdx_], fenceEvent_) );
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void Core::waitGpu(std::size_t fenceIdx) {
    if (fenceIdx >= fenceValues_.size()) {
        throw GFX_EXCEPT("The given fence index \""s + std::to_string(fenceIdx) + "\" is out of range.\n"s
            "The valid range of the fence index is from 0 to "s + std::to_string(fenceValues_.size() - 1) + ".\n"s
        );
    }

    if (pFence_->GetCompletedValue() < fenceValues_[fenceIdx]) {
        DX_THROW_FAILED( pFence_->SetEventOnCompletion(fenceValues_[fenceIdx], fenceEvent_) );
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void Core::signalGpu(std::size_t fenceIdx) {
    if (fenceIdx >= fenceValues_.size()) {
        throw GFX_EXCEPT("The given fence index \""s + std::to_string(fenceIdx) + "\" is out of range.\n"s
            "The valid range of the fence index is from 0 to "s + std::to_string(fenceValues_.size() - 1) + ".\n"s
        );
    }

    ++fenceValues_[fenceIdx];
    DX_THROW_FAILED( pCmdQ_->Signal(pFence_.Get(), fenceValues_[fenceIdx]) );
}

void Core::alterFence() NOEXCEPT {
    fenceIdx_ = (fenceIdx_ + 1) % fenceValues_.size();
}

void Core::alterFence(std::size_t idx) {
    if (idx >= fenceValues_.size()) {
        throw GFX_EXCEPT("The given fence index \""s + std::to_string(idx) + "\" is out of range.\n"s
            "The valid range of the fence index is from 0 to "s + std::to_string(fenceValues_.size() - 1) + ".\n"s
        );
    }

    fenceIdx_ = idx;
}

std::unique_ptr<IRenderContext> Core::createContext() {
    return std::make_unique<D3D12RenderContext>(*this);
}

void Core::cleanup() {
    for (std::size_t i = 0; i < fenceValues_.size(); ++i) {
        signalGpu(i);
        waitGpu(i);
    }

    // the order of reset should be reversed from the order of creation. (the member layout order)
    fenceIdx_ = 0;
    fenceEvent_ = nullptr;
    fenceValues_ = { 0, 0 };
    pFence_.Reset();
    pDsvHeap_.Reset();
    pRtvHeap_.Reset();
    pCmdQ_.Reset();
    pDevice_.Reset();
    gfxCmdListPool_.clear();
    inputLayouts_.clear();
    shaders_.clear();
    upBufs_.clear();
    roots_.clear();
}

bool D3D12RenderContext::castableTo(RenderContextType contextType) const {
    return contextType == RenderContextType::D3D12;
}

std::any D3D12RenderContext::cast(RenderContextType contextType) {
    return pCmdList_;
}

void D3D12RenderContext::preRender() {
    DX_THROW_FAILED( pCmdAlloc_->Reset() );
    DX_THROW_FAILED( pCmdList_->Reset(pCmdAlloc_.Get(), nullptr) );
}

void D3D12RenderContext::postRender() {
    DX_THROW_FAILED( pCmdList_->Close() );

    ID3D12CommandList* ppCmdLists[] = { pCmdList_.Get() };
    DX_THROW_FAILED_VOID( pCmdQ_->ExecuteCommandLists(1, ppCmdLists) );
}

wrl::ComPtr<IDXGIFactory4> Core::spFactory = nullptr;
std::size_t Core::sRtvHeapSize = 0;
std::size_t Core::sDsvHeapSize = 0;
std::size_t Core::sCommonHeapSize = 0;

}   // namespace d3d12

}   // namespace gfx