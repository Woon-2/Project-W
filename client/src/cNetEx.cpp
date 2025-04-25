#include "cNetEx.hpp"

#include "game/physicsSystem.hpp"
#include "game/animSystem.hpp"

#include "assetMap.hpp"

#include <iostream>
#include <optional>

void CNetExSystem::preUpdate() {
    for (const auto& packet : pSession_->getRecvQueue()) {
        processPacket(packet);
    }
    pSession_->getRecvQueue().clear();
}

void CNetExSystem::postUpdate() {
    for (auto& pNetEx : components<NetEx>()) {
        pNetEx->generatePackets(*pSession_);
    }
}

void CNetExSystem::processPacket(const Packet& packet) {
    switch (packet.type) {
    case PacketType::SCInitInfo:
        handleSCInitInfo(packet.scInitInfo);
        break;

    case PacketType::SCInitCreate:
        handleSCInitCreate(packet.scInitCreate);
        break;

    case PacketType::SCWorld:
        handleSCWorld(packet.scWorld, packet);
        break;

    case PacketType::SCInitAssign:
        handleSCInitAssign(packet.scInitAssign);
        break;

    default:
        std::cout << "Invalid packet type received. : " << etoi(packet.type) << '\n';
        break;
    }
}

void CNetExSystem::handleSCInitInfo(const SCInitInfo& scInitInfo) {
    initPacketCnt_ = scInitInfo.packetCnt;
}

fsm::State characterStateIdle(fsm::FSM& fsm) {
    for (;;) {
        while (auto events = co_await fsm.getEvents()) {
            while (auto ev = events.pop()) {
                // do something
            }
        }
    
        co_await fsm.completeStateUpdate();
    }
}

void initAnimations(
    const gfx::d3d12::ResourceStorage& resStorage,
    AssetModel assetModel, AnimController& animCon
) {
    const auto& animClipSlot = resStorage.slot(CNetExSystem::slotKeyAnimClip);

    switch (assetModel) {
    case AssetModel::Character:
        animCon.fsm().addState("Idle", characterStateIdle);
        animCon.fsm().start("Idle");

        animCon.addClip("GO_Character_Idle",
            animClipSlot.get<AnimClip>("GO_Character_Idle")
        );
        animCon.addClip("GO_Character_Idle1",
            animClipSlot.get<AnimClip>("GO_Character_Idle1")
        );
        animCon.addClip("GO_Character_Idle2",
            animClipSlot.get<AnimClip>("GO_Character_Idle2")
        );
        animCon.addClip("GO_Character_Walk",
            animClipSlot.get<AnimClip>("GO_Character_Walk")
        );
        animCon.addClip("GO_Character_Run",
            animClipSlot.get<AnimClip>("GO_Character_Run")
        );

        circular(
            std::vector<std::string>{ "GO_Character_Idle", "GO_Character_Idle1",
                "GO_Character_Idle2", "GO_Character_Walk", "GO_Character_Run" },
            animCon
        ).resume();

        // animCon.play("GO_Character_Run", AnimInstance::ClipMode::Presampled);
        break;

    default:
        break;
    }
}

