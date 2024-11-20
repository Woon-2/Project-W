#include "dxutil/dxLow.hpp"

namespace gfx {

namespace dx {

DXGIFactory::DXGIFactory()
	: dx::DXWrapper<IDXGIFactory4>() {
#ifdef ENABLE_DXGI_INFO
	DX_THROW_FAILED( CreateDXGIFactory2( DXGI_CREATE_FACTORY_DEBUG,
		__uuidof(InterfaceType), &src_
	) );
#else
	DX_THROW_FAILED( CreateDXGIFactory2(0, __uuidof(InterfaceType), &src_) );
#endif
}

}   // namespace gfx::dx

}   // namespace gfx