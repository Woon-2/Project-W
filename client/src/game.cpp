#include "game.hpp"

#include "mouseWin32Adaptor.hpp"
#include "testInput.hpp"

#include "d3d12scene.hpp"

#include "mygfx.hpp"

#include "d3d12inputLayoutPresets.hpp"

#include "resourcePath.hpp"

#include <chrono>
#include <thread>


Game::Game(gfx::ICore& gfx, MyWindow& wnd, ic::Mouse& mouse)
    : timer_(), baseCoordSys_(), camera_(gfx::Camera::Config{
        .fov = 90.f, .aspect = wnd.client().width / static_cast<float>(wnd.client().height),
        .near = 0.1f, .far = 1000.f
    }), pGfx_(&gfx), pWnd_(&wnd), pMouse_(&mouse), models_(), lockFPS_(defLockFPS) {

    pWnd_->addMsgHandler(0, std::make_unique<ic::Win32::MouseMsgHandler<MyWindow>>(*pWnd_, pMouse_));
    pWnd_->addMsgHandler(1, std::make_unique<TestInputHandler<MyWindow>>(*pWnd_, *pMouse_, *pGfx_));

    auto pd3d12Gfx = static_cast<gfx::d3d12::Core*>(&gfx);
    auto pCtx = pGfx_->createContext();
    auto pd3d12Ctx = static_cast<gfx::d3d12::D3D12RenderContext*>(pCtx.get());

    pGfx_->preRender();

    auto mod = gfx::loadModel( resourcePath / "models" / "AC Cobra" / "Shelby.fbx",
        pd3d12Gfx->inputLayout(gfx::d3d12::inputLayoutName(gfx::InputLayoutPreset::Pos3))
    );

    auto car = gfx::d3d12::Model( *pd3d12Gfx, *pd3d12Ctx, mod, "car_vb", "car_ib" );

    car.coord().setParent(&baseCoordSys_);
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

    camera_.coordSys().setParent(&baseCoordSys_);
    camera_.coordSys() << mu::translate(0.f, 100.f, -1000.f);
    camera_.focus( gfx::coord::Pt3( &baseCoordSys_, mu::Vec3(0.f, 0.f, 0.f) ) );
}

void Game::update() {
    processInput();
    timer_.update();

    const auto expectedFrameTime = 1. / lockFPS_;
    const auto restFrameTime = expectedFrameTime - timer_.GetDT();

    if (restFrameTime > 0.) {
        std::this_thread::sleep_for( std::chrono::duration<double>(restFrameTime) );
    }

    // pWnd_->setTitle(timer_.str());

    camera_.updateView();
    baseCoordSys_.traverse();
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
    while (!pMouse_->empty()) {
        const auto e = pMouse_->read();
        std::ostringstream oss;

        switch(e->type().value()) {
        case ic::Mouse::Event::Type::Move:
            oss << "Mouse Position: (" << e->pos().x << ", " << e->pos().y << ")";
            pWnd_->setTitle(oss.str());
            break;

        case ic::Mouse::Event::Type::LPress:
            pWnd_->setTitle("LPress");
            break;

        case ic::Mouse::Event::Type::LRelease:
            pWnd_->setTitle("LRelease");
            break;

        case ic::Mouse::Event::Type::MPress:
            pWnd_->setTitle("MPress");
            break;

        case ic::Mouse::Event::Type::MRelease:
            pWnd_->setTitle("MRelease");
            break;

        case ic::Mouse::Event::Type::RPress:
            pWnd_->setTitle("RPress");
            break;

        case ic::Mouse::Event::Type::RRelease:
            pWnd_->setTitle("RRelease");
            break;

        case ic::Mouse::Event::Type::WheelUp:
            pWnd_->setTitle("WheelUp");
            break;

        case ic::Mouse::Event::Type::WheelDown:
            pWnd_->setTitle("WheelDown");
            break;

        default:
            break;
        }
    }
}