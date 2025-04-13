#include "stage.hpp"

#include "assetMap.hpp"
#include "resourcePath.hpp"

#include <ranges>
#include <algorithm>

void Stage::init() {
    prepareResStorage();
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

    pSystems_->netSystem.preUpdate();

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

            const auto cameraOffset = mu::Vec3(0.f, 1.8f, -1.6f);
            const auto cameraTimeLag = 0.4f;

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
            level_.activateChunk( i, j, staticResStorage_.slot(slotKeyBVHPath), assetBVHInfo(
                static_cast<AssetBVH>(etoi(AssetBVH::Terrain_0_0) + (i * 3u) + j)
            ).key, scene_, pSystems_->collisionSystem );
        }
    }

    scene_.addEntity(directionalLight_);

    scene_.clearStash();

    pRenderer_->init(scene_);
}

void Stage::prepareResStorage() {
    staticResStorage_.addSlot(slotKeyTexture, gfx::d3d12::ResourceStorage::ResType::Texture);
    staticResStorage_.addSlot(slotKeyTexArray, gfx::d3d12::ResourceStorage::ResType::TexArray);
    staticResStorage_.addSlot(slotKeyTexCube, gfx::d3d12::ResourceStorage::ResType::TexCube);
    staticResStorage_.addSlot(slotKeyModel, gfx::d3d12::ResourceStorage::ResType::RefModel);
    staticResStorage_.addSlot(slotKeyBVHPath, gfx::d3d12::ResourceStorage::ResType::BVHPath);
    staticResStorage_.addSlot(slotKeySkeleton, gfx::d3d12::ResourceStorage::ResType::Skeleton);
    staticResStorage_.addSlot(slotKeyAnimClip, gfx::d3d12::ResourceStorage::ResType::AnimClip);

    pSystems_->netSystem.linkResStorage(&staticResStorage_);
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

void loadTexture( gfx::d3d12::ResourceStorage& storage,
    gfx::d3d12::DescriptorRanges& descRanges,
    gfx::d3d12::D3D12Device& device, gfx::d3d12::D3D12GfxCmdList& cmdList,
    AssetTexture key
) {
    const auto& texInfo = assetTextureInfo(key);
    for (auto i = 0ull; i < texInfo.keys.size(); ++i) {
        gfx::d3d12::loadTextureAt(
            storage.slot(Stage::slotKeyTexture), texInfo.paths[i].string(), device,
            cmdList, descRanges.srvRangeTex2D, texInfo.paths[i]
        );
    }
}

void loadHeightmapTexture( gfx::d3d12::ResourceStorage& storage,
    gfx::d3d12::DescriptorRanges& descRanges,
    gfx::d3d12::D3D12Device& device, gfx::d3d12::D3D12GfxCmdList& cmdList,
    AssetTexture key
) {
    const auto& texInfo = assetTextureInfo(key);
    for (auto i = 0ull; i < texInfo.keys.size(); ++i) {
        gfx::d3d12::loadTextureAt(
            storage.slot(Stage::slotKeyTexture), texInfo.paths[i].string(), device,
            cmdList, descRanges.srvRangeTex2D, texInfo.paths[i],
            D3D12_SHADER_RESOURCE_VIEW_DESC{
                .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Texture2D = D3D12_TEX2D_SRV{ .MipLevels = 1u }
            }
        );
    }
}

void Stage::loadTextures(gfx::d3d12::D3D12GfxCmdList& cmdList) {
    loadTexture( staticResStorage_, pCore_->descRanges(),
        pCore_->device(), cmdList, AssetTexture::Helicopter
    );
    loadTexture( staticResStorage_, pCore_->descRanges(),
        pCore_->device(), cmdList, AssetTexture::Character
    );
    loadTexture( staticResStorage_, pCore_->descRanges(),
        pCore_->device(), cmdList, AssetTexture::Tree0
    );
    loadTexture( staticResStorage_, pCore_->descRanges(),
        pCore_->device(), cmdList, AssetTexture::Tree1
    );
    loadTexture( staticResStorage_, pCore_->descRanges(),
        pCore_->device(), cmdList, AssetTexture::Tree2
    );
    loadTexture(staticResStorage_, pCore_->descRanges(),
        pCore_->device(), cmdList, AssetTexture::Terrain
    );

    loadHeightmapTexture( staticResStorage_, pCore_->descRanges(),
        pCore_->device(), cmdList, AssetTexture::TerrainHeightmap
    );
}

void loadModel( gfx::d3d12::ResourceStorage& storage,
    gfx::d3d12::D3D12Device& device, gfx::d3d12::D3D12GfxCmdList& cmdList,
    AssetModel key, Renderer& renderer
) {
    auto modelInfo = assetModelInfo(key);
    if (!modelInfo.animationPath.empty()) {
        gfx::d3d12::loadSkeletalRefModelAndAnimAt(
            storage.slot(Stage::slotKeyModel), storage.slot(Stage::slotKeySkeleton),
            storage.slot(Stage::slotKeyAnimClip), modelInfo.key, modelInfo.key,
            device, cmdList, modelInfo.geometryPath,
            storage.slot(Stage::slotKeyTexture),
            storage.slot(Stage::slotKeyTexArray),
            storage.slot(Stage::slotKeyTexCube),
            modelInfo.animationPath
        );
    }
    else {
        gfx::d3d12::loadRefModelAt(
            storage.slot(Stage::slotKeyModel), modelInfo.key, device, cmdList,
            modelInfo.geometryPath,
            storage.slot(Stage::slotKeyTexture),
            storage.slot(Stage::slotKeyTexArray),
            storage.slot(Stage::slotKeyTexCube)
        );
    }
    renderer.layoutVBsPBR( device, cmdList,
        *storage.slot(Stage::slotKeyModel).get<gfx::d3d12::RefModel>(modelInfo.key),
        1u
    );
}

void Stage::loadModels(gfx::d3d12::D3D12GfxCmdList& cmdList) {
    pCore_->initChunkMesh(cmdList);
    loadModel( staticResStorage_, pCore_->device(), cmdList,
        AssetModel::Helicopter, *pRenderer_
    );
    loadModel( staticResStorage_, pCore_->device(), cmdList,
        AssetModel::Character, *pRenderer_
    );

    loadModel( staticResStorage_, pCore_->device(), cmdList,
        AssetModel::Tree0, *pRenderer_
    );

    loadModel( staticResStorage_, pCore_->device(), cmdList,
        AssetModel::Tree1, *pRenderer_
    );

    loadModel( staticResStorage_, pCore_->device(), cmdList,
        AssetModel::Tree2, *pRenderer_
    );
}

void loadBVHPath(gfx::d3d12::ResourceStorage& storage, AssetBVH key) {
    auto bvhInfo = assetBVHInfo(key);
    storage.slot(Stage::slotKeyBVHPath).load<std::filesystem::path>(
        bvhInfo.key, bvhInfo.path
    );
}

void Stage::loadBVHPaths() {
    loadBVHPath(staticResStorage_, AssetBVH::Helicopter);
    loadBVHPath(staticResStorage_, AssetBVH::Terrain_0_0);
    loadBVHPath(staticResStorage_, AssetBVH::Terrain_0_1);
    loadBVHPath(staticResStorage_, AssetBVH::Terrain_0_2);
    loadBVHPath(staticResStorage_, AssetBVH::Terrain_1_0);
    loadBVHPath(staticResStorage_, AssetBVH::Terrain_1_1);
    loadBVHPath(staticResStorage_, AssetBVH::Terrain_1_2);
    loadBVHPath(staticResStorage_, AssetBVH::Terrain_2_0);
    loadBVHPath(staticResStorage_, AssetBVH::Terrain_2_1);
    loadBVHPath(staticResStorage_, AssetBVH::Terrain_2_2);
}

void Stage::loadLevel(gfx::d3d12::D3D12GfxCmdList& cmdList) {
    level_ = gfx::d3d12engine::LevelRegion(
        staticResStorage_.slot(Stage::slotKeyTexture),
        resourcePath/"LevelGraph.bin",
        resourcePath/"LevelGraph_Terrain.bin"
    );
}