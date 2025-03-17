#include "cNetEx.hpp"

#include <iostream>

void CNetExSystem::preUpdate(const gfx::d3d12engine::Core& core) {
    for (const auto& packet : pSession_->getRecvQueue()) {
        processPacket(packet, core);
    }
    pSession_->getRecvQueue().clear();
}

void CNetExSystem::postUpdate() {
    for (auto& pNetEx : components<NetEx>()) {
        pNetEx->generatePackets(*pSession_);
    }
}

void CNetExSystem::processPacket(const Packet& packet, const gfx::d3d12engine::Core& core) {
    switch (packet.type) {
    case PacketType::SCCreate:
        handleSCCreate(packet.scCreate, core);
        break;

    case PacketType::SCWorld:
        handleSCWorld(packet.scWorld, packet);
        break;

    case PacketType::SCAssign:
        handleSCAssign(packet.scAssign);
        break;

    default:
        std::cout << "Invalid packet type received. : " << etoi(packet.type) << '\n';
        break;
    }
}

void CNetExSystem::handleSCCreate(const SCCreate& scCreate, const gfx::d3d12engine::Core& core) {
    createdEntities_.emplace_back();
    auto& entity = createdEntities_.back();

    switch (scCreate.objType) {
    case ObjectType::Helicopter:
        if (!core.refModelStorage().contains("GO_OH-58D")) {
            throw GFX_EXCEPT("RefModel not found: GO_OH-58D");
        }
        entity.createComponent<NetEx>(std::make_unique<CNetExHelicopter>(entity.id().value()), scCreate.netId);
        entity.createComponent<gameEngine::Coord>();
        entity.as<gameEngine::Coord>().get().setLocalXform(mu::translate(scCreate.xform.translation));

        entity.createComponent<gfx::d3d12engine::Model>("GO_OH-58D", core, entity.as<gameEngine::Coord>());
        entity.as<gfx::d3d12engine::Model>().get().root()->coord() << mu::Mat4x4(scCreate.xform.rotation);
        addEntity(entity);
        break;

    case ObjectType::Tree0:
        if (!core.refModelStorage().contains("GO_URP_Tree_0")) {
            throw GFX_EXCEPT("RefModel not found: GO_URP_Tree_0");
        }
        entity.createComponent<NetEx>(std::make_unique<CNetExTree0>(), scCreate.netId);
        entity.createComponent<gameEngine::Coord>();
        entity.as<gameEngine::Coord>().get().setLocalXform(mu::translate(scCreate.xform.translation));

        entity.createComponent<gfx::d3d12engine::Model>("GO_URP_Tree_0", core, entity.as<gameEngine::Coord>());
        entity.as<gfx::d3d12engine::Model>().get().root()->coord() << mu::Mat4x4(scCreate.xform.rotation);
        addEntity(entity);
        break;

    case ObjectType::Tree1:
        if (!core.refModelStorage().contains("GO_URP_Tree_1")) {
            throw GFX_EXCEPT("RefModel not found: GO_URP_Tree_1");
        }
        entity.createComponent<NetEx>(std::make_unique<CNetExTree1>(), scCreate.netId);
        entity.createComponent<gameEngine::Coord>();
        entity.as<gameEngine::Coord>().get().setLocalXform(mu::translate(scCreate.xform.translation));

        entity.createComponent<gfx::d3d12engine::Model>("GO_URP_Tree_1", core, entity.as<gameEngine::Coord>());
        entity.as<gfx::d3d12engine::Model>().get().root()->coord() << mu::Mat4x4(scCreate.xform.rotation);
        addEntity(entity);
        break;

    case ObjectType::Tree2:
        if (!core.refModelStorage().contains("GO_URP_Tree_2")) {
            throw GFX_EXCEPT("RefModel not found: GO_URP_Tree_2");
        }
        entity.createComponent<NetEx>(std::make_unique<CNetExTree2>(), scCreate.netId);
        entity.createComponent<gameEngine::Coord>();
        entity.as<gameEngine::Coord>().get().setLocalXform(mu::translate(scCreate.xform.translation));

        entity.createComponent<gfx::d3d12engine::Model>("GO_URP_Tree_2", core, entity.as<gameEngine::Coord>());
        entity.as<gfx::d3d12engine::Model>().get().root()->coord() << mu::Mat4x4(scCreate.xform.rotation);
        addEntity(entity);
        break;

    default:
        std::cerr << "Invalid object type received. : " << etoi(scCreate.objType) << '\n';
        break;
    }

    netIdToNetEx_[scCreate.netId] = entity.get<NetEx>();
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

void CNetExSystem::handleSCAssign(const SCAssign& scAssign) {
    if (!netIdToNetEx_.contains(scAssign.netId)) {
        std::cerr << "NetEx not found for netId: " << scAssign.netId << '\n';
        return;
    }

    auto pNetEx = netIdToNetEx_.at(scAssign.netId);
    if (!pNetEx) {
        std::cerr << "NetEx component not found for netId: " << scAssign.netId << '\n';
        return;
    }

    playerID_ = pNetEx->entityID();
}

void CNetExHelicopter::generatePackets(Session& session) {
    auto pCoord = gameEngine::Coord::at(entityID_);
    auto pModel = gfx::d3d12engine::Model::at(entityID_);

    session.enqueuePacket( Packet{
        .size = 16u + sizeof(CSWorld),
        .type = PacketType::CSWorld,
        .csWorld = CSWorld{
            .xform = RigidXform{
                .translation = pCoord->get().localXform().row(3),
                .rotation = mu::NQuat(mu::quatRotMat(pModel->get().root()->coord().localXform()))
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
    auto pCoord = gameEngine::Coord::at(entityID_);
    auto pModel = gfx::d3d12engine::Model::at(entityID_);

    pCoord->get().setLocalXform(mu::translate(scWorld.xform.translation));
    pModel->get().root()->coord().setLocalXform(mu::Mat4x4(scWorld.xform.rotation));
}