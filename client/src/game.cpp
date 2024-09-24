#include "game.hpp"

#include "player.hpp"
#include "cmodel.hpp"

#include "mouseWin32Adaptor.hpp"
#include "testInput.hpp"

#include "d3d12scene.hpp"

#include "mygfx.hpp"

#include <chrono>
#include <thread>

using namespace std::literals;

Game::Game(gfx::ICore& gfx, MyWindow& wnd, ic::Mouse& mouse, ic::Keyboard& keyboard)
    : systems_( static_cast<gfx::d3d12::Core&>(gfx), keyboard ),
    timer_(), pStage_(), pGfx_(&gfx), pWnd_(&wnd),
    pMouse_(&mouse), renderFunc_(&Game::initialRender), lockFPS_(defLockFPS),
    curFenceIdx_(0), prevFenceIdx_(1u) {
    setupWndMsgHandlers();
    ecs::init( ecs::InitDesc{ .threadCnt = 1u, .entityPoolSize = 0x100u } );
    pStage_ = std::make_unique<Stage>( static_cast<gfx::d3d12::Core&>(gfx), wnd.client(), systems_ );
}

void Game::update() {
    timer_.update();

    const auto expectedFrameTime = 1. / lockFPS_;
    const auto restFrameTime = expectedFrameTime - timer_.GetDT();

    if (restFrameTime > 0.) {
        std::this_thread::sleep_for( std::chrono::duration<double>(restFrameTime) );
    }

    pWnd_->setTitle(timer_.str());

    pStage_->update(timer_.GetDT());
}

void Game::render() {
    (this->*renderFunc_)();
}

void Game::initialRender() {
    const auto expectedFrameTime = 1. / lockFPS_;

    // skip rendering if the frame time is too long
    /*if (timer_.GetDT() > expectedFrameTime * 2.) {
        return;
    }*/

    static_cast<MyGfx&>(*pGfx_).setFrame(curFenceIdx_);

    pStage_->render(*pGfx_, *pWnd_);

    auto pd3d12Gfx_ = static_cast<gfx::d3d12::Core*>(pGfx_);
    pd3d12Gfx_->signalGpu(curFenceIdx_);
    std::swap(curFenceIdx_, prevFenceIdx_);

    renderFunc_ = &Game::regularRender;
}

void Game::regularRender() {
    const auto expectedFrameTime = 1. / lockFPS_;

    // skip rendering if the frame time is too long
    /*if (timer_.GetDT() > expectedFrameTime * 2.) {
        return;
    }*/

    static_cast<MyGfx&>(*pGfx_).setFrame(curFenceIdx_);

    pStage_->render(*pGfx_, *pWnd_);

    auto pd3d12Gfx_ = static_cast<gfx::d3d12::Core*>(pGfx_);
    pd3d12Gfx_->signalGpu(curFenceIdx_);
    pd3d12Gfx_->waitGpu(prevFenceIdx_);

    pWnd_->present();

    std::swap(curFenceIdx_, prevFenceIdx_);
}

void Game::setupWndMsgHandlers() {
    pWnd_->addMsgHandler(0, std::make_unique<TestInputHandler<MyWindow>>(*pWnd_, *pMouse_, *pGfx_));
    pWnd_->addMsgHandler(1, std::make_unique<ic::Win32::MouseMsgHandler<MyWindow>>(*pWnd_, pMouse_));
}