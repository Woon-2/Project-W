#include "stage.hpp"

#include "assetMap.hpp"
#include "cmodel.hpp"

#include <ranges>

void Stage::update(double deltaTime) {
    processNetwork(deltaTime);
    processInput(deltaTime);
    simulate(deltaTime);
}

void Stage::render(gfx::ICore& core, gfx::IRenderTarget& target) {
    auto pRenderContext = core.createContext();
    core.preRender();
    pRenderContext->preRender();
    target.preRender(*pRenderContext);
    target.clear(*pRenderContext);

    // auto scene = gfx::d3d12::CameraScene(camera_);

    // scene.addMaterial(&Gun::sMaterial);

    // for (const auto& light : lights_) {
    //     scene.addLight(&light);
    // }
    
    //  pGfx_->render( *pRenderContext, scene, static_cast<MyGfx&>(*pGfx_).renderer(
    //      MyGfx::Renderer::Phong
    //  ), *pWnd_ );

    target.postRender(*pRenderContext);
    pRenderContext->postRender();
    core.postRender();
}

void Stage::processInput(double deltaTime) {
    // inputSystem_.update();
    // pSystems_->physicsSystem.update(static_cast<float>(deltaTime));
}

void Stage::initLights() {
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

void Stage::setupCamera() {
    auto& world = pSystems_->coordRoot.get();

    camera_.coordSys().setParent(&world);
    camera_.coordSys() << mu::translate(0.f, 1.f, -10.f);
    camera_.focus( gfx::coord::Pt3( &world, mu::Vec3(0.f, 0.f, 0.f) ) );
}