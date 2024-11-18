#ifndef __testInput_HPP
#define __testInput_HPP

#include "d3d12engine/d3d12Engine.hpp"

#include "mouse.hpp"

#include <string>

template <class Wnd>
class TestInputHandler : public Win32::MsgHandler<Wnd> {
public:
    using ::Win32::MsgHandler<Wnd>::window;
    using MyWindow = Wnd;
    using MyChar = typename MyWindow::MyChar;
    using MyString = std::basic_string<MyChar>;

    TestInputHandler(MyWindow& wnd, ic::Mouse& mouse, gfx::d3d12engine::Core& core)
        : Win32::MsgHandler<Wnd>(wnd), pMouse_(&mouse), pCore_(&core) {}

    std::optional<LRESULT> operator()(
        const ::Win32::Message& msg
    ) override {
        auto pWnd = static_cast<MyWindow*>( &window() );

        switch (msg.type) {
        case WM_KEYDOWN:
            handleKeyDown(pWnd, msg.wParam);
            return 0;

        case WM_KEYUP:
            handleKeyUp(pWnd, msg.wParam);
            return 0;

        case WM_ACTIVATE:
            if (msg.wParam & WA_ACTIVE) {
                confineCursor(pWnd);
            }
            else {
                pMouse_->freeCursor();
            }
            return 0;

        case WM_LBUTTONDOWN:
            if (!pMouse_->cursorConfined()) {
                confineCursor(pWnd);
            }
            return 0;

        default:
            break;
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
                confineCursor(pWnd);
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
                pCore_->setWindowed();
            }
            else {
                pCore_->setFullScreen();
            }
            break;

        default:
            break;
        }
    }

    void handleKeyUp(MyWindow* pWnd, WPARAM wParam) {

    }

    void confineCursor(MyWindow* pWnd) {
        auto clRect = pWnd->client();
        ::MapWindowPoints(pWnd->nativeHandle(), nullptr, reinterpret_cast<POINT*>(&clRect), 2);
        pMouse_->confineCursor(&clRect);
    }

    ic::Mouse* pMouse_;
    gfx::d3d12engine::Core* pCore_;
};

#endif // __testInput_HPP