#include "game.hpp"

#include "player.hpp"

#include "mouseWin32Adaptor.hpp"
#include "testInput.hpp"

#include <chrono>
#include <thread>

using namespace std::literals;

Game::Game()
    : core_(), mouse_(), keyboard_(), systems_(keyboard_),
    timer_(), pStage_(), lockFPS_(defLockFPS) {
    setupWndMsgHandlers();
}

int Game::run() {
    try {

    pStage_ = std::make_unique<Stage>(core_, systems_);

    for(;;) {
        if (auto returnCode = core_.window().processMessages()) {
            return returnCode.value();
        }

        update();
        render();
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

void Game::update() {
    timer_.update();

    const auto expectedFrameTime = 1. / lockFPS_;
    const auto restFrameTime = expectedFrameTime - timer_.GetDT();

    if (restFrameTime > 0.) {
        std::this_thread::sleep_for( std::chrono::duration<double>(restFrameTime) );
    }

    // TODO: update stage
    pStage_->update(timer_.GetDT());
}

void Game::render() {
    // TODO: render stage
    pStage_->render(core_);
}

void Game::setupWndMsgHandlers() {
    // core_.window().addMsgHandler(0, std::make_unique<TestInputHandler<MyWindow>>(*pWnd_, *pMouse_, *pGfx_));
    // core_.window().addMsgHandler(1, std::make_unique<ic::Win32::MouseMsgHandler<MyWindow>>(*pWnd_, pMouse_));
    core_.window().addMsgHandler( 10000, std::make_unique<
        Win32::BasicMsgHandler<gfx::d3d12engine::Core::MyWindow>
    >(core_.window()) );
}