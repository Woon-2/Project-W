#include "stage.hpp"

#include "assetMap.hpp"

#include <ranges>

void Stage::init(gfx::d3d12engine::Core& core) {
    loadAssets(core);

    initEntities(core);
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

    player_.postUpdate(deltaTime);
}

void Stage::initEntities(gfx::d3d12engine::Core& core) {
    player_.init(core);
    player_.addCamera(mu::Vec3(0.f, 2.8f, -6.5f), 1.f, pSystems_->coordRoot);

    directionalLight_.init(gfx::d3d12::sr::Light{
        .color = mu::Vec3(1.f, 1.f, 1.f).getXmf(),
        .dirV = mu::NVec3(0.f, -1.f, 1.f).getXmf(),
        .intensity = 25.f,
        .type = etoi(gfx::d3d12::sr::Light::Type::Directional)
    });

    scene_.addEntity(player_);
    scene_.addEntity(directionalLight_);

    pSystems_->coordRoot.addEntity(player_);
    pSystems_->inputSystem.addEntity(player_);
    pSystems_->physicsSystem.addEntity(player_);

    scene_.clearStash();
}

void Stage::loadAssets(gfx::d3d12engine::Core& core) {
    auto cmdList = core.fetchCmdList();

    core.prepareGPUResLoad();

    loadTextures(core, cmdList);
    loadModels(core, cmdList);

    core.finishGPUResLoad();
}

void loadTexture(gfx::d3d12engine::Core& core, AssetTexture key) {
    for (const auto& texInfo : assetTextureInfo(key)) {
        for (const auto& path : texInfo.paths) {
            core.loadStaticTexture(path, texInfo.type);
        }
    }
}

void Stage::loadTextures(gfx::d3d12engine::Core& core, gfx::d3d12::D3D12GfxCmdList& cmdList) {
    loadTexture(core, AssetTexture::Helicopter);
}

void loadModel(gfx::d3d12engine::Core& core, AssetModel key, Renderer& renderer) {
    auto modelInfo = assetModelInfo(key);
    core.loadRefModel(modelInfo.path, modelInfo.id);
    renderer.layoutVBs(core, modelInfo.id, 1);
}

void Stage::loadModels(gfx::d3d12engine::Core& core, gfx::d3d12::D3D12GfxCmdList& cmdList) {
    loadModel(core, AssetModel::Helicopter, *pRenderer_);
}