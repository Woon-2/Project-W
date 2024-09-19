#include "game.hpp"

#include "mouseWin32Adaptor.hpp"
#include "testInput.hpp"

#include "d3d12scene.hpp"

#include "mygfx.hpp"

#include "d3d12inputLayoutPresets.hpp"

#include "resourcePath.hpp"

#include <chrono>
#include <thread>

#include "phongShader.hpp"

using namespace std::literals;

Game::Game(gfx::ICore& gfx, MyWindow& wnd, ic::Mouse& mouse, ic::Keyboard& keyboard)
    : timer_(), baseCoordSys_(), camera_(gfx::Camera::Config{
        .fov = 90.f, .aspect = wnd.client().width / static_cast<float>(wnd.client().height),
        .near = 0.1f, .far = 1000.f
    }), /*inputSystem_(keyboard),*/ physicsSystem_(keyboard), pGfx_(&gfx), pWnd_(&wnd),
    pMouse_(&mouse), renderFunc_(&Game::initialRender), lockFPS_(defLockFPS), player_(),
    curFenceIdx_(0), prevFenceIdx_(1u) {
    setupWndMsgHandlers();
    loadAssets();
    setupCamera();
    initECS();
    initLights();

    player_.Init();
}

void Game::update() {
    processNetwork();
    processInput();
    timer_.update();

    const auto expectedFrameTime = 1. / lockFPS_;
    const auto restFrameTime = expectedFrameTime - timer_.GetDT();

    if (restFrameTime > 0.) {
        std::this_thread::sleep_for( std::chrono::duration<double>(restFrameTime) );
    }

    pWnd_->setTitle(timer_.str());

    camera_.updateView();

    auto& playerRigidBody =  ecs::GetComponent<Rigidbody>(player_.entityNumber_);
    const auto playerOldPos = ecs::GetComponent<Position>(player_.entityNumber_);
    const auto playerDeltaPos = playerRigidBody.deltaPosition();

    auto& playerPos = ecs::GetComponent<Position>(player_.entityNumber_);
	playerPos.x = playerOldPos.x + playerDeltaPos.x();
	playerPos.y = playerOldPos.y + playerDeltaPos.y();
	playerPos.z = playerOldPos.z + playerDeltaPos.z();

    baseCoordSys_.traverse();
    
    player_.printPos();
}

void Game::render() {
    (this->*renderFunc_)();
}

void Game::initialRender() {
    const auto expectedFrameTime = 1. / lockFPS_;

    // skip rendering if the frame time is too long
    if (timer_.GetDT() > expectedFrameTime * 2.) {
        return;
    }

    static_cast<gfx::d3d12::PhongShader&>( 
        static_cast<MyGfx&>(*pGfx_).shader( gfx::d3d12::PhongShader::shaderName() )
    ).setFrame(curFenceIdx_);

    auto pRenderContext = pGfx_->createContext();
    pGfx_->preRender();
    pRenderContext->preRender();
    pWnd_->preRender(*pRenderContext);
    pWnd_->clear(*pRenderContext);

    // TODO: camera_->makeScene(world);
    auto scene = gfx::d3d12::CameraScene(camera_);

    scene.addMaterial(&Gun::sMaterial);

    for (const auto& light : lights_) {
        scene.addLight(&light);
    }
    
     pGfx_->render( *pRenderContext, scene, static_cast<MyGfx&>(*pGfx_).renderer(
         MyGfx::Renderer::Phong
     ), *pWnd_ );

    pWnd_->postRender(*pRenderContext);
    pRenderContext->postRender();
    pGfx_->postRender();

    auto pd3d12Gfx_ = static_cast<gfx::d3d12::Core*>(pGfx_);
    pd3d12Gfx_->signalGpu(curFenceIdx_);
    std::swap(curFenceIdx_, prevFenceIdx_);

    renderFunc_ = &Game::regularRender;
}

