#include "game.hpp"

#include "mygfx.hpp"

Game::Game(gfx::ICore& gfx, MyWindow& wnd)
    : pGfx_(&gfx), pWnd_(&wnd), pDrawable_(), player_(), keyManager_(5) {
    auto pd3d12Gfx = static_cast<gfx::d3d12::Core*>(&gfx);
    auto pCtx = pGfx_->createContext();

    pGfx_->preRender();
    pDrawable_ = std::make_unique<gfx::SampleDrawable>(*pd3d12Gfx, static_cast<gfx::d3d12::D3D12RenderContext&>(*pCtx));
    pGfx_->postRender();
    pd3d12Gfx->waitForGpu();
    
    pDrawable_->completeInit(*pd3d12Gfx);
    
    inputSystem_.init(keyboard_);

    ecs::ConfigEntity();
    ecs::RegisterComponent<PlayerController>();
    ecs::RegisterComponent<Position>();

    auto inputSystem_ =  ecs::RegisterSystem<InputSystem>();

    ecs::Signature signature;
    signature.set(ecs::GetComponentType<PlayerController>());
    signature.set(ecs::GetComponentType<Position>());
    ecs::SetSystemSignature<InputSystem>(signature);

    player_.Init();
}

void Game::update() {
    processInput();
    
    inputSystem_.update();
    player_.printPos();
}

void Game::render() {
    auto pRenderContext = pGfx_->createContext();
    pGfx_->preRender();
    pWnd_->preRender(*pRenderContext);
    pWnd_->clear(*pRenderContext);

    auto scene = gfx::SampleScene(*pDrawable_);
    pGfx_->render( scene, static_cast<MyGfx&>(*pGfx_).renderer(
        MyGfx::Renderer::Sample
    ), *pWnd_ );
    pWnd_->postRender(*pRenderContext);
    pGfx_->postRender();

    auto pd3d12Gfx_ = static_cast<gfx::d3d12::Core*>(pGfx_);
    pd3d12Gfx_->waitForGpu();
    pWnd_->present();
    pd3d12Gfx_->alterFence();
    pd3d12Gfx_->waitForGpu();
}

void Game::processInput() {
    keyboard_.patchKeyState();
    keyManager_.updateFrame(keyboard_);
}