#include "stage.hpp"

#include "assetMap.hpp"
#include "resourcePath.hpp"
#include "objectInitializers.hpp"

pmr::unordered_map<ecs::Entity::ID, u32t> gEIdToNetId;
pmr::unordered_map<u32t, ecs::Entity::ID> gNetIdToEId;
pmr::unordered_map<ecs::Entity::ID, mu::Vec3> gDestPoses;

Stage* gpStage = nullptr;

// InputNetworkForwarder 만들기: pushInputEvent (PlayerController가 호출)
// InputNetworkForwarder가 Session&에 대고 패킷을 넣어줌 (buildPackets)

void processSCEnter(SCEnter& scEnter, Session& session, Stage& stage);
void processSCMove(SCMove& scMove, Session& session, Stage& stage);
void processSCLeave(SCLeave& scLeave, Session& session, Stage& stage);
void processSCAssign(SCAssign& scAssign, Session& session, Stage& stage);

void processPacket(Packet& packet, Session& session) {
    switch (packet.type) {
    case PacketType::SCEnter:
        processSCEnter(packet.scEnter, session, *gpStage);
        break;

    case PacketType::SCMove:
        processSCMove(packet.scMove, session, *gpStage);
        break;

    case PacketType::SCLeave:
		processSCLeave( packet.scLeave, session, *gpStage );
        break;

    case PacketType::SCAssign:
        processSCAssign(packet.scAssign, session, *gpStage);
        break;

    default:
        break;
    }
}

void processSCEnter(SCEnter& scEnter, Session& session, Stage& stage) {
    const auto translation = mu::Vec3(
        scEnter.xform.translation[0],
        scEnter.xform.translation[1],
        scEnter.xform.translation[2]
    );

    const auto rotation = mu::NQuat(
        scEnter.xform.rotation[0],
        scEnter.xform.rotation[1],
        scEnter.xform.rotation[2],
        scEnter.xform.rotation[3],
        mu::NQuat::NoNormalize_t{}
    );

    switch (scEnter.objType) {
    case ObjectType::Character: {
        auto entt = createCharacter( 
            translation,
            rotation,
            stage.resStorage(),
            Stage::slotKeyModel,
            Stage::slotKeyBVHPath,
            Stage::slotKeyAnimClip
        );

        entt.createComponent<RigidBody>();
        auto& rb = entt.as<RigidBody>();
        rb.setInvMass( 1.f / 50.f );
        rb.setKFriction( 0.5f );
        rb.disableGravity( );
        
        gNetIdToEId[scEnter.netId] = entt.id().value();
        gEIdToNetId[entt.id().value()] = scEnter.netId;
        gDestPoses[entt.id().value()] = translation;

        stage.addEntity( std::move(entt) );
        break;
    }

    default:
        break;
    }
}

void processSCMove(SCMove& scMove, Session& session, Stage& stage) {
    SCMove delayed{};

    for (u8t i = 0u; i < scMove.moveCnt; ++i) {
        if (!gNetIdToEId.contains(scMove.moves[i].netId)) {
            delayed.moves[delayed.moveCnt++] = scMove.moves[i];
            if (delayed.moveCnt == SCMove::maxMoveCnt) {
                session.delayPacket(
                    Packet{
                        .size = calcPacketSize<SCMove>(SCMove::maxMoveCnt   ),
                        .type = PacketType::SCMove,
                        .scMove = delayed
                    }
                );
                delayed.moveCnt = 0;
            }
            continue;
        }

        const auto eid = gNetIdToEId.at(scMove.moves[i].netId);

        const auto& move = scMove.moves[i];

        const auto dp = gameEngine::Coord::decodeDeltaPos(move.compressedDeltaPos);
        const auto dr = gameEngine::Coord::decodeDeltaRot(move.compressedDeltaRot);

        const auto curPos = mu::Vec3(gameEngine::Coord::at(eid)->get().xform().row(3));
        const auto destPos = gDestPoses.at(eid) + dp;

        auto& rb = *RigidBody::at(eid);
        rb.setVelocity((destPos - curPos) / 0.033f); // server tick time

        gDestPoses.at(eid) = destPos;
        gfx::d3d12engine::Model::at(eid)->get().root()->coord() << mu::Mat4x4(dr);
    }

    if (delayed.moveCnt > 0) {
        session.delayPacket(
            Packet{
                .size = calcPacketSize<SCMove>(delayed.moveCnt),
                .type = PacketType::SCMove,
                .scMove = delayed
            }
        );
    }
}

