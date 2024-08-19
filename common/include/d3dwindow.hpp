#ifndef __D3DWINDOW_HPP
#define __D3DWINDOW_HPP

#include "gfx.hpp"
#include "Window.hpp"
#include "dxtarget.hpp"
#include "dxexcept.hpp"

#include <string>
#include <string_view>

#include "config.hpp"

/**
 * @file d3dwindow.hpp
 */

namespace gfx {

// TODO: Store client rect
// TODO: make fullscreen toggle
/**
 * @brief A window with a DirectX's swapchain.    
 * It creates a swapchain when D3DWindow::open is called.    
 * 
 * The back buffers are created with the same size as the client area of the window.
 * 
 * D3DWindow::present presents the back buffer to the front buffer through the swapchain.     
 * 
 * `pSwapChain_`, `scd_`, and `scfd_`'s access level is protected to allow for more advanced usage for derived classes.
 * @tparam Traits Traits class for the window.
 * @note Currently only supports DXGI_FORMAT_R8G8B8A8_UNORM format and 2 back buffers,    
 * and it doesn't support multisampling.
 */
template <class Traits>
class D3DWindow : public Win32::Window<Traits> {
protected:
    wrl::ComPtr<IDXGISwapChain3> pSwapChain_{};
    DXGI_SWAP_CHAIN_DESC1 scd_{};
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC scfd_{};

public:
    using MyBase = Win32::Window<Traits>;
    using MyChar = typename Traits::MyChar;
    using MyString = typename Traits::MyString;
    using MyStringView = typename Traits::MyStringView;
    using MyBase::nativeHandle;
    using MyBase::defWndName;
    using MyBase::defWndFrame;

    static constexpr std::size_t defBackBufCnt = 2u;

    /**
     * @brief Opens the window with the default window name which specified in Win32::Window::defWndName     
     * and default window frame wich specified in Win32::Window::defWndFrame.    
     * @param pFactory DXGI factory to create the swapchain.
     * @param pDevice D3D device to create the swapchain.
     * @param backBufCnt The number of back buffers, the default value is D3DWindow::defBackBufCnt.
     * @see Win32::Window::defWndName win32::Window::defWndFrame D3DWindow::createSwapchain
     */
    void open(IDXGIFactory2* pFactory, IUnknown* pDevice, std::size_t backBufCnt = defBackBufCnt) {
        open(pFactory, pDevice, defWndName(), backBufCnt);
    }
    /**
     * @brief Opens the window with the specified window frame,    
     * The window name is set to the default window name which specified in Win32::Window::defWndName.
     * @param pFactory DXGI factory to create the swapchain.
     * @param pDevice D3D device to create the swapchain.
     * @param wndFrame The frame of the window.
     * @param backBufCnt The number of back buffers, the default value is D3DWindow::defBackBufCnt.
     * @see Win32::Window::defWndName D3DWindow::createSwapchain
    */
    void open( IDXGIFactory2* pFactory, IUnknown* pDevice, const Win32::WndFrame& wndFrame,
        std::size_t backBufCnt = defBackBufCnt
    ) {
        open(pFactory, pDevice, defWndName(), wndFrame, backBufCnt);
    }
    /**
     * @brief Opens the window with the specified window name,    
     * The window frame is set to the default window frame which specified in Win32::Window::defWndFrame.
     * @param pFactory DXGI factory to create the swapchain.
     * @param pDevice D3D device to create the swapchain.
     * @param wndName The name of the window.
     * @param backBufCnt The number of back buffers, the default value is D3DWindow::defBackBufCnt.
     * @see Win32::Window::defWndFrame D3DWindow::createSwapchain
     */
    void open( IDXGIFactory2* pFactory, IUnknown* pDevice, MyStringView wndName,
        std::size_t backBufCnt = defBackBufCnt
    ) {
        open(pFactory, pDevice, wndName, defWndFrame(), backBufCnt);
    }
    /**
     * @brief Opens the window with the specified window name and frame.
     * @param pFactory DXGI factory to create the swapchain.
     * @param pDevice D3D device to create the swapchain.
     * @param wndName The name of the window.
     * @param wndFrame The frame of the window.
     * @param backBufCnt The number of back buffers, the default value is D3DWindow::defBackBufCnt.
     * @details After opening the window instance, it creates a swapchain via calling D3DWindow::createSwapchain.    
     */
    void open( IDXGIFactory2* pFactory, IUnknown* pDevice, MyStringView wndName,
        const Win32::WndFrame& wndFrame, std::size_t backBufCnt = defBackBufCnt
    ) {
        MyBase::open(wndName, wndFrame);
        createSwapchain(pFactory, pDevice, backBufCnt);
    }

