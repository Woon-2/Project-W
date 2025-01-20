#include "stage.hpp"

#include "assetMap.hpp"
#include "resourcePath.hpp"

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
    pSystems_->inputSystem.update(static_cast<float>(deltaTime),
        ControllerAdapters{ .renderModeController = RenderModeController(pRenderer_) }
    );
}

void Stage::simulate(double deltaTime) {
    pSystems_->physicsSystem.update(static_cast<float>(deltaTime));
    player_.update(static_cast<float>(deltaTime));
    pSystems_->coordRoot.update();

    player_.postUpdate(deltaTime);
}

void Stage::initEntities(gfx::d3d12engine::Core& core) {
    player_.init(core);
    player_.addCamera(mu::Vec3(0.f, 4.8f, -10.f), 1.f, pSystems_->coordRoot);
    player_.as<gfx::d3d12engine::Coord>().get() << mu::translate(0.f, 2.5f, 0.f);

    directionalLight_.init(gfx::d3d12::WorldLight{
        .config = gfx::d3d12::WorldLight::Config{
            .ortho = {
                .width = 20.f,
                .height = 20.f,
                .nearZ = 600.f,
                .farZ = 1800.f
            }
        },
        .color = mu::Vec3(1.f, 1.f, 1.f),
        .dir = mu::NVec3(0.f, -1.f, 1.f),
        .intensity = 5.0f,
        .distanceToCamera = 800.f,
        .type = gfx::d3d12::sr::Light::Type::Directional
    });

    scene_.addEntity(player_);
    level_.activateChunk(1u, 1u, scene_);
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
    loadLevel(core, cmdList);

    core.finishGPUResLoad();
}

void loadTexture(gfx::d3d12engine::Core& core, AssetTexture key) {
    const auto& texInfo = assetTextureInfo(key);
    for (const auto& path : texInfo.paths) {
        core.loadStaticTexture(path, texInfo.type);
    }
}

void Stage::loadTextures(gfx::d3d12engine::Core& core, gfx::d3d12::D3D12GfxCmdList& cmdList) {
    loadTexture(core, AssetTexture::Helicopter);
    loadTexture(core, AssetTexture::Tree0);
    loadTexture(core, AssetTexture::Tree1);
    loadTexture(core, AssetTexture::Tree2);
    loadTexture(core, AssetTexture::Terrain);
}

void loadModel(gfx::d3d12engine::Core& core, AssetModel key, Renderer& renderer) {
    auto modelInfo = assetModelInfo(key);
    core.loadRefModel(modelInfo.path, modelInfo.id);
    renderer.layoutVBsPBR(core, modelInfo.id, 1);
}

void Stage::loadModels(gfx::d3d12engine::Core& core, gfx::d3d12::D3D12GfxCmdList& cmdList) {
    core.initChunkMesh(cmdList);
    loadModel(core, AssetModel::Helicopter, *pRenderer_);
    loadModel(core, AssetModel::Tree0, *pRenderer_);
    loadModel(core, AssetModel::Tree1, *pRenderer_);
    loadModel(core, AssetModel::Tree2, *pRenderer_);
}

void Stage::loadLevel(gfx::d3d12engine::Core& core, gfx::d3d12::D3D12GfxCmdList& cmdList) {
    level_ = gfx::d3d12engine::LevelRegion(core);
}