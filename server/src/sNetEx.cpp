#include "sNetEx.hpp"

#include <iostream>

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
                .size = 16u + sizeof(SCWorld),
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
    session.enqueuePacket(
        Packet{
            .size = 16u + sizeof(SCWorld),
            .type = PacketType::SCWorld,
            .scWorld = SCWorld {
                .netId = netId_,
                .xform = {
                    .translation = mu::Vec3(pCoord->get().xform().row(3)),
                    .rotation = mu::NQuat()
                }
            }
        }
    );
}

void SNetExAI::processPacket(const Packet& packet) {

}