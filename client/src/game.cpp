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
    }), pGfx_(&gfx), pWnd_(&wnd), models_(), lockFPS_(defLockFPS) {
    auto pd3d12Gfx = static_cast<gfx::d3d12::Core*>(&gfx);
    auto pCtx = pGfx_->createContext();
    auto pd3d12Ctx = static_cast<gfx::d3d12::D3D12RenderContext*>(pCtx.get());

    pGfx_->preRender();
    
    auto car = gfx::d3d12::Model( *pd3d12Gfx, *pd3d12Ctx, gfx::loadModel( resourcePath/"models"/"AC Cobra"/"Shelby.obj",
        pd3d12Gfx->inputLayout( gfx::d3d12::inputLayoutName(gfx::InputLayoutPreset::Pos3) )
    ), "car_vb", "car_ib" );

    car.coord() << mu::rotateY(mu::Degree(90.f));

    models_.push_back(std::move(car));

    // models_.emplace_back( *pd3d12Gfx, *pd3d12Ctx,
    //     gfx::loadModel( resourcePath/"models"/"AC Cobra"/"Shelby.obj",
    //         pd3d12Gfx->inputLayout( gfx::d3d12::inputLayoutName(gfx::InputLayoutPreset::Pos3) )
    //     ), "car_vb", "car_ib"
    // );
    models_.emplace_back( *pd3d12Gfx, *pd3d12Ctx,
        gfx::loadModel( resourcePath/"models"/"box"/"box.obj",
            pd3d12Gfx->inputLayout( gfx::d3d12::inputLayoutName(gfx::InputLayoutPreset::Pos3) )
        ), "box_vb", "box_ib"
    );

    pGfx_->postRender();
    pd3d12Gfx->waitForGpu();

    for (auto& model : models_) {
        model.completeInit(*pd3d12Gfx);
    }

    camera_.coordSys() << mu::translate(0.f, 1.f, -15.f);
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
    for (auto& model : models_) {
        scene.addModel(model);
    }
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