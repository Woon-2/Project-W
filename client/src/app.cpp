#include "app.hpp"

#include "mygfx.hpp"

App::App(HINSTANCE hInstance)
    : window_() {
    MyWindow::setHInst(hInstance);
}

App::~App() {
    pGfx_->cleanup();
    gfx::DXFactory::cleanup();
}

int App::run() {
    try {

    gfx::DXInfoQueue::init();
    gfx::DXFactory::init();

    gfx::d3d12::Core::configRtvHeapSize(2);
    gfx::d3d12::Core::configDsvHeapSize(1);
    gfx::d3d12::Core::configDXFactory(gfx::DXFactory::get());

    pGfx_ = std::make_unique<MyGfx>();
    pGfx_->init();

    window_.open(static_cast<gfx::d3d12::Core&>(*pGfx_));
    window_.setTitle("project-W client");
    window_.show(SW_SHOW);

    window_.addMsgHandler(10000, std::make_unique< Win32::BasicMsgHandler<MyWindow> >(window_));

    pGame_ = std::make_unique<Game>(*pGfx_, window_);

    for(;;) {
        if (auto returnCode = window_.processMessages()) {
            return returnCode.value();
        }

        pGame_->update();
        pGame_->render();
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