void processSCLeave( SCLeave& scLeave, Session& session, Stage& stage ) {
    for ( auto i = 0u; i < scLeave.leaveCnt; ++i ) {
		auto netId = scLeave.leavedIds[ i ];
        if ( !gNetIdToEId.contains( netId ) ) {
            continue;
        }

		auto eId = gNetIdToEId.at( netId );
		auto entt = ecs::Entity( eId );
        if ( !entt.valid( ) ) {
            continue;
        }
        
		gNetIdToEId.erase( netId );
		gEIdToNetId.erase( eId );

		stage.pScene( )->eraseEntity( entt );
		stage.pSystems( )->animSystem.eraseEntity( entt );
		stage.pSystems( )->coordRoot.eraseEntity( entt );
		stage.pSystems( )->physicsSystem.eraseEntity( entt );
		stage.pSystems( )->collisionSystem.eraseEntity( entt );
		stage.pSystems( )->inputSystem.eraseEntity( entt );
        entt.release();
        // entt.reset( );
    }
}

void processSCAssign(SCAssign& scAssign, Session& session, Stage& stage) {
    if (!gNetIdToEId.contains(scAssign.netId)) {
        session.delayPacket(
            Packet{
                .size = calcPacketSize<SCAssign>(),
                .type = PacketType::SCAssign,
                .scAssign = scAssign
            }
        );
        return;
    }

    auto entt = ecs::Entity(gNetIdToEId.at(scAssign.netId));

    if (!entt.valid()) {
        session.delayPacket(
            Packet{
                .size = calcPacketSize<SCAssign>(),
                .type = PacketType::SCAssign,
                .scAssign = scAssign
            }
        );
        return;
    }

    session.setEntityId(entt.id().value());
    auto it = std::ranges::find_if(stage.entities(), [playerId = entt.id().value()](const auto& e) {
        if (!e.valid()) {
            return false;
        }
        return e.id().value() == playerId;
    } );
    stage.setPlayer(&*it);

    entt.createComponent<PlayerController>(std::make_unique<NetworkInputHandler>(entt.id().value()));

    entt.createComponent<gfx::d3d12engine::Camera>(gfx::d3d12::Camera::Config());
    auto& camera = entt.as<gfx::d3d12engine::Camera>();

    const auto cameraOffset = mu::Vec3(0.f, 1.8f, -1.6f);
    const auto cameraTimeLag = 0.4f;

    camera.get().coordMovement() << mu::translate(cameraOffset);
    camera.get().coordRotation().setLocalXform(
        mu::transpose(mu::lookAt(mu::Vec3(), -cameraOffset, mu::Vec3(0.f, 1.f, 0.f)))
    );
    camera.get().coordMovement().setParent(&stage.pSystems()->coordRoot.get());
    camera.setOffset(cameraOffset);
    camera.setTimeLag(cameraTimeLag);
    camera.attach(entt.as<gfx::d3d12engine::Model>());

    stage.pSystems()->inputSystem.addEntity(entt);
    stage.pScene()->updateEntity(entt);

    stage.initScene();

    entt.release();
}

void Stage::addEntity(ecs::Entity&& entt) {
    if (entt.valid()) {
        if (entt.get<AnimController>()) {
            pSystems_->animSystem.addEntity(entt);
        }

        pSystems_->coordRoot.addEntity(entt);
        pSystems_->physicsSystem.addEntity(entt);
        pSystems_->collisionSystem.addEntity(entt);
        scene_.addEntity(entt);

        entities_.emplace_back(std::move(entt));
    }
}

void Stage::init() {
    pSession_->setPacketProcessor(processPacket);
    gpStage = this;

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

    auto packetQ = std::move(pSession_->getDelayedQueue());
    pSession_->getDelayedQueue().clear();

    for (auto& packet : packetQ) {
        processPacket(packet, *pSession_);
    }
}

void Stage::updateNetwork(double deltaTime) {
    pControllerAdapters_->inputNetworkForwarder.buildPackets(*pSession_);
    pSession_->flushPackets();
}

