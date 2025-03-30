#include "stage.hpp"

#include "assetMap.hpp"
#include "resourcePath.hpp"

#include <ranges>
#include <algorithm>

void Stage::init() {
    loadAssets();

    initEntities();
    pSystems_->coordRoot.update();
}

void Stage::update(double deltaTime) {
    processPackets(deltaTime);
    processInput(deltaTime);
    simulate(deltaTime);
    updateNetwork(deltaTime);
}

void Stage::render() {
    if (pPlayer_) {
        pCore_->render(*pRenderer_, scene_);
        scene_.clearStash();
    }
}

void Stage::processPackets(double deltaTime) {
    pSession_->recvPackets();

    bool alreadyInitialized = pSystems_->netSystem.hasInitialized();

    pSystems_->netSystem.preUpdate(*pCore_);

    if (!pSystems_->netSystem.hasInitialized()) {
        return;
    }

    auto v = pSystems_->netSystem.retreiveCreatedEntities();
    auto playerIdx = -1u;
    const auto oldEntitySize = entities_.size();

    for (auto& entt : v) {
        entt.as<gfx::d3d12engine::Model>().get().markRenderPass(gfx::d3d12::rp::PBRIllumination::id);
        entt.as<gfx::d3d12engine::Model>().get().markRenderPass(gfx::d3d12::rp::ShadowMap::id);

        if (entt.as<NetEx>().hasCategory(NetExCategory::Player)) {
            // as a vector is a contiguous range,
            // address gap between two element represents the gap of the indices.
            playerIdx = static_cast<std::uint32_t>(&entt - v.data());

            const auto cameraOffset = mu::Vec3(0.f, 4.8f, -10.f);
            const auto cameraTimeLag = 1.f;

            entt.createComponent<PlayerController>();
            entt.createComponent<RigidBody>();

            entt.createComponent<gfx::d3d12engine::Camera>(gfx::d3d12::Camera::Config());
            auto& camera = entt.as<gfx::d3d12engine::Camera>();
            camera.get().coordMovement() << mu::translate(cameraOffset);
            camera.get().coordRotation().setLocalXform(
                mu::transpose(mu::lookAt(mu::Vec3(), -cameraOffset, mu::Vec3(0.f, 1.f, 0.f)))
            );
            camera.get().coordMovement().setParent(&pSystems_->coordRoot.get());
            camera.setOffset(cameraOffset);
            camera.setTimeLag(cameraTimeLag);
            camera.attach(entt.as<gfx::d3d12engine::Model>());

            pSystems_->inputSystem.addEntity(entt);
            pSystems_->physicsSystem.addEntity(entt);
        }

        pSystems_->coordRoot.addEntity(entt);
        pSystems_->collisionSystem.addEntity(entt);
        scene_.addEntity(entt);
    }
    
    std::ranges::move(v, std::back_inserter(entities_));

    if (!alreadyInitialized && pSystems_->netSystem.hasInitialized()) {
        pPlayer_ = &entities_[oldEntitySize + playerIdx];
        initScene();
    }
}

void Stage::updateNetwork(double deltaTime) {
    if (pPlayer_) {
        pPlayer_->as<NetEx>().generatePackets(*pSession_);
    }
    pSession_->flushPackets();
}

void Stage::processInput(double deltaTime) {
    pSystems_->inputSystem.update(static_cast<float>(deltaTime),
        ControllerAdapters{ .renderModeController = RenderModeController(pRenderer_) }
    );
}

void Stage::simulate(double deltaTime) {
    pSystems_->physicsSystem.update(static_cast<float>(deltaTime));

    if (pPlayer_) {
        pPlayer_->as<gameEngine::Coord>().get()
		    << mu::translate( pPlayer_->as<RigidBody>().deltaPosition()
        );
    }

    pSystems_->coordRoot.update();
    pSystems_->collisionSystem.update();

    if (pPlayer_) {
        pPlayer_->as<gfx::d3d12engine::Camera>().update(static_cast<float>(deltaTime));
        pPlayer_->as<gfx::d3d12engine::Camera>().get().updateView();
    }
}

void Stage::initEntities() {

}

