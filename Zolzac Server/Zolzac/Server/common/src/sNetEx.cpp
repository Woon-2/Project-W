#include "sNetEx.hpp"

#include <iostream>

void SNetExSystem::addEntity(ecs::Entity& entity) {
    ecs::System<NetEx>::addEntity(entity);
    auto& netEx = entity.as<NetEx>();

    if ( typeid(*netEx.getProcessor()) == typeid(SNetExHelicopter) ) {
        freeHelicopters_.push_back(&netEx);
    }

    netIdToNetEx_[netEx.netId()] = &netEx;
}

void SNetExSystem::addSession(Session& session) {
	session.setNetSystem( this );
    {
        std::lock_guard<std::mutex> lock( addSessionLock_ );
        pReservedSessions_.push_back( &session );
    }

    session.enqueuePacket(
        Packet{
            .size = calcPacketSize<SCInitInfo>(),
            .type = PacketType::SC_InitInfo,
            .scInitInfo = SCInitInfo{
                // N SCInitCreate Packet + 1 SCInitAssign Packet
                .packetCnt = static_cast<std::uint32_t>(components<NetEx>().size() + 1u)
            }
        }
    );

    for (auto& pNetEx : components<NetEx>()) {
        auto pCoord = gameEngine::Coord::at(pNetEx->entityID().value());

        auto translation = pCoord->get().xform().row(3);

        session.enqueuePacket(
            Packet{
                .size = calcPacketSize<SCCreate>(),
                .type = PacketType::SC_InitCreate,
                .scInitCreate = SCCreate{
                    .netId = pNetEx->netId(),
                    .objType = ObjectType::Character,
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
            .size = calcPacketSize<SCInitAssign>( ),
            .type = PacketType::SC_InitAssign,
            .scInitAssign = SCInitAssign{
				.netId = pNetEx->netId( )
            }
        }
    );
}

void SNetExSystem::preUpdate( ) {
	std::lock_guard<std::mutex> lock( addSessionLock_ );
    for ( auto& pSession : pReservedSessions_ ) {
		pSessions_.push_back( pSession );
    }
    pReservedSessions_.clear( );
}

void SNetExSystem::postUpdate() {
    for (auto& pSession : pSessions_) {
        if ( pSession->getAcceptFlag( ) ) {
            for ( auto& pNetEx : components<NetEx>( ) ) {
                pNetEx->generatePackets( *pSession );
            }
        }
    }
}

void SNetExSystem::processPacket(const Packet& packet) {
    switch (packet.type) {
    case PacketType::CS_World:
        if (!netIdToNetEx_.contains(packet.csWorld.netId)) {
            std::cerr << "NetEx not found for netId: " << packet.csWorld.netId << '\n';
            return;
        }
        netIdToNetEx_.at(packet.csWorld.netId)->processPacket(packet);
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
                .type = PacketType::SC_World,
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
    case PacketType::CS_World:
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
    auto pCoord = gameEngine::Coord::at(entityID());

    const auto translation = pCoord->get().xform().row(3);

    session.enqueuePacket(
        Packet{
            .size = calcPacketSize<SCWorld>(),
            .type = PacketType::SC_World,
            .scWorld = SCWorld {
                .netId = netId(),
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