#ifndef __dxLow_HPP
#define __dxLow_HPP

#include "Window.hpp"
#include "config.hpp"

#include "dxexcept.hpp"

#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXMath.h>
#include "directx/d3dcommon.h"

#include <type_traits>
#include <vector>
#include <string>
#include <string_view>

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

template <class Traits>
class DXWindow : public DXWrapper<IDXGISwapChain3>, public Win32::Window<Traits> {
protected:
    DXGI_SWAP_CHAIN_DESC1 scd_{};
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC scfd_{};
    bool fullScreen_{ false };

public:
    using MyBase = Win32::Window<Traits>;
    using MyChar = typename Traits::MyChar;
    using MyString = typename Traits::MyString;
    using MyStringView = typename Traits::MyStringView;
    using MyBase::nativeHandle;
    using MyBase::defWndName;
    using MyBase::defWndFrame;

    using DXWrapper<IDXGISwapChain3>::DXWrapper;

    static constexpr std::size_t defBackBufCnt = 3u;

    void open(DXGIFactory& factory, IUnknown* pDevice, std::size_t backBufCnt = defBackBufCnt) {
        open(factory, pDevice, defWndName(), backBufCnt);
    }

    void open( DXGIFactory& factory, IUnknown* pDevice, const Win32::WndFrame& wndFrame,
        std::size_t backBufCnt = defBackBufCnt
    ) {
        open(factory, pDevice, defWndName(), wndFrame, backBufCnt);
    }

    void open( DXGIFactory& factory, IUnknown* pDevice, MyStringView wndName,
        std::size_t backBufCnt = defBackBufCnt
    ) {
        open(factory, pDevice, wndName, defWndFrame(), backBufCnt);
    }

    void open( DXGIFactory& factory, IUnknown* pDevice, MyStringView wndName,
        const Win32::WndFrame& wndFrame, std::size_t backBufCnt = defBackBufCnt
    ) {
        MyBase::open(wndName, wndFrame);
        createSwapchain(factory, pDevice, backBufCnt);
    }

    // TODO: consider enabling multisampling
    void createSwapchain(DXGIFactory& factory, IUnknown* pDevice, std::size_t backBufCnt = defBackBufCnt) {
        auto tmp = wrl::ComPtr<IDXGISwapChain1>();

        scd_ = DXGI_SWAP_CHAIN_DESC1{
            .Width = static_cast<UINT>( this->client().width ),
            .Height = static_cast<UINT>( this->client().height ),
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .Stereo = false,
            .SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = static_cast<UINT>(backBufCnt),
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
            .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
        };

        scfd_ = DXGI_SWAP_CHAIN_FULLSCREEN_DESC{
            .RefreshRate = DXGI_RATIONAL{ .Numerator = 60, .Denominator = 1 },
            .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
            .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
            .Windowed = true
        };

        DX_THROW_FAILED( factory.get()->CreateSwapChainForHwnd(
            factory.get().Get(), nativeHandle(), &scd_, &scfd_, nullptr, &tmp
        ) );

        DX_THROW_FAILED( tmp.As(&get()) );

        DX_THROW_FAILED( factory.get()->MakeWindowAssociation(
            nativeHandle(), DXGI_MWA_NO_ALT_ENTER 
        ) );
    }

    void present() {
        DX_THROW_FAILED( get()->Present(1, 0) );
    }

    std::size_t backBufCnt() const NOEXCEPT {
        return scd_.BufferCount;
    }

    bool fullScreen() const NOEXCEPT {
        return fullScreen_;
    }

    bool windowed() const NOEXCEPT {
        return !fullScreen_;
    }

    void setFullScreen(void* pContext) {
        if ( fullScreen_ ) {
            return;
        }

        DX_THROW_FAILED( get()->SetFullscreenState(true, nullptr) );

        auto md = DXGI_MODE_DESC {
            .Width = scd_.Width,
            .Height = scd_.Height,
            .RefreshRate = scfd_.RefreshRate,
            .Format = scd_.Format,
            .ScanlineOrdering = scfd_.ScanlineOrdering,
            .Scaling = scfd_.Scaling
        };

        DX_THROW_FAILED( get()->ResizeTarget(&md) );

        preResizeBuffers(pContext);
        DX_THROW_FAILED( get()->ResizeBuffers(scd_.BufferCount, scd_.Width,
            scd_.Height, scd_.Format, scd_.Flags
        ) );
        postResizeBuffers(pContext);

        fullScreen_ = true;
    }

    void setWindowed(void* pContext) {
        if ( !fullScreen_ ) {
            return;
        }

        DX_THROW_FAILED( get()->SetFullscreenState(false, nullptr) );

        auto md = DXGI_MODE_DESC {
            .Width = scd_.Width,
            .Height = scd_.Height,
            .RefreshRate = scfd_.RefreshRate,
            .Format = scd_.Format,
            .ScanlineOrdering = scfd_.ScanlineOrdering,
            .Scaling = scfd_.Scaling
        };

        DX_THROW_FAILED( get()->ResizeTarget(&md) );

        preResizeBuffers(pContext);
        DX_THROW_FAILED( get()->ResizeBuffers(scd_.BufferCount, scd_.Width,
            scd_.Height, scd_.Format, scd_.Flags
        ) );
        postResizeBuffers(pContext);

        fullScreen_ = false;
    }

private:
    virtual void preResizeBuffers(void* pContext) = 0;
    virtual void postResizeBuffers(void* pContext) = 0;
};

/**
 * @brief Traits class for DXWindow.    
 * Besides the Win32::BasicWindowTraits functionalities, it provides the window class name for D3DWindow.    
 * If you need customization from this class, you can derive from it and override the static member functions.
 * @tparam `T` The character type.
 * @see Win32::BasicWindowTraits DXWindow
 */
template <Win32::Win32Char T>
struct BasicDXDWTraits : public Win32::BasicWindowTraits<T> {
    using MyWindow = DXWindow<BasicDXDWTraits>;
    using MyBase = Win32::BasicWindowTraits<T>;
    using MyChar = T;
    using MyString = std::basic_string<MyChar>;
    using MyStringView = std::basic_string_view<MyChar>;

    static constexpr const MyStringView clsName() NOEXCEPT {
        if constexpr ( std::is_same_v<MyChar, CHAR> ) {
            return "DXDW";
        }
        else /* WCHAR */ {
            return L"DXDW";
        }
    }
};

}    // namespace gfx::dx

}	// namespace gfx

#endif // __dxLow_HPP