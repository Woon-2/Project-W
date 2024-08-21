#ifndef __MouseWin32Adaptor_HPP
#define __MouseWin32Adaptor_HPP

#include "mouse.hpp"
#include "Window.hpp"

#include <vector>
#include <cstdint>

namespace ic {

namespace Win32 {

class Mouse : public ic::Mouse {
public:
    void setPos(const Point& pt) override {
        SetCursorPos(pt.x, pt.y);
    }

    void hideCursor() override {
        while (ShowCursor(false) >= 0)
            ;
        cursorShown_ = false;
    }

    void showCursor() override {
        while (ShowCursor(true) < 0)
            ;
        cursorShown_ = true;
    }

    void confineCursor(void* pScreenRect) override {
        auto& screenRect = *static_cast<const RECT*>(pScreenRect);
        ClipCursor(&screenRect);
        cursorConfined_ = true;
    }

    void freeCursor() override {
        ClipCursor(nullptr);
        cursorConfined_ = false;
    }

    bool cursorShown() const override {
        return cursorShown_;
    }

    bool cursorConfined() const override {
        return cursorConfined_;
    }

private:
    bool cursorShown_ = true;
    bool cursorConfined_ = false;
};

template <class Wnd>
class MouseMsgHandler : public ::Win32::MsgHandler<Wnd>{
public:
    using ::Win32::MsgHandler<Wnd>::window;
    using MyWindow = Wnd;
    using MyChar = typename MyWindow::MyChar;
    using MyMouse = ic::Mouse;
    using MyMouseMsgAPI = MouseMsgAPI;
    using MyString = std::basic_string<MyChar>;

    MouseMsgHandler(MyWindow& wnd, MyMouse* pMouse)
        : ::Win32::MsgHandler<MyWindow>(wnd), pMouse_(pMouse), cursorEnabled_(true) {
        auto rid = RAWINPUTDEVICE{
            .usUsagePage = 0x01,    // Generic Desktop Controls
            .usUsage = 0x02,    // Mouse
            .dwFlags = 0,
            .hwndTarget = nullptr   // NULL for the whole system
        };

        if ( !RegisterRawInputDevices(&rid, 1, sizeof(rid)) ) {
            throw WND_LAST_EXCEPT();
        }
    }

    std::optional<LRESULT> operator()(
        const ::Win32::Message& msg
    ) override {
        static auto sRawInputBuffer = std::vector<std::uint8_t>(256);

        auto pt = makePoint(msg.lParam);

        switch (msg.type) {
        case WM_MOUSEMOVE:
            if ( insideClient(pt) ) {
                MyMouseMsgAPI::onMouseMove(*pMouse_, pt);

                if ( !pMouse_->inWindow() ) {
                    SetCapture( window().nativeHandle() );
                    MyMouseMsgAPI::onEnter(*pMouse_, pt);
                }
            }
            else {
                if ( msg.wParam & (MK_LBUTTON | MK_RBUTTON) ) {
                    MyMouseMsgAPI::onMouseMove(*pMouse_, pt);
                }
                else {
                    ReleaseCapture();
                    MyMouseMsgAPI::onLeave(*pMouse_, pt);
                }
            }
            return 0;

        case WM_LBUTTONDOWN:
            MyMouseMsgAPI::onLeftPressed(*pMouse_, pt);
            return 0;

        case WM_LBUTTONUP:
            MyMouseMsgAPI::onLeftReleased(*pMouse_, pt);
            return 0;

        case WM_RBUTTONDOWN:
            MyMouseMsgAPI::onRightPressed(*pMouse_, pt);
            return 0;

        case WM_RBUTTONUP:
            MyMouseMsgAPI::onRightReleased(*pMouse_, pt);
            return 0;

        case WM_MBUTTONDOWN:
            MyMouseMsgAPI::onMidPressed(*pMouse_, pt);
            return 0;

        case WM_MBUTTONUP:
            MyMouseMsgAPI::onMidReleased(*pMouse_, pt);
            return 0;

        case WM_MOUSEWHEEL:
            MyMouseMsgAPI::onMouseWheel(*pMouse_, pt,
                GET_WHEEL_DELTA_WPARAM(msg.wParam)
            );
            return 0;

        case WM_INPUT: {
            auto size = UINT{};

            if ( GetRawInputData(
                reinterpret_cast<HRAWINPUT>(msg.lParam),
                RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)
            ) == -1) {
                throw WND_LAST_EXCEPT();
            }

            if (size > sRawInputBuffer.size()) {
                sRawInputBuffer.resize(size);
            }

            if (GetRawInputData(
                reinterpret_cast<HRAWINPUT>(msg.lParam),
                RID_INPUT, sRawInputBuffer.data(), &size, sizeof(RAWINPUTHEADER)
            ) != size) {
                throw WND_LAST_EXCEPT();
            }

            auto& ri = reinterpret_cast<const RAWINPUT&>(*sRawInputBuffer.data());
            if (ri.header.dwType == RIM_TYPEMOUSE) {
                MyMouseMsgAPI::onRaw(*pMouse_, Mouse::RawDelta{
                    ri.data.mouse.lLastX, ri.data.mouse.lLastY
                } );
            }
            return 0;
        }

        default:
            break;
        }

        return {};
    }
private:
    static Mouse::Point makePoint(LPARAM lParam) {
        return Mouse::Point{
            static_cast<short>(
                (static_cast<unsigned int>(lParam)) & 0xffff
            ),
            static_cast<short>(
                (static_cast<unsigned int>(lParam) >> 16) & 0xffff
            )
        };
    }

    bool insideClient(Mouse::Point pt) NOEXCEPT {
        auto client = window().client();

        auto cx = static_cast<decltype(pt.x)>(client.x);
        auto cy = static_cast<decltype(pt.y)>(client.y);
        auto cw = static_cast<decltype(pt.x)>(client.width);
        auto ch = static_cast<decltype(pt.y)>(client.height);

        return pt.x >= cx && pt.y >= cy
            && pt.x <= cx + cw
            && pt.y <= cy + ch;
    }

    MyMouse* pMouse_;
    bool cursorEnabled_;
};

}   // namespace ic::Win32

}   // namespace ic

#endif // __MouseWin32Adaptor_HPP