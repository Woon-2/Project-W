#ifndef __MOUSE_HANDLER_HPP
#define __MOUSE_HANDLER_HPP

#include "Window.hpp"
#include "mouse.hpp"

#include <sstream>

template <class Wnd>
class MouseMsgHandler : public Win32::MsgHandler<Wnd>{
public:
    using Win32::MsgHandler<Wnd>::window;
    using MyWindow = Wnd;
    using MyChar = typename MyWindow::MyChar;
    using MyMouse = ic::Mouse;
    using MyMouseMsgAPI = ic::MouseMsgAPI;
    using MyString = std::basic_string<MyChar>;

    MouseMsgHandler(MyWindow& wnd, MyMouse* pMouse) NOEXCEPT
        : Win32::MsgHandler<MyWindow>(wnd), pMouse_(pMouse) {}

    std::optional<LRESULT> operator()(
        const Win32::Message& msg
    ) override {
        while (!pMouse_->empty()) {
            const auto e = pMouse_->read();
            std::ostringstream oss;

            switch(e->type().value()) {
            case ic::Mouse::Event::Type::Move:
                oss << "Mouse Position: (" << e->pos().x << ", " << e->pos().y << ")";
                window().setTitle(oss.str());
                break;

            case ic::Mouse::Event::Type::LPress:
                window().setTitle("LPress");
                break;

            case ic::Mouse::Event::Type::LRelease:
                window().setTitle("LRelease");
                break;

            case ic::Mouse::Event::Type::MPress:
                window().setTitle("MPress");
                break;

            case ic::Mouse::Event::Type::MRelease:
                window().setTitle("MRelease");
                break;

            case ic::Mouse::Event::Type::RPress:
                window().setTitle("RPress");
                break;

            case ic::Mouse::Event::Type::RRelease:
                window().setTitle("RRelease");
                break;

            case ic::Mouse::Event::Type::WheelUp:
                window().setTitle("WheelUp");
                break;

            case ic::Mouse::Event::Type::WheelDown:
                window().setTitle("WheelDown");
                break;

            default:
                break;
            }
        }

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

        default:
            break;
        }

        return {};
    }
private:
    static ic::Mouse::Point makePoint(LPARAM lParam) {
        return ic::Mouse::Point{
            static_cast<short>(
                (static_cast<unsigned int>(lParam)) & 0xffff
            ),
            static_cast<short>(
                (static_cast<unsigned int>(lParam) >> 16) & 0xffff
            )
        };
    }

    bool insideClient(ic::Mouse::Point pt) NOEXCEPT {
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
};

#endif // __MOUSE_HANDLER_HPP