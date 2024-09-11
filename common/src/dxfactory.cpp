#include "dxfactory.hpp"

namespace gfx {

void DXFactory::init() {
    UINT factoryFlag = 0;
#ifdef ENABLE_DXGI_INFO
    factoryFlag |= DXGI_CREATE_FACTORY_DEBUG;
#endif  // ENABLE_DXGI_INFO
    DX_THROW_FAILED(
        CreateDXGIFactory2(factoryFlag, __uuidof(IDXGIFactory4), &spFactory)
    );
}

void DXFactory::cleanup() {
    spFactory.Reset();
}

wrl::ComPtr<IDXGIFactory4> DXFactory::spFactory = nullptr;

}   // namespace gfx