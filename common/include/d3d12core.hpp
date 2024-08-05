#ifndef __D3D12CORE_HPP
#define __D3D12CORE_HPP

#include "gfx.hpp"

#include <d3d12.h>
#include "dxfactory.hpp"
#include "dxtarget.hpp"
#include "dxexcept.hpp"

#include <memory>
#include <vector>
#include <array>
#include <map>

#define ENABLE_D3D12_WINDOW

#ifdef ENABLE_D3D12_WINDOW
#include "d3dwindow.hpp"
#endif  // ENABLE_D3D12_WINDOW

namespace gfx {

namespace wrl = Microsoft::WRL;

namespace d3d12 {

class Core : public ICore {
public:
    friend class D3D12RenderContext;
#ifdef ENABLE_D3D12_WINDOW
    friend class WindowAttorney;
#endif  // ENABLE_D3D12_WINDOW
    using RootIdx = std::size_t;

    static void configDXFactory(wrl::ComPtr<IDXGIFactory4> factory) {
        spFactory = factory;
    }
    static wrl::ComPtr<IDXGIFactory4> dxFactory() {
        return spFactory;
    }

    static void configRtvHeapSize(std::size_t size) {
        sRtvHeapSize = size;
    }
    static std::size_t rtvHeapSize() {
        return sRtvHeapSize;
    }
    static void configDsvHeapSize(std::size_t size) {
        sDsvHeapSize = size;
    }
    static std::size_t dsvHeapSize() {
        return sDsvHeapSize;
    }

    void init() override;
    void render(const IScene& scene, const IRenderer& renderer, IRenderTarget& target) override;
    void preRender() override;
    void postRender() override;
    void cleanup() override;
     
    std::unique_ptr<IRenderContext> createContext() override;

    void waitForGpu();
    void alterFence();
    void alterFence(std::size_t idx);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapStart() const NOEXCEPT {
        return pRtvHeap_->GetCPUDescriptorHandleForHeapStart();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHeapStart() const NOEXCEPT {
        return pDsvHeap_->GetCPUDescriptorHandleForHeapStart();
    }

    wrl::ComPtr<ID3D12RootSignature> root(RootIdx idx) const NOEXCEPT {
        if (roots_.contains(idx)) {
            return roots_.at(idx);
        }
        return nullptr;
    }

    wrl::ComPtr<ID3D12RootSignature> root(const IRenderer* pRenderer) const NOEXCEPT {
        if (rootMap_.contains(pRenderer)) {
            return root(rootMap_.at(pRenderer));
        }
        return nullptr;
    }

    void addRoot(RootIdx idx, ID3D12RootSignature* pRoot) {
        roots_[idx] = pRoot;
    }

    void mapRoot(const IRenderer* pRenderer, RootIdx idx) {
        rootMap_[pRenderer] = idx;
    }

private:
    // TODO: make it return multiple adapters enumerated.
    wrl::ComPtr<IDXGIAdapter1> enumAdapters();
    void createDevice(IDXGIAdapter1* pAdapter);
    void createCommandQueueAndList(ID3D12Device* pDevice);
    void buildRtvAndDsvHeaps(ID3D12Device* pDevice);
    void createFenceAndEvent(ID3D12Device* pDevice);

    wrl::ComPtr<ID3D12GraphicsCommandList> cmdList() NOEXCEPT {
        return pCmdList_;
    }

    static wrl::ComPtr<IDXGIFactory4> spFactory;
    static std::size_t sRtvHeapSize;
    static std::size_t sDsvHeapSize;

    std::map<RootIdx, wrl::ComPtr<ID3D12RootSignature>> roots_;
    std::map<const IRenderer*, RootIdx> rootMap_;
    wrl::ComPtr<ID3D12Device> pDevice_;
    wrl::ComPtr<ID3D12CommandQueue> pCmdQ_;
    wrl::ComPtr<ID3D12CommandAllocator> pCmdAlloc_;
    wrl::ComPtr<ID3D12GraphicsCommandList> pCmdList_;
    wrl::ComPtr<ID3D12DescriptorHeap> pRtvHeap_;
    wrl::ComPtr<ID3D12DescriptorHeap> pDsvHeap_;
    wrl::ComPtr<ID3D12Fence> pFence_;
    std::array<UINT64, 2> fenceValues_ = { 0, 0 };
    HANDLE fenceEvent_ = nullptr;
    std::size_t fenceIdx_ = 0;
};

class D3D12RenderContext : public IRenderContext {
public:
    D3D12RenderContext(Core& core)
        : pCmdList_(core.cmdList()) {}

