#ifndef __SESSION_HPP
#define __SESSION_HPP

#include "net/netLow.hpp"
#include "net/protocol.hpp"

#include "ecs.hpp"

#include <iostream>
#include <cstdint>
#include <deque>
#include <forward_list>
#include <optional>
#include <vector>
#include <array>
#include <memory>
#include <set>

class IDPool {
public:
    static void initList();

    static std::optional<std::uint16_t> allocID();

    static void deallocID(std::uint16_t id);
    
    private:
    static std::forward_list<std::uint16_t> idList_;
};

class NetExProcessorBase {
public:
    NetExProcessorBase(ecs::Entity::ID entityID)
        : entityID_(entityID), netId_(-1u) {
        if (auto allocatedId = IDPool::allocID(); !allocatedId) {
            std::cerr << "Failed to allocate netId\n";
        }
        else {
            netId_ = allocatedId.value();
        }
    }

    virtual ~NetExProcessorBase() {
        if (netId_ != -1u) {
            IDPool::deallocID(netId_);
        }
    }

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

private:
    std::unique_ptr<NetExProcessorBase> pProcessor_;
};

class Session {
public:
    static constexpr std::size_t recvBufSize = 40960u;

    Session()
        : recvBuf_{}, sendQueue_{}, recvQueue_{}, sock_(INVALID_SOCKET), id_(-1)
        , recvBytesRemain_(0), recvOffset_(0), readOffset_(0) {}

    ~Session();

    Session(net::TcpSocket&& sock);
    Session(net::TcpSocket&& sock, std::uint32_t id)
        : recvBuf_{}, sendQueue_{}, recvQueue_{}, sock_(std::move(sock)), id_(id)
        , recvBytesRemain_(0), recvOffset_(0), readOffset_(0) {}

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

    bool operator==(const Session& other) const noexcept {
        return id_ == other.id_;
    }

    std::deque<Packet>& getRecvQueue() noexcept {
        return recvQueue_;
    }

private:
    std::array< char, recvBufSize > recvBuf_;
    std::deque<Packet> sendQueue_;
    std::deque<Packet> recvQueue_;
    net::TcpSocket sock_;
    std::uint32_t id_;
    std::uint32_t recvBytesRemain_;
    std::uint32_t recvOffset_;
    std::uint32_t readOffset_;
};


#endif  // __SESSION_HPP