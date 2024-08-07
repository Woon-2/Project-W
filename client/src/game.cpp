#include "game.hpp"

Game::Game(gfx::ICore& gfx, MyWindow& wnd)
    : pGfx_(&gfx), pWnd_(&wnd), pRenderer_( std::make_unique<gfx::SampleRenderer>() ),
    pDrawable_() {
    pRenderer_->init();
    auto pCtx = pGfx_->createContext();

    pGfx_->preRender();

    auto pd3d12Gfx = static_cast<gfx::d3d12::Core*>(&gfx);
    pDrawable_ = std::make_unique<gfx::SampleDrawable>(*pd3d12Gfx, static_cast<gfx::d3d12::D3D12RenderContext&>(*pCtx));
    pGfx_->postRender();

    pd3d12Gfx->waitForGpu();
    
    pDrawable_->completeInit(*pd3d12Gfx);
}

void Game::update() {
    processInput();
}

void Game::render() {
    auto pRenderContext = pGfx_->createContext();
    pGfx_->preRender();
    pWnd_->preRender(*pRenderContext);
    pWnd_->clear(*pRenderContext);

    auto scene = gfx::SampleScene(*pDrawable_);
    pGfx_->render(scene, *pRenderer_, *pWnd_);
    pWnd_->postRender(*pRenderContext);
    pGfx_->postRender();

    auto pd3d12Gfx_ = static_cast<gfx::d3d12::Core*>(pGfx_);
    pd3d12Gfx_->waitForGpu();
    pWnd_->present();
    pd3d12Gfx_->alterFence();
    pd3d12Gfx_->waitForGpu();
}

void Game::processInput() {

}