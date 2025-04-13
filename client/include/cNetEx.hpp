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
    static constexpr auto slotKeyTexture = "texture";
    static constexpr auto slotKeyTexArray = "texarray";
    static constexpr auto slotKeyTexCube = "texcube";
    static constexpr auto slotKeyModel = "model";
    static constexpr auto slotKeyBVHPath = "bvhpath";
    static constexpr auto slotKeySkeleton = "skeleton";
    static constexpr auto slotKeyAnimClip = "animclip";

    CNetExSystem() = default;
    CNetExSystem(Session* pSession, gfx::d3d12::ResourceStorage* pResStorage = nullptr)
        : createdEntities_(), netIdToNetEx_(), pResStorage_(pResStorage), pSession_(pSession),
        initPacketCnt_(-1u), recvdInitPacketCnt_(0u) {}

    void linkResStorage(gfx::d3d12::ResourceStorage* pResStorage) noexcept {
        pResStorage_ = pResStorage;
    }
    void preUpdate();
    void postUpdate();
    std::vector<ecs::Entity>&& retreiveCreatedEntities() noexcept {
        return std::move(createdEntities_);
    }

    bool hasInitialized() const noexcept {
        return initPacketCnt_ == recvdInitPacketCnt_;
    }

private:
    void processPacket(const Packet& packet);
    void handleSCInitInfo(const SCInitInfo& scInitInfo);
    void handleSCInitCreate(const SCInitCreate& scCreate);
    void handleSCInitAssign(const SCInitAssign& scAssign);
    void handleSCWorld(const SCWorld& scWorld, const Packet& originalPacket);

    std::vector<ecs::Entity> createdEntities_;
    std::map<std::uint32_t, NetEx*> netIdToNetEx_;
    gfx::d3d12::ResourceStorage* pResStorage_;
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