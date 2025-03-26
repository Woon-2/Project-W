#include "sNetEx.hpp"

#include <iostream>

void SNetExSystem::addEntity(ecs::Entity& entity) {
    ecs::System<NetEx>::addEntity(entity);
    auto& netEx = entity.as<NetEx>();

    if ( typeid(*netEx.getProcessor()) == typeid(SNetExHelicopter) ) {
        freeHelicopters_.push_back(&netEx);
    }

    netIdToNetEx_[netEx.id()] = &netEx;
}

void SNetExSystem::addSession(Session& session) {
    pSessions_.push_back(&session);

    for (auto& pNetEx : components<NetEx>()) {
        auto pCoord = gameEngine::Coord::at(pNetEx->entityID().value());

        auto translation = pCoord->get().xform().row(3);

        session.enqueuePacket(
            Packet{
                .size = calcPacketSize<SCCreate>(),
                .type = PacketType::SCCreate,
                .scCreate = SCCreate{
                    .netId = pNetEx->id(),
                    .objType = ObjectType::Helicopter,
                    .xform = {
                        .translation = {translation.x(), translation.y(), translation.z()},
                        .rotation = {0.0f, 0.0f, 0.0f}
                    }
                }
            }
        );
    }

    auto pNetEx = freeHelicopters_.front();
    freeHelicopters_.pop_front();

    session.enqueuePacket(
        Packet{
            .size = calcPacketSize<SCAssign>(),
            .type = PacketType::SCAssign,
            .scAssign = SCAssign{
                .netId = pNetEx->id()
            }
        }
    );
}

void SNetExSystem::preUpdate() {
    for (auto& pSession : pSessions_) {
        for (const auto& packet : pSession->getRecvQueue()) {
            processPacket(packet);
        }
        pSession->getRecvQueue().clear();
    }
}

void SNetExSystem::postUpdate() {
    for (auto& pSession : pSessions_) {
        for (auto& pNetEx : components<NetEx>()) {
            pNetEx->generatePackets(*pSession);
        }
    }
}

void SNetExSystem::processPacket(const Packet& packet) {
    switch (packet.type) {
    case PacketType::CSWorld:
        for (auto& pNetEx : components<NetEx>()) {
            pNetEx->processPacket(packet);
        }
        break;
    
    default:
        break;
    }
}

void SNetExHelicopter::generatePackets(Session& session) {
    if (lastReceivedWorld_.has_value()) {
        session.enqueuePacket(
            Packet{
                .size = calcPacketSize<SCWorld>(),
                .type = PacketType::SCWorld,
                .scWorld = SCWorld {
                    .netId = lastReceivedWorld_.value().netId,
                    .xform = lastReceivedWorld_.value().xform
                }
            }
        );
    }
}

void SNetExHelicopter::processPacket(const Packet& packet) {
    switch (packet.type) {
    case PacketType::CSWorld:
        handleCSWorld(packet.csWorld);
        break;

    default:
        std::cerr << "Invalid packet type received. : " << etoi(packet.type) << '\n';
        break;
    }
}

void SNetExHelicopter::handleCSWorld(const CSWorld& csWorld) {
    lastReceivedWorld_ = csWorld;
}

void SNetExAI::generatePackets(Session& session) {
    auto pCoord = gameEngine::Coord::at(entityID_);

    const auto translation = pCoord->get().xform().row(3);

    session.enqueuePacket(
        Packet{
            .size = calcPacketSize<SCWorld>(),
            .type = PacketType::SCWorld,
            .scWorld = SCWorld {
                .netId = netId_,
                .xform = {
                    .translation = { translation.x(), translation.y(), translation.z() },
                    .rotation = { 0.0f, 0.0f, 0.0f }
                }
            }
        }
    );
}

void SNetExAI::processPacket(const Packet& packet) {

}