void Stage::initScene() {
    directionalLight_.init(gfx::d3d12::WorldLight{
        .config = gfx::d3d12::WorldLight::Config{
            .ortho = {
                .width = 180.f,
                .height = 60.f,
                .nearZ = 200.f,
                .farZ = 3000.f
            }
        },
        .color = mu::Vec3(1.f, 0.92f, 0.76f),
        .dir = mu::NVec3(0.f, -1.f, 0.33f),
        .intensity = 5.0f,
        .distanceToCamera = 1600.f,
        .type = gfx::d3d12::sr::Light::Type::Directional
    });

    for (auto i = 0u; i < 3u; ++i) {
        for (auto j = 0u; j < 3u; ++j) {
            level_.activateChunk( i, j, *pCore_, assetBVHInfo(
                static_cast<AssetBVH>(etoi(AssetBVH::Terrain_0_0) + (i * 3u) + j)
            ).key, scene_, pSystems_->collisionSystem );
        }
    }

    scene_.addEntity(directionalLight_);

    scene_.clearStash();

    pRenderer_->init(scene_);
}

void Stage::loadAssets() {
    auto cmdList = pCore_->fetchCmdList();

    pCore_->prepareGPUResLoad();

    loadTextures(cmdList);
    loadModels(cmdList);
    loadBVHPaths();
    loadLevel(cmdList);

    pCore_->finishGPUResLoad();
}

void loadTexture(gfx::d3d12engine::Core& core, AssetTexture key) {
    const auto& texInfo = assetTextureInfo(key);
    for (auto i = 0ull; i < texInfo.keys.size(); ++i) {
        core.registTexturePath(texInfo.keys[i], texInfo.paths[i]);
        core.loadStaticTexture(texInfo.keys[i], texInfo.type);
    }
}

void loadHeightmapTexture(gfx::d3d12engine::Core& core, AssetTexture key) {
    const auto& texInfo = assetTextureInfo(key);
    for (auto i = 0ull; i < texInfo.keys.size(); ++i) {
        core.registTexturePath(texInfo.keys[i], texInfo.paths[i]);
        core.loadStaticTexture(texInfo.keys[i], D3D12_SHADER_RESOURCE_VIEW_DESC{
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = D3D12_TEX2D_SRV{ .MipLevels = 1u }
        });
    }
}

void Stage::loadTextures(gfx::d3d12::D3D12GfxCmdList& cmdList) {
    loadTexture(*pCore_, AssetTexture::Helicopter);
    loadTexture(*pCore_, AssetTexture::Character);
    loadTexture(*pCore_, AssetTexture::Tree0);
    loadTexture(*pCore_, AssetTexture::Tree1);
    loadTexture(*pCore_, AssetTexture::Tree2);
    loadTexture(*pCore_, AssetTexture::Terrain);
    loadHeightmapTexture(*pCore_, AssetTexture::TerrainHeightmap);
}

void loadModel(gfx::d3d12engine::Core& core, AssetModel key, Renderer& renderer) {
    auto modelInfo = assetModelInfo(key);
    core.registRefModelPath(modelInfo.key, modelInfo.path);
    core.loadRefModel(modelInfo.key);
    renderer.layoutVBsPBR(core, modelInfo.key, 1);
}

void Stage::loadModels(gfx::d3d12::D3D12GfxCmdList& cmdList) {
    pCore_->initChunkMesh(cmdList);
    loadModel(*pCore_, AssetModel::Helicopter, *pRenderer_);
    loadModel(*pCore_, AssetModel::Character, *pRenderer_);
    loadModel(*pCore_, AssetModel::Tree0, *pRenderer_);
    loadModel(*pCore_, AssetModel::Tree1, *pRenderer_);
    loadModel(*pCore_, AssetModel::Tree2, *pRenderer_);
}

void loadBVHPath(gfx::d3d12engine::Core& core, AssetBVH key) {
    auto bvhInfo = assetBVHInfo(key);
    core.loadBVHPath(bvhInfo.key, bvhInfo.path);
}

void Stage::loadBVHPaths() {
    loadBVHPath(*pCore_, AssetBVH::Helicopter);
    loadBVHPath(*pCore_, AssetBVH::Terrain_0_0);
    loadBVHPath(*pCore_, AssetBVH::Terrain_0_1);
    loadBVHPath(*pCore_, AssetBVH::Terrain_0_2);
    loadBVHPath(*pCore_, AssetBVH::Terrain_1_0);
    loadBVHPath(*pCore_, AssetBVH::Terrain_1_1);
    loadBVHPath(*pCore_, AssetBVH::Terrain_1_2);
    loadBVHPath(*pCore_, AssetBVH::Terrain_2_0);
    loadBVHPath(*pCore_, AssetBVH::Terrain_2_1);
    loadBVHPath(*pCore_, AssetBVH::Terrain_2_2);
}

void Stage::loadLevel(gfx::d3d12::D3D12GfxCmdList& cmdList) {
    level_ = gfx::d3d12engine::LevelRegion(*pCore_,
        resourcePath/"LevelGraph.bin",
        resourcePath/"LevelGraph_Terrain.bin"
    );
}