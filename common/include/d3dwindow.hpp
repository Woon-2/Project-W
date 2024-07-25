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
    wrl::ComPtr<IDXGISwapChain1> pSwapChain_;
    RECT clientRect_;

public:
    using Win32::Window<Traits>::nativeHandle;

    // TODO: consider enabling multisampling
    // TODO: consider multiple back buffers
    void createSwapchain(IDXGIFactory2* pFactory, void* pDevice) {
        auto scd = DXGI_SWAP_CHAIN_DESC1{
            .Width = static_cast<UINT>( clientRect_.right - clientRect_.left ),
            .Height = static_cast<UINT>( clientRect_.bottom - clientRect_.top ),
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

        auto scfd = DXGI_SWAP_CHAIN_FULLSCREEN_DESC{
            .RefreshRate = DXGI_RATIONAL{ .Numerator = 60, .Denominator = 1 },
            .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
            .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
            .Windowed = true
        };

        // TODO: buffer index storing
        // TODO: back buffer render targets
        DX_THROW_FAILED( pFactory->CreateSwapChainForHwnd(
            pDevice, nativeHandle(), &scd, &scfd, nullptr, &pSwapChain_
        ) );

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

    static constexpr const MyStringView defWndName() NOEXCEPT {
        return Win32::BasicWindowTraits<MyChar>::defWndName();
    }

    static constexpr const Win32::WndFrame defWndFrame() NOEXCEPT {
        return Win32::BasicWindowTraits<MyChar>::defWndFrame();
    }

    static void regist(HINSTANCE hInst) {
        return Win32::BasicWindowTraits<MyChar>::regist(hInst);
    }

    static void unregist(HINSTANCE hInst) {
        return Win32::BasicWindowTraits<MyChar>::unregist(hInst);
    }

    static HWND create(HINSTANCE hInst, MyWindow* pWnd, IDXGIFactory2* pFactory, void* pDevice) {
        return create(hInst, pWnd, pFactory, pDevice, defWndName(), defWndFrame());
    }

    static HWND create( HINSTANCE hInst, MyWindow* pWnd, IDXGIFactory2* pFactory, void* pDevice,
        MyStringView wndName
    ) {
        return create(hInst, pWnd, pFactory, pDevice, wndName, defWndFrame());
    }

    static HWND create( HINSTANCE hInst, MyWindow* pWnd, IDXGIFactory2* pFactory, void* pDevice,
        const Win32::WndFrame& wndFrame
    ) {
        return create(hInst, pWnd, pFactory, pDevice, defWndName(), wndFrame);
    }

    static HWND create( HINSTANCE hInst, MyWindow* pWnd, IDXGIFactory2* pFactory, void* pDevice,
        MyStringView wndName, const Win32::WndFrame& wndFrame
    );

    static void destroy(HWND hWnd) {
        Win32::BasicWindowTraits<MyChar>::destroy(hWnd);
    }

    static void show(HWND hWnd, int nCmdShow) {
        Win32::BasicWindowTraits<MyChar>::show(hWnd, nCmdShow);
    }
};

template <Win32::Win32Char T>
HWND BasicD3DWTraits<T>::create( HINSTANCE hInst, MyWindow* pWnd, IDXGIFactory2* pFactory,
    void* pDevice, MyStringView wndName, const Win32::WndFrame& wndFrame
) {
    pWnd->createSwapchain(pFactory, pDevice);
}

}   // namespace gfx


#endif // __D3DWINDOW_HPP