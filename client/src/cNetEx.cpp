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

    auto translation = mu::Vec3(scCreate.xform.translation[0],
        scCreate.xform.translation[1], scCreate.xform.translation[2]
    );

    const auto x = scCreate.xform.rotation[0];
    const auto y = scCreate.xform.rotation[1];
    const auto z = scCreate.xform.rotation[2];

    float wSquared = 1.0f - (x * x + y * y + z * z);
    const auto w = (wSquared > 0.0f) ? std::sqrt(wSquared) : 0.0f;

    auto rotation = mu::NQuat(x, y, z, w);

    switch (scCreate.objType) {
    case ObjectType::Helicopter:
        if (!core.refModelStorage().contains("GO_OH-58D")) {
            throw GFX_EXCEPT("RefModel not found: GO_OH-58D");
        }
        entity.createComponent<NetEx>(std::make_unique<CNetExHelicopter>(entity.id().value()), scCreate.netId);
        entity.createComponent<gameEngine::Coord>();
        entity.as<gameEngine::Coord>().get().setLocalXform(mu::translate(translation));

        entity.createComponent<gfx::d3d12engine::Model>("GO_OH-58D", core, entity.as<gameEngine::Coord>());
        entity.as<gfx::d3d12engine::Model>().get().root()->coord() << mu::Mat4x4(rotation);
        addEntity(entity);
        break;

    case ObjectType::Tree0:
        if (!core.refModelStorage().contains("GO_URP_Tree_0")) {
            throw GFX_EXCEPT("RefModel not found: GO_URP_Tree_0");
        }
        entity.createComponent<NetEx>(std::make_unique<CNetExTree0>(), scCreate.netId);
        entity.createComponent<gameEngine::Coord>();
        entity.as<gameEngine::Coord>().get().setLocalXform(mu::translate(translation));

        entity.createComponent<gfx::d3d12engine::Model>("GO_URP_Tree_0", core, entity.as<gameEngine::Coord>());
        entity.as<gfx::d3d12engine::Model>().get().root()->coord() << mu::Mat4x4(rotation);
        addEntity(entity);
        break;

    case ObjectType::Tree1:
        if (!core.refModelStorage().contains("GO_URP_Tree_1")) {
            throw GFX_EXCEPT("RefModel not found: GO_URP_Tree_1");
        }
        entity.createComponent<NetEx>(std::make_unique<CNetExTree1>(), scCreate.netId);
        entity.createComponent<gameEngine::Coord>();
        entity.as<gameEngine::Coord>().get().setLocalXform(mu::translate(translation));

        entity.createComponent<gfx::d3d12engine::Model>("GO_URP_Tree_1", core, entity.as<gameEngine::Coord>());
        entity.as<gfx::d3d12engine::Model>().get().root()->coord() << mu::Mat4x4(rotation);
        addEntity(entity);
        break;

    case ObjectType::Tree2:
        if (!core.refModelStorage().contains("GO_URP_Tree_2")) {
            throw GFX_EXCEPT("RefModel not found: GO_URP_Tree_2");
        }
        entity.createComponent<NetEx>(std::make_unique<CNetExTree2>(), scCreate.netId);
        entity.createComponent<gameEngine::Coord>();
        entity.as<gameEngine::Coord>().get().setLocalXform(mu::translate(translation));

        entity.createComponent<gfx::d3d12engine::Model>("GO_URP_Tree_2", core, entity.as<gameEngine::Coord>());
        entity.as<gfx::d3d12engine::Model>().get().root()->coord() << mu::Mat4x4(rotation);
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

    const auto translation = pCoord->get().localXform().row(3);
    const auto rotation = mu::quatRotMat(pModel->get().root()->coord().localXform());

    session.enqueuePacket( Packet{
        .size = calcPacketSize<CSWorld>(),
        .type = PacketType::CSWorld,
        .csWorld = CSWorld{
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
    auto pCoord = gameEngine::Coord::at(entityID_);
    auto pModel = gfx::d3d12engine::Model::at(entityID_);

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