    bool castableTo(RenderContextType contextType) const override;
    std::any cast(RenderContextType contextType) override;

private:
    wrl::ComPtr<ID3D12GraphicsCommandList> pCmdList_;
};

#ifdef ENABLE_D3D12_WINDOW
class WindowAttorney {
public:
    template <class Traits>
    friend class Window;

private:
    static void* factory(Core& core) {
        return core.spFactory.Get();
    }

    static void* cmdQ(Core& core) {
        return core.pCmdQ_.Get();
    }

    static void* device(Core& core) {
        return core.pDevice_.Get();
    }
};

template <class Traits>
class Window : public D3DWindow<Traits>, public IRenderTarget {
protected:
    void* getFactoryFromCore(Core& core) {
        return WindowAttorney::factory(core);
    }

    void* getCmdQFromCore(Core& core) {
        return WindowAttorney::cmdQ(core);
    }

    void* getDeviceFromCore(Core& core) {
        return WindowAttorney::device(core);
    }

public:
    template <Win32::Win32Char T>
    friend struct BasicD3D12WTraits;

    using MyBase = D3DWindow<Traits>;
    using MyChar = typename Traits::MyChar;
    using MyString = typename Traits::MyString;
    using MyStringView = typename Traits::MyStringView;
    using MyBase::nativeHandle;
    using MyBase::defWndName;
    using MyBase::defWndFrame;

    Window()
        : backBuffers_(2), depthBuffers_(1), pFirstRtv_(), pFirstDsv_(),
        rtvStride_(0), dsvStride_(0) {}

    void open(Core& core) {
        open(core, defWndName());
    }

    void open(Core& core, const Win32::WndFrame& wndFrame) {
        open(core, defWndName(), wndFrame);
    }

    void open(Core& core, MyStringView wndName) {
        open(core, wndName, defWndFrame());
    }

    // TODO: replace versioned type with type aliases
    void open(Core& core, MyStringView wndName, const Win32::WndFrame& wndFrame) {
        MyBase::open( static_cast<IDXGIFactory2*>( WindowAttorney::factory(core) ),
            static_cast<ID3D12CommandQueue*>( WindowAttorney::cmdQ(core) ),
            wndName, wndFrame
        );
        auto pDevice = static_cast<ID3D12Device*>( WindowAttorney::device(core) );
        createDepthBuffers( pDevice );
        buildRtv( pDevice, core.rtvHeapStart() );
        buildDsv( pDevice, core.dsvHeapStart() );
    }

    void buildRtv(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE pFirstRtv);
    void buildDsv(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE pFirstDsv);
    void createDepthBuffers(ID3D12Device* pDevice);

    bool castableTo(RenderTargetType rentarType) const override;
    std::any cast(RenderTargetType rentarType) override;
    // TODO: CPU - GPU synchronization
    void clear(IRenderContext& renderContext) override;
    void preRender(IRenderContext& renderContext) override;
    void postRender(IRenderContext& renderContext) override;

private:
    std::vector<wrl::ComPtr<ID3D12Resource>> backBuffers_;
    std::vector<wrl::ComPtr<ID3D12Resource>> depthBuffers_;
    D3D12_CPU_DESCRIPTOR_HANDLE pFirstRtv_;
    D3D12_CPU_DESCRIPTOR_HANDLE pFirstDsv_;
    UINT rtvStride_;
    UINT dsvStride_;
};

// TODO: make D3DWindow's back buffer count modifiable, and reflect that in here. 
template <class Traits>
void Window<Traits>::buildRtv(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE pFirstRtv) {
    rtvStride_ = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    pFirstRtv_ = pFirstRtv;
    
    for (auto i = 0; i < 2u; ++i) {
        this->pSwapChain_->GetBuffer(i, __uuidof(ID3D12Resource), &backBuffers_[i]);
        pDevice->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, pFirstRtv);
        pFirstRtv.ptr += rtvStride_;
    }
}

// TODO: deal with multiple depth stencils.
template <class Traits>
void Window<Traits>::buildDsv(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE pFirstDsv) {
    dsvStride_ = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    pDevice->CreateDepthStencilView(depthBuffers_[0].Get(), nullptr, pFirstDsv);

    pFirstDsv_ = pFirstDsv;
}

template <class Traits>
void Window<Traits>::createDepthBuffers(ID3D12Device* pDevice) {
    auto depthDesc = D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = static_cast<UINT>(this->client().width),
        .Height = static_cast<UINT>(this->client().height),
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    };

