#ifndef __SESSION_HPP
#define __SESSION_HPP

#include "net/netInclude.hpp"
#include "net/protocol.hpp"

#include <cstdint>
#include <deque>
#include <forward_list>
#include <optional>

class IDPool {
public:
    static void initList();

    static std::optional<std::uint16_t> allocID();

    static void deallocID(std::uint16_t id);
    
    private:
    static std::forward_list<std::uint16_t> idList_;
};

class Session {
public:
    Session()
        : sock_(INVALID_SOCKET), id_(-1) {}

    ~Session();

    Session(SOCKET sock);
    Session(SOCKET sock, std::uint32_t id)
        : sock_(sock), id_(id) {}

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    Session(Session&& rhs) noexcept;
    Session& operator=(Session&& other) noexcept;

    SOCKET sock() const noexcept { return sock_; }
    std::uint32_t id() const noexcept { return id_; }

    void setId(std::uint32_t id) noexcept {
        id_ = id;
    }

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