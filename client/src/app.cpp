#include "app.hpp"

App::App(HINSTANCE hInstance)
    : window_() {
    MyWindow::setHInst(hInstance);
}

App::~App() {
    gfx_->cleanup();
    gfx::DXFactory::cleanup();
}

int App::run() {
    try {

    gfx::DXInfoQueue::init();
    gfx::DXFactory::init();

    gfx::d3d12::Core::configRtvHeapSize(2);
    gfx::d3d12::Core::configDsvHeapSize(1);
    gfx::d3d12::Core::configDXFactory(gfx::DXFactory::get());

    gfx_ = std::make_unique<gfx::d3d12::Core>();

    gfx_->init();

    window_.open(static_cast<gfx::d3d12::Core&>(*gfx_));
    window_.setTitle("project-W client");
    window_.show(SW_SHOW);

    window_.addMsgHandler(10000, std::make_unique< Win32::BasicMsgHandler<MyWindow> >(window_));

    for(;;) {
        if (auto returnCode = window_.processMessages()) {
            return returnCode.value();
        }
    }

    } catch (const gfx::Exception& e) {
        MessageBoxA(nullptr, e.what(), "GFX Exception",
            MB_OK | MB_ICONEXCLAMATION);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Standard Exception",
            MB_OK | MB_ICONEXCLAMATION);
    } catch(...) {
        MessageBoxA(nullptr, "no details available",
            "Unknown Exception", MB_OK | MB_ICONEXCLAMATION);
    }

    return 0;
}