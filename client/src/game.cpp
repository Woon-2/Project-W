#include "game.hpp"

#include "d3d12scene.hpp"

#include "mygfx.hpp"

#include "d3d12inputLayoutPresets.hpp"

#include "resourcePath.hpp"

#include <chrono>
#include <thread>


Game::Game(gfx::ICore& gfx, MyWindow& wnd)
    : timer_(), baseCoordSys_(), camera_(baseCoordSys_, gfx::Camera::Config{
        .fov = 90.f, .aspect = wnd.client().width / static_cast<float>(wnd.client().height),
        .near = 0.1f, .far = 1000.f
    }), pGfx_(&gfx), pWnd_(&wnd), pDrawable_(), lockFPS_(defLockFPS) {
    auto pd3d12Gfx = static_cast<gfx::d3d12::Core*>(&gfx);
    auto pCtx = pGfx_->createContext();
    auto pd3d12Ctx = static_cast<gfx::d3d12::D3D12RenderContext*>(pCtx.get());

    pGfx_->preRender();
    pDrawable_ = std::make_unique<gfx::d3d12::Model>( *pd3d12Gfx, *pd3d12Ctx,
        gfx::loadModel( resourcePath/"models"/"Gun _obj"/"Gun.obj",
            pd3d12Gfx->inputLayout( gfx::d3d12::inputLayoutName(gfx::InputLayoutPreset::Pos3) )
        ), "house_vb", "house_ib"
    );
    pGfx_->postRender();
    pd3d12Gfx->waitForGpu();
    
    pDrawable_->completeInit(*pd3d12Gfx);

    camera_.coordSys() << mu::translate(0.f, 2.f, -5.f);
    camera_.focus( gfx::coord::Pt3( baseCoordSys_, mu::Vec3(0.f, 0.f, 0.f) ) );
}

void Game::update() {
    processInput();
    timer_.update();

    const auto expectedFrameTime = 1. / lockFPS_;
    const auto restFrameTime = expectedFrameTime - timer_.GetDT();

    if (restFrameTime > 0.) {
        std::this_thread::sleep_for( std::chrono::duration<double>(restFrameTime) );
    }

    pWnd_->setTitle(timer_.str());

    camera_.updateView();
}

void Game::render() {
    const auto expectedFrameTime = 1. / lockFPS_;

    // skip rendering if the frame time is too long
    if (timer_.GetDT() > expectedFrameTime * 2.) {
        return;
    }

    auto pRenderContext = pGfx_->createContext();
    pGfx_->preRender();
    pWnd_->preRender(*pRenderContext);
    pWnd_->clear(*pRenderContext);

    // TODO: camera_->makeScene(world);
    auto scene = gfx::d3d12::CameraScene(camera_);
    scene.addModel( *pDrawable_ );
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

}