void Game::regularRender() {
    const auto expectedFrameTime = 1. / lockFPS_;

    // skip rendering if the frame time is too long
    if (timer_.GetDT() > expectedFrameTime * 2.) {
        return;
    }

    static_cast<gfx::d3d12::PhongShader&>( 
        static_cast<MyGfx&>(*pGfx_).shader( gfx::d3d12::PhongShader::shaderName() )
    ).setFrame(curFenceIdx_);

    auto pRenderContext = pGfx_->createContext();
    pGfx_->preRender();
    pRenderContext->preRender();
    pWnd_->preRender(*pRenderContext);
    pWnd_->clear(*pRenderContext);

    // TODO: camera_->makeScene(world);
    auto scene = gfx::d3d12::CameraScene(camera_);

    scene.addMaterial(&Gun::sMaterial);

    for (const auto& light : lights_) {
        scene.addLight(&light);
    }
    
     pGfx_->render( *pRenderContext, scene, static_cast<MyGfx&>(*pGfx_).renderer(
         MyGfx::Renderer::Phong
     ), *pWnd_ );

    pWnd_->postRender(*pRenderContext);
    pRenderContext->postRender();
    pGfx_->postRender();

    auto pd3d12Gfx_ = static_cast<gfx::d3d12::Core*>(pGfx_);
    pd3d12Gfx_->signalGpu(curFenceIdx_);
    pd3d12Gfx_->waitGpu(prevFenceIdx_);

    pWnd_->present();

    std::swap(curFenceIdx_, prevFenceIdx_);
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
    
    // inputSystem_.update();
    physicsSystem_.update(static_cast<float>(timer_.GetDT()));
}

void Game::setupWndMsgHandlers() {
    pWnd_->addMsgHandler(0, std::make_unique<TestInputHandler<MyWindow>>(*pWnd_, *pMouse_, *pGfx_));
    pWnd_->addMsgHandler(1, std::make_unique<ic::Win32::MouseMsgHandler<MyWindow>>(*pWnd_, pMouse_));
}

void Game::loadAssets() {
    auto pd3d12Gfx = static_cast<gfx::d3d12::Core*>(pGfx_);
    Gun::loadAssets(*pd3d12Gfx, curFenceIdx_);
}

void Game::setupCamera() {
    camera_.coordSys().setParent(&baseCoordSys_);
    camera_.coordSys() << mu::translate(0.f, 1.f, -10.f);
    camera_.focus( gfx::coord::Pt3( &baseCoordSys_, mu::Vec3(0.f, 0.f, 0.f) ) );
}

void Game::initECS() {
    ecs::ConfigEntity();
    ecs::RegisterComponent<Position>();
    ecs::RegisterComponent<PlayerController>();
    ecs::RegisterComponent<Rigidbody>();

    // auto inputSystem_ = ecs::RegisterSystem<InputSystem>();
    auto physicsSystem_ = ecs::RegisterSystem<PhysicsSystem>();

    //{
    //    ecs::Signature signature;
    //    signature.set(ecs::GetComponentType<PlayerController>());
    //    signature.set(ecs::GetComponentType<Position>());
    //    ecs::SetSystemSignature<InputSystem>(signature);
    //}

    {
        ecs::Signature signature;
        signature.set(ecs::GetComponentType<Position>());
        signature.set(ecs::GetComponentType<PlayerController>());
        signature.set(ecs::GetComponentType<Rigidbody>());
        ecs::SetSystemSignature<PhysicsSystem>(signature);
    }

    
}

void Game::processNetwork() {
    // TODO: implement network
}

void Game::initLights() {
    lights_.push_back( gfx::d3d12::sr::PhongLight{
        .ambient = dx::XMFLOAT4{ 0.51f, 0.54f, 0.57f, 1.f },
        .diffuse = dx::XMFLOAT4{ 0.54f, 0.56f, 0.58f, 1.f },
        .specular = dx::XMFLOAT4{ 0.25f, 0.25f, 0.25f, 1.f },
        .falloff = 1.f,
        .dirV = dx::XMFLOAT3{ -0.1f, -0.6f, 0.4f },
        .type = gfx::d3d12::sr::PhongLight::kTypeDirectional
    } );

    lights_.push_back( gfx::d3d12::sr::PhongLight{
        .ambient = dx::XMFLOAT4{ 0.f, 0.f, 0.f, 1.f },
        .diffuse = dx::XMFLOAT4{ 0.15f, 0.3f, 0.65f, 1.f },
        .specular = dx::XMFLOAT4{ 0.1f, 0.15f, 0.2f, 0.f },
        .posV = dx::XMFLOAT3{ -50.f, 20.f, -5.f },
        .falloff = 8.f,
        .dirV = dx::XMFLOAT3{ 0.f, 0.f, 1.f },
        .cosTheta = std::cos( static_cast<float>( mu::Radian( mu::Degree(25.f) ) ) ),
        .atten = dx::XMFLOAT3{ 1.f, 0.045f, 0.0075f },
        .cosPhi = std::cos( static_cast<float>( mu::Radian( mu::Degree(60.f) ) ) ),
        .type = gfx::d3d12::sr::PhongLight::kTypeSpot
    } );
}