#include "stage.hpp"

#include "assetMap.hpp"

#include <ranges>

void Stage::init(gfx::d3d12engine::Core& core) {
    loadAssets(core);

    initEntities(core);
    initLights();

    pSystems_->coordRoot.update();
    pRenderer_->init(scene_);
}

void Stage::update(double deltaTime) {
    processNetwork(deltaTime);
    processInput(deltaTime);
    simulate(deltaTime);
}

void Stage::render(gfx::d3d12engine::Core& core) {
    core.render(*pRenderer_, scene_);
    scene_.clearStash();
}

void Stage::processNetwork(double deltaTime) {
    // networkSystem_.update();
}

void Stage::processInput(double deltaTime) {
    pSystems_->inputSystem.update(static_cast<float>(deltaTime));
}

void Stage::simulate(double deltaTime) {
    pSystems_->physicsSystem.update(static_cast<float>(deltaTime));
    player_.update(static_cast<float>(deltaTime));
    pSystems_->coordRoot.update();

    player_.postUpdate();
}

void Stage::initEntities(gfx::d3d12engine::Core& core) {
    player_.init(core);
    player_.addCamera(mu::Vec3(0.f, 100.f, -100.f), 1.f, pSystems_->coordRoot);

    scene_.addEntity(player_);
    pSystems_->coordRoot.addEntity(player_);
    pSystems_->inputSystem.addEntity(player_);
    pSystems_->physicsSystem.addEntity(player_);

    scene_.clearStash();
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

void Stage::loadAssets(gfx::d3d12engine::Core& core) {
    core.prepareGPUResLoad();

    loadTextures(core);
    loadModels(core);

    core.finishGPUResLoad();
}

void loadTexture(gfx::d3d12engine::Core& core, AssetTexture key) {
    for (const auto& texInfo : assetTextureInfo(key)) {
        for (const auto& path : texInfo.paths) {
            core.loadStaticTexture(path, texInfo.type);
        }
    }
}

void Stage::loadTextures(gfx::d3d12engine::Core& core) {
    loadTexture(core, AssetTexture::Helicopter);
}

void loadModel(gfx::d3d12engine::Core& core, AssetModel key, Renderer& renderer) {
    auto modelInfo = assetModelInfo(key);
    core.loadRefModel(modelInfo.path, modelInfo.id);
    renderer.layoutVBs(core, modelInfo.id, 1);
}

void Stage::loadModels(gfx::d3d12engine::Core& core) {
    loadModel(core, AssetModel::Helicopter, *pRenderer_);
}