void buildEntityWithAsset( mu::Vec3 translation,
    mu::NQuat rotation,
    const gfx::d3d12::ResourceStorage& resStorage,
    std::optional<AssetModel> assetModel,
    std::optional<AssetBVH> assetBVH,
    ecs::Entity& entity
) {
    entity.createComponent<gameEngine::Coord>();
    entity.as<gameEngine::Coord>().get().setLocalXform(mu::translate(translation));

    auto& modelSlot = resStorage.slot(CNetExSystem::slotKeyModel);
    auto& bvhPathSlot = resStorage.slot(CNetExSystem::slotKeyBVHPath);

    if (assetModel.has_value()) {
        const auto asset = assetModel.value();
        const auto key = assetModelInfo(asset).key;
        bool hasAnimation = !assetModelInfo(asset).animationPath.empty();

        if (!modelSlot.contains<gfx::d3d12::RefModel>(key)) {
            throw GFX_EXCEPT("Model not found: " + key);
        }

        entity.createComponent<gfx::d3d12engine::Model>(modelSlot, key, entity.as<gameEngine::Coord>());
        if (hasAnimation) {
            entity.createComponent<AnimController>(std::to_string(entity.id().value()));
            entity.as<AnimController>().setSkeleton(
                entity.as<gfx::d3d12engine::Model>().get().refModel()->skeleton()
            );
            initAnimations(resStorage, assetModel.value(), entity.as<AnimController>());
        }
        entity.as<gfx::d3d12engine::Model>().get().root()->coord() << mu::Mat4x4(rotation);
    }

    if (assetBVH.has_value()) {
        const auto asset = assetBVH.value();
        const auto& key = assetBVHInfo(asset).key;

        if (!bvhPathSlot.contains<std::filesystem::path>(key)) {
            throw GFX_EXCEPT("BVH path not found: " + key);
        }

        entity.createComponent<BoundingVolume>(*bvhPathSlot.get<std::filesystem::path>(key));
    }
}

void CNetExSystem::handleSCInitCreate(const SCInitCreate& scInitCreate) {
    createdEntities_.emplace_back();
    auto& entity = createdEntities_.back();

    auto translation = mu::Vec3(scInitCreate.xform.translation[0],
        scInitCreate.xform.translation[1], scInitCreate.xform.translation[2]
    );

    const auto x = scInitCreate.xform.rotation[0];
    const auto y = scInitCreate.xform.rotation[1];
    const auto z = scInitCreate.xform.rotation[2];

    float wSquared = 1.0f - (x * x + y * y + z * z);
    const auto w = (wSquared > 0.0f) ? std::sqrt(wSquared) : 0.0f;

    auto rotation = mu::NQuat(x, y, z, w);

    switch (scInitCreate.objType) {
    case ObjectType::Character:
        entity.createComponent<NetEx>(std::make_unique<CNetExHelicopter>(entity.id().value()));
        entity.as<NetEx>().addCategory(NetExCategory::Character);
        buildEntityWithAsset(translation, rotation, *pResStorage_, AssetModel::Character, {}, entity);
        entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
            gfx::d3d12engine::rp::PBRAnimatedIllumination::id
        );
        entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
            gfx::d3d12engine::rp::ShadowMapAnimated::id
        );
        addEntity(entity);
        break;

    case ObjectType::Helicopter:
        entity.createComponent<NetEx>(std::make_unique<CNetExHelicopter>(entity.id().value()));
        entity.as<NetEx>().addCategory(NetExCategory::Helicopter);
        buildEntityWithAsset(translation, rotation, *pResStorage_, AssetModel::Helicopter, {}, entity);
        entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
            gfx::d3d12engine::rp::PBRIllumination::id
        );
        entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
            gfx::d3d12engine::rp::ShadowMap::id
        );
        addEntity(entity);
        break;

    case ObjectType::Tree0:
        entity.createComponent<NetEx>(std::make_unique<CNetExTree0>(entity.id().value()));
        buildEntityWithAsset(translation, rotation, *pResStorage_, AssetModel::Tree0, {}, entity);
        addEntity(entity);
        entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
            gfx::d3d12engine::rp::PBRIllumination::id
        );
        entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
            gfx::d3d12engine::rp::ShadowMap::id
        );
        break;

    case ObjectType::Tree1:
        entity.createComponent<NetEx>(std::make_unique<CNetExTree1>(entity.id().value()));
        buildEntityWithAsset(translation, rotation, *pResStorage_, AssetModel::Tree1, {}, entity);
        addEntity(entity);
        entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
            gfx::d3d12engine::rp::PBRIllumination::id
        );
        entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
            gfx::d3d12engine::rp::ShadowMap::id
        );
        break;

    case ObjectType::Tree2:
        entity.createComponent<NetEx>(std::make_unique<CNetExTree2>(entity.id().value()));
        buildEntityWithAsset(translation, rotation, *pResStorage_, AssetModel::Tree2, {}, entity);
        addEntity(entity);
        entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
            gfx::d3d12engine::rp::PBRIllumination::id
        );
        entity.as<gfx::d3d12engine::Model>().get().markRenderPass(
            gfx::d3d12engine::rp::ShadowMap::id
        );
        break;

    default:
        std::cerr << "Invalid object type received. : " << etoi(scInitCreate.objType) << '\n';
        break;
    }

    netIdToNetEx_[scInitCreate.netId] = entity.get<NetEx>();
    ++recvdInitPacketCnt_;
}

