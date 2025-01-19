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
    player_.addCamera(mu::Vec3(0.f, 2.8f, -6.5f), 1.f, pSystems_->coordRoot);
    player_.as<gfx::d3d12engine::Coord>().get() << mu::translate(0.f, 2.5f, 0.f);

    /*
    const auto lightPos = mu::Vec3( pCamera_->repPos() - pLight_->dir * 800.f );
    const auto lightDir = pLight_->dir;

    const auto lightView = mu::lookAt( lightPos, lightPos + lightDir, mu::Vec3( 0.f, 1.f, 0.f ) );
    const auto lightProj = mu::ortho( -10.f, 10.f, -10.f, 10.f, 600.f, 1200.f );
    */

    directionalLight_.init(gfx::d3d12::WorldLight{
        .config = gfx::d3d12::WorldLight::Config{
            .ortho = {
                .width = 20.f,
                .height = 20.f,
                .nearZ = 600.f,
                .farZ = 1200.f
            }
        },
        .color = mu::Vec3(1.f, 1.f, 1.f),
        .dir = mu::NVec3(0.f, -1.f, 1.f),
        .intensity = 5.0f,
        .distanceToCamera = 800.f,
        .type = gfx::d3d12::sr::Light::Type::Directional
    });

    scene_.addEntity(player_);
    for (auto& subsetRows : terrain_.subsets()) {
        for (auto& subset : subsetRows) {
            scene_.addEntity(subset);
        }
    }
    scene_.addEntity(directionalLight_);

    pSystems_->coordRoot.addEntity(player_);
    pSystems_->coordRoot.addEntity(terrain_);
    pSystems_->inputSystem.addEntity(player_);
    pSystems_->physicsSystem.addEntity(player_);

    scene_.clearStash();
}

void Stage::loadAssets(gfx::d3d12engine::Core& core) {
    auto cmdList = core.fetchCmdList();

    core.prepareGPUResLoad();

    loadTextures(core, cmdList);
    loadModels(core, cmdList);
    loadTerrains(core, cmdList);

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
    renderer.layoutVBsPBR(core, modelInfo.id, 1);
}

void Stage::loadModels(gfx::d3d12engine::Core& core, gfx::d3d12::D3D12GfxCmdList& cmdList) {
    loadModel(core, AssetModel::Helicopter, *pRenderer_);
}

void Stage::loadTerrains(gfx::d3d12engine::Core& core, gfx::d3d12::D3D12GfxCmdList& cmdList) {
    terrain_.init(
        "Terrain",
        resourcePath/"terrains/Height-Map.png",
        resourcePath/"terrains/Diffuse-Map.dds",
        mu::Vec3(0.05f, 200.f, 0.05f),
        core, mu::Vec3(0.f, -7.5f, 0.f)
    );
    for (const auto& subsetRows : terrain_.subsets()) {
        for (const auto& subset : subsetRows) {
            pRenderer_->layoutVBsPBRMacro(core, subset.refModelKey(), 1);
        }
    }
}