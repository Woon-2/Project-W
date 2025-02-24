#ifndef __SESSION_HPP
#define __SESSION_HPP

#include "net/netInclude.hpp"
#include "net/protocol.hpp"
#include <cstdint>
#include <deque>

class Session {
public:
    Session()
        : sock_(INVALID_SOCKET), id_(-1) {}

    ~Session();

    Session(SOCKET sock);

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    Session(Session&& rhs) noexcept;
    Session& operator=(Session&& other) noexcept;

    SOCKET sock() const noexcept { return sock_; }
    std::uint32_t id() const noexcept { return id_; }

    void enqueuePacket(const Packet& packet) {
        sendQueue_.push_back(packet);
    }

    void flushPackets();

    bool operator==(const Session& other) const noexcept {
        return id_ == other.id_;
    }

private:
    std::deque<Packet> sendQueue_;
    SOCKET sock_;
    std::uint32_t id_;
};

#endif  // __SESSION_HPP