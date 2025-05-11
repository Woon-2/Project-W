#ifndef __SESSION_HPP
#define __SESSION_HPP

#include "net/netLow.hpp"
#include "net/protocol.hpp"

#include "ecs.hpp"

#include <iostream>
#include <deque>
#include <vector>
#include <array>
#include <memory>
#include <set>

class Session {
public:
    static constexpr std::size_t recvBufSize = 40960u;

    using PacketProcessor = void(*)(Packet& packet, Session& session);

    Session()
        : recvBuf_{}, sendQueue_{}, sock_(INVALID_SOCKET), id_(-1)
        , recvBytesRemain_(0), packetProcessor_(nullptr),
        entityId_(-1u) {}

    ~Session() = default;

    Session(net::TcpSocket&& sock);
    Session(net::TcpSocket&& sock, std::uint32_t id)
        : recvBuf_{}, sendQueue_{}, sock_(std::move(sock)), id_(id)
        , recvBytesRemain_(0), packetProcessor_(nullptr),
        entityId_(-1u) {}

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    Session(Session&& rhs) noexcept;
    Session& operator=(Session&& other) noexcept;

    net::TcpSocket& sock() noexcept { return sock_; }
    const net::TcpSocket& sock() const noexcept { return sock_; }
    std::uint32_t id() const noexcept { return id_; }

    void setId(std::uint32_t id) noexcept {
        id_ = id;
    }

    void enqueuePacket(const Packet& packet) {
        sendQueue_.push_back(packet);
    }

    void flushPackets();

    void recvPackets();

    void delayPacket(const Packet& packet) {
        delayedQueue_.push_back(packet);
    }

    auto& getDelayedQueue() noexcept {
        return delayedQueue_;
    }

    bool operator==(const Session& other) const noexcept {
        return id_ == other.id_;
    }

    void setPacketProcessor(PacketProcessor processor) noexcept {
        packetProcessor_ = processor;
    }

    void setEntityId(ecs::Entity::ID entityId) noexcept {
        entityId_ = entityId;
    }
    ecs::Entity::ID getEntityId() const noexcept {
        return entityId_;
    }

private:
    std::array< char, recvBufSize > recvBuf_;
    std::deque<Packet> sendQueue_;
    std::deque<Packet> delayedQueue_;
    net::TcpSocket sock_;
    std::uint32_t id_;
    std::uint32_t recvBytesRemain_;
    PacketProcessor packetProcessor_;
    ecs::Entity::ID entityId_;
};


#endif  // __SESSION_HPP