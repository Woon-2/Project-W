#include "dxutil/dxLow.hpp"

namespace gfx {

namespace dx {

DirectX::XMMATRIX XM_CALLCONV loadMat(const DirectX::XMFLOAT4X4& mat) {
	return DirectX::XMLoadFloat4x4(&mat);
}
DirectX::XMMATRIX XM_CALLCONV loadMat(const DirectX::XMFLOAT3X3& mat) {
	return DirectX::XMLoadFloat3x3(&mat);
}

void XM_CALLCONV storeMat(DirectX::FXMMATRIX src, DirectX::XMFLOAT4X4& dst) {
	DirectX::XMStoreFloat4x4(&dst, src);
}
void XM_CALLCONV storeMat(DirectX::FXMMATRIX src, DirectX::XMFLOAT3X3& dst) {
	DirectX::XMStoreFloat3x3(&dst, src);
}

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