    auto heapProp = D3D12_HEAP_PROPERTIES{
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0
    };

    auto cv = D3D12_CLEAR_VALUE{
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .DepthStencil = { .Depth = 1.0f, .Stencil = 0u }
    };

    DX_THROW_FAILED( pDevice->CreateCommittedResource(
        &heapProp, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, __uuidof(ID3D12Resource), &depthBuffers_[0]
    ) );
}

template <class Traits>
bool Window<Traits>::castableTo(RenderTargetType rentarType) const {
    return rentarType == RenderTargetType::D3D12;
}

template <class Traits>
std::any Window<Traits>::cast(RenderTargetType rentarType) {
    switch (rentarType) {
    case RenderTargetType::D3D12:
        return D3D12_CPU_DESCRIPTOR_HANDLE{
            .ptr = pFirstRtv_.ptr + this->pSwapChain_->GetCurrentBackBufferIndex() * rtvStride_
        };   
    case RenderTargetType::D3D12_DEPTH:
        return pFirstDsv_;
    default:
        throw GFX_EXCEPT("Cannot cast to the requested render target type.");
    }
}

template <class Traits>
void Window<Traits>::clear(IRenderContext& renderContext) {
    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        renderContext.cast(RenderContextType::D3D12)
    );

    auto bufIdx = this->pSwapChain_->GetCurrentBackBufferIndex();
    auto pRtv = D3D12_CPU_DESCRIPTOR_HANDLE{
        .ptr = pFirstRtv_.ptr + bufIdx * rtvStride_
    };

    static constexpr float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };

    DX_THROW_FAILED_VOID( pCmdList->ClearRenderTargetView(
        pRtv, clearColor, 0, nullptr
    ) );

    DX_THROW_FAILED_VOID( pCmdList->ClearDepthStencilView(
        pFirstDsv_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0, nullptr
    ) );
}

template <class Traits>
void Window<Traits>::preRender(IRenderContext& renderContext) {
    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        renderContext.cast(RenderContextType::D3D12)
    );

    const auto bar = D3D12_RESOURCE_BARRIER{
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
            .pResource = backBuffers_[this->pSwapChain_->GetCurrentBackBufferIndex()].Get(),
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
            .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
        }
    };

    pCmdList->ResourceBarrier(1, &bar);
}

template <class Traits>
void Window<Traits>::postRender(IRenderContext& renderContext) {
    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        renderContext.cast(RenderContextType::D3D12)
    );

    const auto bar = D3D12_RESOURCE_BARRIER{
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
            .pResource = backBuffers_[this->pSwapChain_->GetCurrentBackBufferIndex()].Get(),
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
            .StateAfter = D3D12_RESOURCE_STATE_PRESENT
        }
    };

    pCmdList->ResourceBarrier(1, &bar);
}

template <Win32::Win32Char T>
struct BasicD3D12WTraits : public BasicD3DWTraits<T> {
    using MyWindow = Window<BasicD3D12WTraits>;
    using MyBase = BasicD3DWTraits<T>;
    using MyChar = T;
    using MyString = std::basic_string<MyChar>;
    using MyStringView = std::basic_string_view<MyChar>;

    static constexpr const MyStringView clsName() NOEXCEPT {
        if constexpr (std::is_same_v<MyChar, CHAR>) {
            return "D3D12W";
        }
        else /* WCHAR */ {
            return L"D3D12W";
        }
    }
};

#endif  // ENABLE_D3D12_WINDOW

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12CORE_HPP