#include "stage.hpp"

#include "assetMap.hpp"
#include "resourcePath.hpp"

#include <ranges>

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
    for (;;) {
        Packet recvPacket{};
        auto recvBytes = ::recv(pSession_->sock(), reinterpret_cast<char*>(&recvPacket), sizeof(Packet), 0);
        if (recvBytes == SOCKET_ERROR) {
            return;
        }

        const auto cameraOffset = mu::Vec3(0.f, 4.8f, -10.f);
        const auto cameraTimeLag = 1.f;

        static int fuck = 0;

        switch (recvPacket.type) {
        case PacketType::SCAssign: {
            pSession_->setId(recvPacket.scAssign.id);
            pPlayer_ = &entities_[recvPacket.scAssign.id];
            pPlayer_->createComponent<PlayerController>();

            pPlayer_->createComponent<gfx::d3d12engine::Camera>(gfx::d3d12::Camera::Config());
            auto& camera = pPlayer_->as<gfx::d3d12engine::Camera>();
            camera.get().coordMovement() << mu::translate(cameraOffset);
            camera.get().coordRotation().setLocalXform(
                mu::transpose(mu::lookAt(mu::Vec3(), -cameraOffset, mu::Vec3(0.f, 1.f, 0.f)))
            );
            camera.get().coordMovement().setParent(&pSystems_->coordRoot.get());
            camera.setOffset(cameraOffset);
            camera.setTimeLag(cameraTimeLag);
            camera.attach(pPlayer_->as<gfx::d3d12engine::Model>());

            pSystems_->coordRoot.addEntity(*pPlayer_);
            pSystems_->inputSystem.addEntity(*pPlayer_);
            pSystems_->physicsSystem.addEntity(*pPlayer_);

            initScene();
            break;
        }

        case PacketType::SCWorld:
            for (auto i = 0u; i < maxConnection; ++i) {
                if (i == pSession_->id()) {
                    continue;
                }
                auto& xform = recvPacket.scWorld.xforms[i];

                entities_[i].as<gfx::d3d12engine::Coord>().get().setLocalXform(mu::translate(xform.pos));
                entities_[i].as<gfx::d3d12engine::Model>().get().root()->coord().setLocalXform(mu::Mat4x4(xform.rot));
            }
            break;
        }
    }
}

void Stage::updateNetwork(double deltaTime) {
    if (pPlayer_) {
        pSession_->enqueuePacket(Packet{
            .type = PacketType::CSWorld,
            .size = sizeof(CSWorld),
            .csWorld = {
                .xform = {
                    .pos = pPlayer_->as<gfx::d3d12engine::Coord>().get().localXform().row(3),
                    .rot = mu::NQuat(mu::quatRotMat(pPlayer_->as<gfx::d3d12engine::Model>().get().root()->coord().localXform()))
                }
            }
        });
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
        pPlayer_->as<gfx::d3d12engine::Coord>().get()
		    << mu::translate( pPlayer_->as<RigidBody>().deltaPosition()
        );
    }

    pSystems_->coordRoot.update();

    if (pPlayer_) {
        pPlayer_->as<gfx::d3d12engine::Camera>().update(deltaTime);
        pPlayer_->as<gfx::d3d12engine::Camera>().get().updateView();
    }
}

void Stage::initEntities() {
    entities_ = level_.instantiateAllObjects(*pCore_, pSystems_->coordRoot.get());
}

void Stage::initScene() {
    directionalLight_.init(gfx::d3d12::WorldLight{
        .config = gfx::d3d12::WorldLight::Config{
            .ortho = {
                .width = 180.f,
                .height = 60.f,
                .nearZ = 600.f,
                .farZ = 1600.f
            }
        },
        .color = mu::Vec3(1.f, 0.92f, 0.76f),
        .dir = mu::NVec3(0.f, -1.f, 1.f),
        .intensity = 5.0f,
        .distanceToCamera = 800.f,
        .type = gfx::d3d12::sr::Light::Type::Directional
    });

    for (auto i = 0u; i < 3u; ++i) {
        for (auto j = 0u; j < 3u; ++j) {
            level_.activateChunk(i, j, scene_);
        }
    }

    for (auto& entt : entities_) {
        entt.as<gfx::d3d12engine::Model>().get().markRenderPass(gfx::d3d12::rp::PBRIllumination::id);
        entt.as<gfx::d3d12engine::Model>().get().markRenderPass(gfx::d3d12::rp::ShadowMap::id);
        entt.createComponent<RigidBody>();

        scene_.addEntity(entt);
        pSystems_->physicsSystem.addEntity(entt);
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

void Stage::loadTextures(gfx::d3d12::D3D12GfxCmdList& cmdList) {
    loadTexture(*pCore_, AssetTexture::Helicopter);
    loadTexture(*pCore_, AssetTexture::Tree0);
    loadTexture(*pCore_, AssetTexture::Tree1);
    loadTexture(*pCore_, AssetTexture::Tree2);
    loadTexture(*pCore_, AssetTexture::Terrain);
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
    loadModel(*pCore_, AssetModel::Tree0, *pRenderer_);
    loadModel(*pCore_, AssetModel::Tree1, *pRenderer_);
    loadModel(*pCore_, AssetModel::Tree2, *pRenderer_);
}

void Stage::loadLevel(gfx::d3d12::D3D12GfxCmdList& cmdList) {
    level_ = gfx::d3d12engine::LevelRegion(*pCore_);
}