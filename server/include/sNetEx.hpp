#ifndef __SERVER_NETEX_HPP
#define __SERVER_NETEX_HPP

#include "ecs.hpp"

#include "net/session.hpp"
#include "game/level.hpp"

#include <vector>
#include <map>
#include <optional>
#include <list>

class SNetExSystem : public ecs::System<NetEx> {
public:
    SNetExSystem() = default;

    SNetExSystem(std::vector<Session*>&& pSessions)
        : netIdToNetEx_(), pSessions_(std::move(pSessions)) {}

    void addEntity(ecs::Entity& entity);
    void addSession(Session& session);

    void preUpdate();
    void postUpdate();

private:
    void processPacket(const Packet& packet);

    std::map<std::uint32_t, NetEx*> netIdToNetEx_;
    std::vector<Session*> pSessions_;
    std::list<NetEx*> freeHelicopters_; 
};

class SNetExHelicopter : public NetExProcessorBase {
public:
    SNetExHelicopter(ecs::Entity::ID entityId)
        : NetExProcessorBase(entityId) {}

    void generatePackets(Session& session) override;
    void processPacket(const Packet& packet) override;

private:
    void handleCSWorld(const CSWorld& csWorld);

    std::optional<CSWorld> lastReceivedWorld_;
};

class SNetExAI : public NetExProcessorBase {
public:
    SNetExAI(ecs::Entity::ID entityId)
        : NetExProcessorBase(entityId) {}

    void generatePackets(Session& session) override;
    void processPacket(const Packet& packet) override;

private:
    void handleCSWorld(const CSWorld& csWorld);
};

#endif // __SERVER_NETEX_HPP