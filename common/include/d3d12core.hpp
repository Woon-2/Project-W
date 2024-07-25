#ifndef __D3D12CORE_HPP
#define __D3D12CORE_HPP

#include "gfx.hpp"

#include <d3d12.h>
#include "dxtarget.hpp"

#include <memory>

namespace gfx {

namespace wrl = Microsoft::WRL;

namespace d3d12 {

class D3D12Core : public ICore {
public:
    D3D12Core(IDXGIFactory7& factory, IDXGIAdapter4& adapter);

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
    static std::size_t sRtvHeapSize;
    static std::size_t sDsvHeapSize;

    wrl::ComPtr<ID3D12Device> pDevice_;
    wrl::ComPtr<ID3D12CommandQueue> pCmdQ_;
    wrl::ComPtr<ID3D12CommandAllocator> pCmdAlloc_;
    wrl::ComPtr<ID3D12GraphicsCommandList> pCmdList_;
    wrl::ComPtr<ID3D12DescriptorHeap> pRtvHeap_;
    wrl::ComPtr<ID3D12DescriptorHeap> pDsvHeap_;
};

std::size_t D3D12Core::sRtvHeapSize = 0;
std::size_t D3D12Core::sDsvHeapSize = 0;

class D3D12RenderContext : public IRenderContext {
public:
    D3D12RenderContext(ID3D12Device& device, ID3D12CommandAllocator& cmdAlloc, ID3D12GraphicsCommandList& cmdList);

    bool castableTo(const std::type_info& type) const override;
};

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12CORE_HPP