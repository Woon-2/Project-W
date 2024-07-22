#include "app.hpp"

App::App(HINSTANCE hInstance)
    : window_() {
    MyWindow::setHInst(hInstance);
}

int App::run() {
    window_.open();
    window_.setTitle("project-W client");
    window_.show(SW_SHOW);

    window_.addMsgHandler(10000, std::make_unique< Win32::BasicMsgHandler<MyWindow> >(window_));

    for(;;) {
        if (auto returnCode = window_.processMessages()) {
            return returnCode.value();
        }
    }
}