void Stage::processInput(double deltaTime) {
    pSystems_->inputSystem.update( MilliSeconds( static_cast<float>(deltaTime) * 1000.f ),
        pCore_->window().client(), *pControllerAdapters_
    );
}

void Stage::simulate(double deltaTime) {
    pSystems_->physicsSystem.update( MilliSeconds( static_cast<float>(deltaTime) * 1000.f ) );

    for (auto& entity : entities_) {
        if (!entity.valid()) {
            continue;
        }

        const auto eid = entity.id().value();

        if (auto pCoord = gameEngine::Coord::at(eid)) {
            const auto cdp = pCoord->compressedDeltaPos();
            const auto cdr = pCoord->compressedDeltaRot();
            pCoord->resetDeltaPos();
            pCoord->resetDeltaRot();

            const auto dp = pCoord->decodeDeltaPos(cdp);
            const auto dr = pCoord->decodeDeltaRot(cdr);

            pCoord->get() << mu::translate(dp);
            if (auto pModel = gfx::d3d12engine::Model::at(eid)) {
                pModel->get().root()->coord() << mu::Mat4x4(dr);
            }
        }
    }

    pSystems_->coordRoot.update( );
    
    if (pPlayer_) {
        pPlayer_->as<AnimController>().print();
    }

    auto cmdList = pCore_->fetchCmdList();
    cmdList.reset();
    pSystems_->animSystem.update( pCore_->cmdQueue(), cmdList,
        MilliSeconds(static_cast<float>(deltaTime) * 1000.f)
    );
    cmdList.close();
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
}

void Stage::loadAssets() {
    auto cmdList = pCore_->fetchCmdList();

    pCore_->prepareGPUResLoad();

    loadTextures(cmdList);
    loadModels(cmdList);
    loadBVHPaths();
    loadLevel(cmdList);

	auto pTex = staticResStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>(assetTextureInfo(AssetTexture::SkySphere).paths.front().string());

    pRenderer_->initResources(pCore_->device(), cmdList, pTex);

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

	loadTexture(staticResStorage_, pCore_->descRanges(),
		pCore_->device(), cmdList, AssetTexture::SkySphere
	);
}

void loadModel( gfx::d3d12::ResourceStorage& storage,
    gfx::d3d12engine::Core& core, gfx::d3d12::D3D12GfxCmdList& cmdList,
    AssetModel key, Renderer& renderer
) {
    auto& device = core.device();
    auto modelInfo = assetModelInfo(key);
    if (!modelInfo.animationPath.empty()) {
        gfx::d3d12::loadSkeletalRefModelAndAnimAt(
            storage.slot(Stage::slotKeyModel), storage.slot(Stage::slotKeySkeleton),
            storage.slot(Stage::slotKeyAnimClip), storage.slot(Stage::slotKeyTexture),
            modelInfo.key, modelInfo.key,
            device, cmdList, modelInfo.geometryPath,
            storage.slot(Stage::slotKeyTexture),
            storage.slot(Stage::slotKeyTexArray),
            storage.slot(Stage::slotKeyTexCube),
            core.descRanges().srvRangeTex2D,
            modelInfo.animationPath
        );
        renderer.layoutVBsPBRAnimated(device, cmdList,
            *storage.slot(Stage::slotKeyModel).get<gfx::d3d12::RefModel>(modelInfo.key),
            1u
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
        renderer.layoutVBsPBR(device, cmdList,
            *storage.slot(Stage::slotKeyModel).get<gfx::d3d12::RefModel>(modelInfo.key),
            1u
        );
    }
}

void Stage::loadModels(gfx::d3d12::D3D12GfxCmdList& cmdList) {
    pCore_->initChunkMesh(cmdList);
    loadModel( staticResStorage_, *pCore_, cmdList,
        AssetModel::Helicopter, *pRenderer_
    );
    loadModel( staticResStorage_, *pCore_, cmdList,
        AssetModel::Character, *pRenderer_
    );

    loadModel( staticResStorage_, *pCore_, cmdList,
        AssetModel::Tree0, *pRenderer_
    );

    loadModel( staticResStorage_, *pCore_, cmdList,
        AssetModel::Tree1, *pRenderer_
    );

    loadModel( staticResStorage_, *pCore_, cmdList,
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
        getResourcePath()/"LevelGraph.bin",
        getResourcePath()/"LevelGraph_Terrain.bin"
    );
}