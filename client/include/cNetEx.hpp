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
        : createdEntities_(), netIdToNetEx_(), pSession_(pSession),
        initPacketCnt_(-1u), recvdInitPacketCnt_(0u) {}

    void preUpdate(const gfx::d3d12engine::Core& core);
    void postUpdate();
    std::vector<ecs::Entity>&& retreiveCreatedEntities() noexcept {
        return std::move(createdEntities_);
    }

    bool hasInitialized() const noexcept {
        return initPacketCnt_ == recvdInitPacketCnt_;
    }

    std::optional<ecs::Entity::ID> playerID_ = std::nullopt;

private:
    void processPacket(const Packet& packet, const gfx::d3d12engine::Core& core);
    void handleSCInitInfo(const SCInitInfo& scInitInfo);
    void handleSCInitCreate(const SCInitCreate& scCreate, const gfx::d3d12engine::Core& core);
    void handleSCInitAssign(const SCInitAssign& scAssign);
    void handleSCWorld(const SCWorld& scWorld, const Packet& originalPacket);

    std::vector<ecs::Entity> createdEntities_;
    std::map<std::uint32_t, NetEx*> netIdToNetEx_;
    Session* pSession_;
    std::uint32_t initPacketCnt_;
    std::uint32_t recvdInitPacketCnt_;
};

class CNetExHelicopter : public NetExProcessorBase {
public:
    CNetExHelicopter(ecs::Entity::ID entityId)
        : NetExProcessorBase(entityId) {}

    void generatePackets(Session& session) override;
    void processPacket(const Packet& packet) override;

private:
    void handleSCWorld(const SCWorld& scWorld);
};

class CNetExTree0 : public NetExProcessorBase {
public:
    CNetExTree0(ecs::Entity::ID entityId)
        : NetExProcessorBase(entityId) {}

    void generatePackets(Session& session) override {}
    void processPacket(const Packet& packet) override {}
};

class CNetExTree1 : public NetExProcessorBase {
public:
    CNetExTree1(ecs::Entity::ID entityId)
        : NetExProcessorBase(entityId) {}

    void generatePackets(Session& session) override {}
    void processPacket(const Packet& packet) override {}
};

class CNetExTree2 : public NetExProcessorBase {
public:
    CNetExTree2(ecs::Entity::ID entityId)
        : NetExProcessorBase(entityId) {}

    void generatePackets(Session& session) override {}
    void processPacket(const Packet& packet) override {}
};

#endif  // __CLIENT_NETEX_HPP