#ifndef __SESSION_HPP
#define __SESSION_HPP

#include "net/netLow.hpp"
#include "net/protocol.hpp"

#include "ecs.hpp"
#include "IDPool.hpp"

#include <iostream>
#include <deque>
#include <vector>
#include <array>
#include <memory>
#include <set>

class Session {
public:
    static constexpr std::size_t recvBufSize = 40960u;

    Session()
        : recvBuf_{}, sendQueue_{}, recvQueue_{}, sock_(INVALID_SOCKET), id_(-1)
        , recvBytesRemain_(0), recvOffset_(0) {}

    ~Session();

    Session(net::TcpSocket&& sock);
    Session(net::TcpSocket&& sock, std::uint32_t id)
        : recvBuf_{}, sendQueue_{}, recvQueue_{}, sock_(std::move(sock)), id_(id)
        , recvBytesRemain_(0), recvOffset_(0) {}

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
};


#endif  // __SESSION_HPP