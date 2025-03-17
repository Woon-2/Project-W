#ifndef __CLIENT_NETEX_HPP
#define __CLIENT_NETEX_HPP

#include "ecs.hpp"

#include "net/session.hpp"
#include "game/level.hpp"

#include "d3d12engine/d3d12Engine.hpp"

#include <vector>
#include <map>
#include <optional>

class CNetExSystem : public ecs::System<NetEx> {
public:
    CNetExSystem() = default;
    CNetExSystem(Session* pSession)
        : createdEntities_(), netIdToNetEx_(), pSession_(pSession) {}

    void preUpdate(const gfx::d3d12engine::Core& core);
    void postUpdate();
    std::vector<ecs::Entity>&& retreiveCreatedEntities() noexcept {
        return std::move(createdEntities_);
    }

    std::optional<ecs::Entity::ID> playerID_ = std::nullopt;

private:
    void processPacket(const Packet& packet, const gfx::d3d12engine::Core& core);
    void handleSCCreate(const SCCreate& scCreate, const gfx::d3d12engine::Core& core);
    void handleSCWorld(const SCWorld& scWorld, const Packet& originalPacket);
    void handleSCAssign(const SCAssign& scAssign);

    std::vector<ecs::Entity> createdEntities_;
    std::map<std::uint32_t, NetEx*> netIdToNetEx_;
    Session* pSession_;
};

class CNetExHelicopter : public INetExProcessor {
public:
    CNetExHelicopter(ecs::Entity::ID entityID) : entityID_(entityID) {}

    void generatePackets(Session& session) override;
    void processPacket(const Packet& packet) override;

private:
    void handleSCWorld(const SCWorld& scWorld);

    ecs::Entity::ID entityID_;
};

class CNetExTree0 : public INetExProcessor {
public:
    void generatePackets(Session& session) override {}
    void processPacket(const Packet& packet) override {}
};

class CNetExTree1 : public INetExProcessor {
public:
    void generatePackets(Session& session) override {}
    void processPacket(const Packet& packet) override {}
};

class CNetExTree2 : public INetExProcessor {
public:
    void generatePackets(Session& session) override {}
    void processPacket(const Packet& packet) override {}
};

#endif  // __CLIENT_NETEX_HPP