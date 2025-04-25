#include "cNetEx.hpp"

#include "objectInitializers.hpp"

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

void CNetExSystem::handleSCInitCreate(const SCInitCreate& scInitCreate) {
    auto translation = mu::Vec3(scInitCreate.xform.translation[0],
        scInitCreate.xform.translation[1], scInitCreate.xform.translation[2]
    );

    const auto x = scInitCreate.xform.rotation[0];
    const auto y = scInitCreate.xform.rotation[1];
    const auto z = scInitCreate.xform.rotation[2];

    float wSquared = 1.0f - (x * x + y * y + z * z);
    const auto w = (wSquared > 0.0f) ? std::sqrt(wSquared) : 0.0f;

    auto rotation = mu::NQuat(x, y, z, w);

    ecs::Entity* pEntity = nullptr;

    switch (scInitCreate.objType) {
    case ObjectType::Character:
        pEntity = &createdEntities_.emplace_back(
            createCharacter(translation, rotation, *pResStorage_,
                slotKeyModel, slotKeyBVHPath, slotKeyAnimClip
            )
        );
        pEntity->createComponent<NetEx>(std::make_unique<CNetExHelicopter>(pEntity->id().value()));
        pEntity->as<NetEx>().addCategory(NetExCategory::Character);
        addEntity(*pEntity);
        break;

    case ObjectType::Helicopter:
        pEntity = &createdEntities_.emplace_back(
            createHelicopter(translation, rotation, *pResStorage_,
                slotKeyModel, slotKeyBVHPath, slotKeyAnimClip
            )
        );
        pEntity->createComponent<NetEx>(std::make_unique<CNetExHelicopter>(pEntity->id().value()));
        pEntity->as<NetEx>().addCategory(NetExCategory::Helicopter);
        addEntity(*pEntity);
        break;

    case ObjectType::Tree0:
        pEntity = &createdEntities_.emplace_back(
            createTree0(translation, rotation, *pResStorage_,
                slotKeyModel, slotKeyBVHPath, slotKeyAnimClip
            )
        );
        pEntity->createComponent<NetEx>(std::make_unique<CNetExTree0>(pEntity->id().value()));
        addEntity(*pEntity);
        break;

    case ObjectType::Tree1:
        pEntity = &createdEntities_.emplace_back(
            createTree1(translation, rotation, *pResStorage_,
                slotKeyModel, slotKeyBVHPath, slotKeyAnimClip
            )
        );
        pEntity->createComponent<NetEx>(std::make_unique<CNetExTree1>(pEntity->id().value()));
        addEntity(*pEntity);
        break;

    case ObjectType::Tree2:
        pEntity = &createdEntities_.emplace_back(
            createTree2(translation, rotation, *pResStorage_,
                slotKeyModel, slotKeyBVHPath, slotKeyAnimClip
            )
        );
        pEntity->createComponent<NetEx>(std::make_unique<CNetExTree2>(pEntity->id().value()));
        addEntity(*pEntity);
        break;

    default:
        std::cerr << "Invalid object type received. : " << etoi(scInitCreate.objType) << '\n';
        break;
    }

    if (pEntity) {
        netIdToNetEx_[scInitCreate.netId] = pEntity->get<NetEx>();
    }
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