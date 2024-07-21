#include "app.hpp"

App::App(HINSTANCE hInstance)
    : windows_() {
    MyWindow::setHInst(hInstance);
}

int App::run() {
    windows_.emplace_back(std::make_unique<MyWindow>());
    auto& wnd = *windows_.back();
    wnd.setTitle("project-W client");
    wnd.show(SW_SHOW);

    wnd.addMsgHandler(10000, std::make_unique< Win32::BasicMsgHandler<MyWindow> >(wnd));

    for(;;) {
        if (auto returnCode = wnd.processMessages()) {
            return returnCode.value();
        }
    }
}