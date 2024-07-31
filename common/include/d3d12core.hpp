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
template <class Traits>
class Window : public D3DWindow<Traits>, public IRenderTarget {
public:
    Window() : backBuffers_(2), depthBuffers_(1) {}
    void buildRtv(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE pFirstRtv);
    void buildDsv(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE pFirstDsv);
    void createDepthBuffers(ID3D12Device* pDevice);

    bool castableTo(RenderTargetType rentarType) const override;
    std::any cast(RenderTargetType rentarType) override;
    // TODO: CPU - GPU synchronization
    void preRender(IRenderContext& renderContext) override;
    void postRender(IRenderContext& renderContext) override {
        this->present();
    }

private:
    std::vector<wrl::ComPtr<ID3D12Resource>> backBuffers_;
    std::vector<wrl::ComPtr<ID3D12Resource>> depthBuffers_;
    D3D12_CPU_DESCRIPTOR_HANDLE pFirstRtv_;
    D3D12_CPU_DESCRIPTOR_HANDLE pFirstDsv_;
};

// TODO: make D3DWindow's back buffer count modifiable, and reflect that in here. 
template <class Traits>
void Window<Traits>::buildRtv(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE pFirstRtv) {
    auto stride = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    
    for (auto i = 0; i < 2u; ++i) {
        this->pSwapChain_->GetBuffer(i, __uuidof(ID3D12Resource), &backBuffers_[i]);
        this->pDevice->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, pFirstRtv);
        pFirstRtv.ptr += stride;
    }

    pFirstRtv_ = pFirstRtv;
}

// TODO: deal with multiple depth stencils.
template <class Traits>
void Window<Traits>::buildDsv(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE pFirstDsv) {
    pDevice->CreateDepthStencilView(depthBuffers_[0].Get(), nullptr, pFirstDsv);

    pFirstDsv_ = pFirstDsv;
}

template <class Traits>
void Window<Traits>::createDepthBuffers(ID3D12Device* pDevice) {
    auto depthDesc = D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = static_cast<UINT>(this->clientRect_.right - this->clientRect_.left),
        .Height = static_cast<UINT>(this->clientRect_.bottom - this->clientRect_.top),
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
        return pFirstRtv_;
    case RenderTargetType::D3D12_DEPTH:
        return pFirstDsv_;
    default:
        throw GFX_EXCEPT("Cannot cast to the requested render target type.");
    }
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
            return "D3DW";
        }
        else /* WCHAR */ {
            return L"D3DW";
        }
    }

    static HWND create(HINSTANCE hInst, MyWindow* pWnd, IDXGIFactory2* pFactory, void* pDevice) {
        return create(hInst, pWnd, pFactory, pDevice, MyBase::defWndName(), MyBase::defWndFrame());
    }

    static HWND create( HINSTANCE hInst, MyWindow* pWnd, IDXGIFactory2* pFactory, void* pDevice,
        MyStringView wndName
    ) {
        return create(hInst, pWnd, pFactory, pDevice, wndName, MyBase::defWndFrame());
    }

    static HWND create( HINSTANCE hInst, MyWindow* pWnd, IDXGIFactory2* pFactory, void* pDevice,
        const Win32::WndFrame& wndFrame
    ) {
        return create(hInst, pWnd, pFactory, pDevice, MyBase::defWndName(), wndFrame);
    }

    static HWND create( HINSTANCE hInst, MyWindow* pWnd, IDXGIFactory2* pFactory, void* pDevice,
        MyStringView wndName, const Win32::WndFrame& wndFrame
    );
};

template <Win32::Win32Char T>
HWND BasicD3D12WTraits<T>::create( HINSTANCE hInst, MyWindow* pWnd, IDXGIFactory2* pFactory, void* pDevice,
    MyStringView wndName, const Win32::WndFrame& wndFrame
) {
    auto ret = BasicD3DWTraits<T>::create(hInst, pWnd, wndName, wndFrame);
    pWnd->createDepthBuffers(static_cast<ID3D12Device*>(pDevice));
}

#endif  // ENABLE_D3D12_WINDOW

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12CORE_HPP