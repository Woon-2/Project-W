#ifndef __SERVER_NETEX_HPP
#define __SERVER_NETEX_HPP

#include "ecs.hpp"

#include "net/session.hpp"
#include "game/level.hpp"

#include <vector>
#include <map>
#include <optional>

class SNetExSystem : public ecs::System<NetEx> {
public:
    SNetExSystem() = default;

    SNetExSystem(std::vector<Session*>&& pSessions)
        : netIdToNetEx_(), pSessions_(std::move(pSessions)) {}

    void addEntity(ecs::Entity& entity) {
        ecs::System<NetEx>::addEntity(entity);
        auto& netEx = entity.as<NetEx>();
        netIdToNetEx_[netEx.id()] = &netEx;
    }

    void addSession(Session& session) {
        pSessions_.push_back(&session);

        for (auto& pNetEx : components<NetEx>()) {
            auto pCoord = gameEngine::Coord::at(pNetEx->entityID().value());

            auto translation = pCoord->get().xform().row(3);

            session.enqueuePacket(
                Packet{
                    .size = 16u + sizeof(SCCreate),
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
            session.enqueuePacket(
                Packet{
                    .size = 16u + sizeof(SCAssign),
                    .type = PacketType::SCAssign,
                    .scAssign = SCAssign{
                        .netId = pNetEx->id()
                    }
                }
            );
        }
    }

    void preUpdate();
    void postUpdate();

private:
    void processPacket(const Packet& packet);

    std::map<std::uint32_t, NetEx*> netIdToNetEx_;
    std::vector<Session*> pSessions_;
};

class SNetExHelicopter : public INetExProcessor {
public:
    SNetExHelicopter(ecs::Entity::ID entityID) : lastReceivedWorld_{}, entityID_(entityID) {}

    void generatePackets(Session& session) override;
    void processPacket(const Packet& packet) override;

private:
    void handleCSWorld(const CSWorld& csWorld);

    std::optional<CSWorld> lastReceivedWorld_;
    ecs::Entity::ID entityID_;
};

class SNetExAI : public INetExProcessor {
public:
    SNetExAI(ecs::Entity::ID entityID, std::uint32_t netId) : entityID_(entityID), netId_(netId) {}

    void generatePackets(Session& session) override;
    void processPacket(const Packet& packet) override;

private:
    void handleCSWorld(const CSWorld& csWorld);

    ecs::Entity::ID entityID_;
    std::uint32_t netId_;
};

#endif // __SERVER_NETEX_HPP