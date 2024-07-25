#include "d3d12core.hpp"

namespace gfx {

namespace d3d12 {

void D3D12Core::init() {
    auto adapter = enumAdapters();
    createDevice(adapter.Get());
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

void D3D12Core::createDevice(IDXGIAdapter1* pAdapter) {
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

wrl::ComPtr<IDXGIFactory4> D3D12Core::spFactory = nullptr;
std::size_t D3D12Core::sRtvHeapSize = 0;
std::size_t D3D12Core::sDsvHeapSize = 0;

}   // namespace d3d12

}   // namespace gfx