#ifndef __netEx_HPP
#define __netEx_HPP

#include "stdafx.hpp"

#include "ecs.hpp"

#include "net/protocol.hpp"

enum class NetExCategory {
    Player = 0x01,
    AI = 0x02
};

class NetExProcessorBase {
public:
    NetExProcessorBase(ecs::Entity::ID entityID, u32t netId)
        : entityID_(entityID), netId_(-netId) {}

    virtual void generatePackets(class Session& session) = 0;
    virtual void processPacket(const Packet& packet) = 0;

    ecs::Entity::ID entityID() const noexcept {
        return entityID_;
    }

    std::uint32_t netId() const noexcept {
        return netId_;
    }

    void addCategory(NetExCategory category) {
        categories_.insert(category);
    }
    bool hasCategory(NetExCategory category) const {
        return categories_.contains(category);
    }

private:
    std::set<NetExCategory> categories_;
    ecs::Entity::ID entityID_;
    std::uint32_t netId_;
};

class NetEx : public ecs::Component {
public:
    ENABLE_COMPONENT(NetEx);

    NetEx( const ecs::Entity& entity, std::unique_ptr<NetExProcessorBase>&& pNetExProcessor) NOEXCEPT
        : ecs::Component(entity), pProcessor_(std::move(pNetExProcessor)) {}

    void generatePackets(Session& session) {
        pProcessor_->generatePackets(session);
    }
    void processPacket(const Packet& packet) {
        pProcessor_->processPacket(packet);
    }

    std::uint32_t netId() const {
        return pProcessor_->netId();
    }

    NetExProcessorBase* getProcessor() noexcept {
        return pProcessor_.get();
    }

    void addCategory(NetExCategory category) {
        pProcessor_->addCategory(category);
    }

    bool hasCategory(NetExCategory category) const {
        return pProcessor_->hasCategory(category);
    }

private:
    std::unique_ptr<NetExProcessorBase> pProcessor_;
};

#endif // __netEx_HPP