    // TODO: consider enabling multisampling
    // TODO: consider multiple back buffers
    /**
     * @brief Creates a swapchain for the window internally.
     * @param pFactory DXGI factory to create the swapchain.
     * @param pDevice D3D device to create the swapchain.
     * @param backBufCnt The number of back buffers.
     * @details The swapchain is created with the following settings:
     * The swapchain is created with the following settings:    
     * - Width and height are set to the client area of the window.    
     * - Format is DXGI_FORMAT_R8G8B8A8_UNORM.     
     * - Stereo is false.    
     * - Sample count is 1.    
     * - Buffer usage is DXGI_USAGE_RENDER_TARGET_OUTPUT.    
     * - Buffer count is backBufCnt.    
     * - Scaling is DXGI_SCALING_NONE.    
     * - Swap effect is DXGI_SWAP_EFFECT_FLIP_DISCARD.    
     * - Alpha mode is DXGI_ALPHA_MODE_UNSPECIFIED.    
     * - Flags is DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH.
     */
    void createSwapchain(IDXGIFactory2* pFactory, IUnknown* pDevice, std::size_t backBufCnt = defBackBufCnt) {
        auto tmp = wrl::ComPtr<IDXGISwapChain1>();

        scd_ = DXGI_SWAP_CHAIN_DESC1{
            .Width = static_cast<UINT>( this->client().width ),
            .Height = static_cast<UINT>( this->client().height ),
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .Stereo = false,
            .SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = static_cast<UINT>(backBufCnt),
            .Scaling = DXGI_SCALING_NONE,
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

        // TODO: buffer index storing
        // TODO: back buffer render targets
        DX_THROW_FAILED( pFactory->CreateSwapChainForHwnd(
            pDevice, nativeHandle(), &scd_, &scfd_, nullptr, &tmp
        ) );

        DX_THROW_FAILED( tmp.As(&pSwapChain_) );

        DX_THROW_FAILED( pFactory->MakeWindowAssociation(
            nativeHandle(), DXGI_MWA_NO_ALT_ENTER 
        ) );
    }

    /**
     * @brief Presents the back buffer to the front buffer through the swapchain.
     * @details The back buffer is presented with the following settings:    
     * - Sync interval is set to 1.    
     * - Flags is set to 0.
     */
    void present() {
        pSwapChain_->Present(1, 0);
    }

    /**
     * @brief Returns the number of back buffers.
     * @return `std::size_t` The number of back buffers.
     */
    std::size_t backBufCnt() const NOEXCEPT {
        return scd_.BufferCount;
    }
};

/**
 * @brief Traits class for D3DWindow.    
 * Besides the Win32::BasicWindowTraits functionalities, it provides the window class name for D3DWindow.    
 * If you need customization from this class, you can derive from it and override the static member functions.
 * @tparam `T` The character type.
 * @see Win32::BasicWindowTraits D3DWindow
 */
template <Win32::Win32Char T>
struct BasicD3DWTraits : public Win32::BasicWindowTraits<T> {
    using MyWindow = D3DWindow<BasicD3DWTraits>;
    using MyBase = Win32::BasicWindowTraits<T>;
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
};

}   // namespace gfx


#endif // __D3DWINDOW_HPP