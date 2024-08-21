#ifndef __testInput_HPP
#define __testInput_HPP

#include "Window.hpp"
#include "mouse.hpp"

#include "gfx.hpp"

#include <string>

template <class Wnd>
class TestInputHandler : public Win32::MsgHandler<Wnd> {
public:
    using ::Win32::MsgHandler<Wnd>::window;
    using MyWindow = Wnd;
    using MyChar = typename MyWindow::MyChar;
    using MyString = std::basic_string<MyChar>;

    TestInputHandler(MyWindow& wnd, ic::Mouse& mouse, gfx::ICore& gfx)
        : Win32::MsgHandler<Wnd>(wnd), pMouse_(&mouse), pGfx_(&gfx) {}

    std::optional<LRESULT> operator()(
        const ::Win32::Message& msg
    ) override {
        auto pWnd = static_cast<MyWindow*>( &window() );

        if (msg.type == WM_KEYDOWN) {
            handleKeyDown(pWnd, msg.wParam);
            return 0;
        }
        else if (msg.type == WM_KEYUP) {
            handleKeyUp(pWnd, msg.wParam);
            return 0;
        }

        return {};
    }

private:
    void handleKeyDown(MyWindow* pWnd, WPARAM wParam) {
        switch (wParam) {
        case VK_F7:
            if (pMouse_->cursorConfined()) {
                pMouse_->freeCursor();
            }
            else {
                auto clRect = pWnd->client();
                ::MapWindowPoints(pWnd->nativeHandle(), nullptr, reinterpret_cast<POINT*>(&clRect), 2);
                pMouse_->confineCursor(&clRect);
            }
            break;

        case VK_F8:
            if (pMouse_->cursorShown()) {
                pMouse_->hideCursor();
            }
            else {
                pMouse_->showCursor();
            }
            break;

        case VK_F9:
            if (pWnd->fullScreen()) {
                pWnd->setWindowed(*pGfx_);
            }
            else {
                pWnd->setFullScreen(*pGfx_);
            }
            break;

        default:
            break;
        }
    }

    void handleKeyUp(MyWindow* pWnd, WPARAM wParam) {

    }

    ic::Mouse* pMouse_;
    gfx::ICore* pGfx_;
};

#endif // __testInput_HPP