void CNetExSystem::handleSCWorld(const SCWorld& scWorld, const Packet& originalPacket) {
    if (!netIdToNetEx_.contains(scWorld.netId)) {
        std::cerr << "NetEx not found for netId: " << scWorld.netId << '\n';
        return;
    }
    
    auto pNetEx = netIdToNetEx_.at(scWorld.netId);
    if (!pNetEx) {
        std::cerr << "NetEx component not found for netId: " << scWorld.netId << '\n';
        return;
    }

    auto entityID = pNetEx->entityID();
    if (!entityID.has_value()) {
        std::cerr << "EntityID not found for netId: " << scWorld.netId << '\n';
        return;
    }

    pNetEx->processPacket(originalPacket);
}

void CNetExSystem::handleSCInitAssign(const SCInitAssign& scAssign) {
    if (!netIdToNetEx_.contains(scAssign.netId)) {
        std::cerr << "NetEx not found for netId: " << scAssign.netId << '\n';
        return;
    }

    auto pNetEx = netIdToNetEx_.at(scAssign.netId);
    if (!pNetEx) {
        std::cerr << "NetEx component not found for netId: " << scAssign.netId << '\n';
        return;
    }

    pNetEx->addCategory(NetExCategory::Player);
    ++recvdInitPacketCnt_;
}

void CNetExHelicopter::generatePackets(Session& session) {
    auto pCoord = gameEngine::Coord::at(entityID());
    auto pModel = gfx::d3d12engine::Model::at(entityID());

    const auto translation = pCoord->get().localXform().row(3);
    const auto rotation = mu::quatRotMat(pModel->get().root()->coord().localXform());

    session.enqueuePacket( Packet{
        .size = calcPacketSize<CSWorld>(),
        .type = PacketType::CSWorld,
        .csWorld = CSWorld{
            .netId = netId(),
            .xform = RigidXform{
                .translation = {translation.x(), translation.y(), translation.z()},
                .rotation = {rotation.x(), rotation.y(), rotation.z()}
            }
        }
    } );
}

void CNetExHelicopter::processPacket(const Packet& packet) {
    switch (packet.type) {
    case PacketType::SCWorld:
        handleSCWorld(packet.scWorld);
        break;

    default:
        std::cerr << "Invalid packet type received. : " << etoi(packet.type) << '\n';
        break;
    }
}

void CNetExHelicopter::handleSCWorld(const SCWorld& scWorld) {
    auto pCoord = gameEngine::Coord::at(entityID());
    auto pModel = gfx::d3d12engine::Model::at(entityID());

    const auto translation = mu::Vec3(scWorld.xform.translation[0],
        scWorld.xform.translation[1], scWorld.xform.translation[2]
    );

    const auto x = scWorld.xform.rotation[0];
    const auto y = scWorld.xform.rotation[1];
    const auto z = scWorld.xform.rotation[2];

    float wSquared = 1.0f - (x * x + y * y + z * z);
    const auto w = (wSquared > 0.0f) ? std::sqrt(wSquared) : 0.0f;

    const auto rotation = mu::NQuat(x, y, z, w);

    pCoord->get().setLocalXform(mu::translate(translation));
    pModel->get().root()->coord().setLocalXform(mu::Mat4x4(rotation));
}