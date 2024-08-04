#ifndef __D3DWINDOW_HPP
#define __D3DWINDOW_HPP

#include "gfx.hpp"
#include "Window.hpp"
#include "dxtarget.hpp"
#include "dxexcept.hpp"

#include <string>
#include <string_view>

namespace gfx {

// TODO: Store client rect
// TODO: make fullscreen toggle
template <class Traits>
class D3DWindow : public Win32::Window<Traits> {
protected:
    wrl::ComPtr<IDXGISwapChain3> pSwapChain_;

public:
    using MyBase = Win32::Window<Traits>;
    using MyChar = typename Traits::MyChar;
    using MyString = typename Traits::MyString;
    using MyStringView = typename Traits::MyStringView;
    using MyBase::nativeHandle;
    using MyBase::defWndName;
    using MyBase::defWndFrame;

    void open(IDXGIFactory2* pFactory, IUnknown* pDevice) {
        open(pFactory, pDevice, defWndName());
    }

    void open(IDXGIFactory2* pFactory, IUnknown* pDevice, const Win32::WndFrame& wndFrame) {
        open(pFactory, pDevice, defWndName(), wndFrame);
    }

    void open(IDXGIFactory2* pFactory, IUnknown* pDevice, MyStringView wndName) {
        open(pFactory, pDevice, wndName, defWndFrame());
    }

    void open(IDXGIFactory2* pFactory, IUnknown* pDevice, MyStringView wndName, const Win32::WndFrame& wndFrame) {
        MyBase::open(wndName, wndFrame);
        createSwapchain(pFactory, pDevice);
    }

    // TODO: consider enabling multisampling
    // TODO: consider multiple back buffers
    void createSwapchain(IDXGIFactory2* pFactory, IUnknown* pDevice) {
        auto tmp = wrl::ComPtr<IDXGISwapChain1>();

        const auto scd = DXGI_SWAP_CHAIN_DESC1{
            .Width = static_cast<UINT>( this->client().width ),
            .Height = static_cast<UINT>( this->client().height ),
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .Stereo = false,
            .SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = 2u,
            .Scaling = DXGI_SCALING_NONE,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
            .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
        };

        const auto scfd = DXGI_SWAP_CHAIN_FULLSCREEN_DESC{
            .RefreshRate = DXGI_RATIONAL{ .Numerator = 60, .Denominator = 1 },
            .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
            .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
            .Windowed = true
        };

        // TODO: buffer index storing
        // TODO: back buffer render targets
        DX_THROW_FAILED( pFactory->CreateSwapChainForHwnd(
            pDevice, nativeHandle(), &scd, &scfd, nullptr, &tmp
        ) );

        DX_THROW_FAILED( tmp.As(&pSwapChain_) );

        DX_THROW_FAILED( pFactory->MakeWindowAssociation(
            nativeHandle(), DXGI_MWA_NO_ALT_ENTER 
        ) );
    }

    void present() {
        pSwapChain_->Present(1, 0);
    }
};

template <Win32::Win32Char T>
struct BasicD3DWTraits {
    using MyWindow = D3DWindow<BasicD3DWTraits>;
    using MyChar = T;
    using MyString = std::basic_string<MyChar>;
    using MyStringView = std::basic_string_view<MyChar>;

    static constexpr const MyStringView clsName() NOEXCEPT {
        if constexpr ( std::is_same_v<MyChar, CHAR> ) {
            return "D3DW";
        }
        else /* WCHAR */ {
            return L"D3DW";
        }
    }

    static void regist(HINSTANCE hInst) {
        return Win32::BasicWindowTraits<MyChar>::regist(hInst);
    }

    static void unregist(HINSTANCE hInst) {
        return Win32::BasicWindowTraits<MyChar>::unregist(hInst);
    }

    static void destroy(HWND hWnd) {
        Win32::BasicWindowTraits<MyChar>::destroy(hWnd);
    }

    static void show(HWND hWnd, int nCmdShow) {
        Win32::BasicWindowTraits<MyChar>::show(hWnd, nCmdShow);
    }
};

}   // namespace gfx


#endif // __D3DWINDOW_HPP