#ifndef __dxLow_HPP
#define __dxLow_HPP

#include "window.hpp"

#include "config.hpp"

#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXMath.h>
#include "directx/d3dcommon.h"

#include <type_traits>
#include <vector>

namespace gfx {

namespace wrl = Microsoft::WRL;

namespace dx {

using XMUINT2 = DirectX::XMUINT2;
using XMUINT3 = DirectX::XMUINT3;
using XMUINT4 = DirectX::XMUINT4;
using XMFLOAT2 = DirectX::XMFLOAT2;
using XMFLOAT3 = DirectX::XMFLOAT3;
using XMFLOAT4 = DirectX::XMFLOAT4;
using XMFLOAT3X3 = DirectX::XMFLOAT3X3;
using XMFLOAT4X4 = DirectX::XMFLOAT4X4;

template <class T>
T XM_CALLCONV convertMat(DirectX::FXMMATRIX mat) {
    T result;
    if constexpr (std::is_same_v<T, DirectX::XMFLOAT4X4>) {
        DirectX::XMStoreFloat4x4(&result, mat);
    } else if constexpr (std::is_same_v<T, DirectX::XMFLOAT3X3>) {
        DirectX::XMStoreFloat3x3(&result, mat);
    } else if constexpr (std::is_same_v<T, DirectX::XMFLOAT3X4>) {
        DirectX::XMStoreFloat3x4(&result, mat);
    }
    return result;
}


template <class TInterface>
class DXWrapper {
public:
	using InterfaceType = TInterface;

	DXWrapper() = default;

	DXWrapper(const wrl::ComPtr<InterfaceType>& src) NOEXCEPT
		: src_(src) {}

	DXWrapper(wrl::ComPtr<InterfaceType>&& src) NOEXCEPT
		: src_(std::move(src)) {}

	template <class T>
	wrl::ComPtr<T> as() {
		wrl::ComPtr<T> ret{};
		src_.As<T>(&ret);
		return ret;
	}

	auto& get() NOEXCEPT {
		return src_;
	}

	const auto& get() const NOEXCEPT {
		return src_;
	}

protected:
	wrl::ComPtr<InterfaceType> src_;
};

class DXGIAdapter : public DXWrapper<IDXGIAdapter1> {
public:
	using DXWrapper<IDXGIAdapter1>::DXWrapper;
};

class DXGIFactory : public DXWrapper<IDXGIFactory4> {
public:
	DXGIFactory();
};

}    // namespace gfx::dx

}	// namespace gfx

#endif // __dxLow_HPP