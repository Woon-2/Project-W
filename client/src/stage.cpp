#include "stage.hpp"

#include <ranges>

void Stage::init() {
    initEntities();
    initLights();

    setupCamera();
    // pSystems_->coordRoot.update();
}

void Stage::update(double deltaTime) {
    processNetwork(deltaTime);
    processInput(deltaTime);
    simulate(deltaTime);
}

void Stage::render(gfx::d3d12engine::Core& core) {
    
}

void Stage::processNetwork(double deltaTime) {
    // networkSystem_.update();
}

void Stage::processInput(double deltaTime) {
    // pSystems_->inputSystem.update(static_cast<float>(deltaTime));
}

void Stage::simulate(double deltaTime) {
    // pSystems_->physicsSystem.update(static_cast<float>(deltaTime));
    // player_.update();

    // // camera update
    // camera_.coordSys() << mu::translate(player_.as<RigidBody>().deltaPosition());
    // //

    // pSystems_->coordRoot.update();
    // camera_.unfocus();
    // camera_.focus( gfx::coord::Pt3( &player_.as<Coord>().get(), mu::Vec3(0.f, 0.f, 0.f) ) );
    // camera_.updateView();
}

void Stage::initEntities() {
    // pSystems_->assetSystem.addEntity(player_);
    // pSystems_->coordRoot.addEntity(player_);
    // pSystems_->inputSystem.addEntity(player_);
    // pSystems_->physicsSystem.addEntity(player_);

    // pSystems_->assetSystem.allocCtx();
    // pSystems_->assetSystem.loadAssets();
    // pSystems_->assetSystem.freeCtx();
    // player_.linkAssets(pSystems_->assetSystem);

    // pSystems_->fragmentizer.addEntity(player_);
}

void Stage::initLights() {
    // lights_.push_back( gfx::d3d12::sr::PhongLight{
    //     .ambient = dx::XMFLOAT4{ 0.51f, 0.54f, 0.57f, 1.f },
    //     .diffuse = dx::XMFLOAT4{ 0.54f, 0.56f, 0.58f, 1.f },
    //     .specular = dx::XMFLOAT4{ 0.25f, 0.25f, 0.25f, 1.f },
    //     .falloff = 1.f,
    //     .dirV = dx::XMFLOAT3{ -0.0f, -0.9f, -0.4f },
    //     .type = gfx::d3d12::sr::PhongLight::kTypeDirectional
    // } );

    // lights_.push_back( gfx::d3d12::sr::PhongLight{
    //     .ambient = dx::XMFLOAT4{ 0.f, 0.f, 0.f, 1.f },
    //     .diffuse = dx::XMFLOAT4{ 0.15f, 0.3f, 0.65f, 1.f },
    //     .specular = dx::XMFLOAT4{ 0.1f, 0.15f, 0.2f, 0.f },
    //     .posV = dx::XMFLOAT3{ -50.f, 20.f, -5.f },
    //     .falloff = 8.f,
    //     .dirV = dx::XMFLOAT3{ 0.f, 0.f, 1.f },
    //     .cosTheta = std::cos( static_cast<float>( mu::Radian( mu::Degree(25.f) ) ) ),
    //     .atten = dx::XMFLOAT3{ 1.f, 0.045f, 0.0075f },
    //     .cosPhi = std::cos( static_cast<float>( mu::Radian( mu::Degree(60.f) ) ) ),
    //     .type = gfx::d3d12::sr::PhongLight::kTypeSpot
    // } );
}

void Stage::setupCamera() {
    // auto& world = pSystems_->coordRoot.get();

    // camera_.coordSys().setParent(&world);
    // camera_.coordSys() << mu::translate(0.f, 120.f, -120.f);
    // camera_.focus( gfx::coord::Pt3( &world, mu::Vec3(0.f, 0.f, 0.f) ) );
}