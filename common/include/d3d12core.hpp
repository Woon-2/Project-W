#ifndef __D3D12CORE_HPP
#define __D3D12CORE_HPP

#include "gfx.hpp"

#include <d3d12.h>
#include "dxfactory.hpp"
#include "dxtarget.hpp"
#include "dxexcept.hpp"

#include <memory>

#define ENABLE_D3D12_WINDOW

#ifdef ENABLE_D3D12_WINDOW
#include "d3dwindow.hpp"
#endif  // ENABLE_D3D12_WINDOW

namespace gfx {

namespace wrl = Microsoft::WRL;

namespace d3d12 {

class Core : public ICore {
public:
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
    void cleanup() override;
    std::unique_ptr<IRenderContext> createContext() override;

private:
    // TODO: make it return multiple adapters enumerated.
    wrl::ComPtr<IDXGIAdapter1> enumAdapters();
    void createDevice(IDXGIAdapter1* pAdapter);
    void createCommandQueueAndList(ID3D12Device* pDevice);
    void buildRtvAndDsvHeaps(ID3D12Device* pDevice);

    static wrl::ComPtr<IDXGIFactory4> spFactory;
    static std::size_t sRtvHeapSize;
    static std::size_t sDsvHeapSize;

    wrl::ComPtr<ID3D12Device> pDevice_;
    wrl::ComPtr<ID3D12CommandQueue> pCmdQ_;
    wrl::ComPtr<ID3D12CommandAllocator> pCmdAlloc_;
    wrl::ComPtr<ID3D12GraphicsCommandList> pCmdList_;
    wrl::ComPtr<ID3D12DescriptorHeap> pRtvHeap_;
    wrl::ComPtr<ID3D12DescriptorHeap> pDsvHeap_;
};

class D3D12RenderContext : public IRenderContext {
public:
    D3D12RenderContext(ID3D12Device& device, ID3D12CommandAllocator& cmdAlloc, ID3D12GraphicsCommandList& cmdList);

    bool castableTo(const std::type_info& type) const override;
};

#ifdef ENABLE_D3D12_WINDOW
template <class Traits>
class Window : public D3DWindow<Traits>, public IRenderTarget {
public:

private:
};
#endif  // ENABLE_D3D12_WINDOW

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12CORE_HPP