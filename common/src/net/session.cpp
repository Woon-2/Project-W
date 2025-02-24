#include "net/session.hpp"

#include <numeric>
#include <iostream>

void IDPool::initList() {
    idList_.resize(maxConnection);
    std::iota(idList_.begin(), idList_.end(), 0u);
}

std::optional<std::uint16_t> IDPool::allocID() {
    if(idList_.empty()){
        return std::nullopt;
    }

    auto id = idList_.front();
    idList_.pop_front();

    return id;
}

Session::~Session() {
    if (id_ != -1) {
        IDPool::deallocID(id_);
        if (sock_ != INVALID_SOCKET) {
            closesocket(sock_);
        }
    }
}

Session::Session(SOCKET sock)
    : sock_(sock), id_(-1) {
    if (auto allocatedId = IDPool::allocID()) {
        id_ = allocatedId.value();
    }
}

Session::Session(Session&& rhs) noexcept
    : sock_(std::exchange(rhs.sock_, INVALID_SOCKET))
    , id_(std::exchange(rhs.id_, -1)) {}

Session& Session::operator=(Session&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    sock_ = std::exchange(other.sock_, INVALID_SOCKET);
    id_ = std::exchange(other.id_, -1);

    return *this;
}

void IDPool::deallocID(std::uint16_t id) {
    idList_.push_front(id);
}

void Session::flushPackets() {
    while (!sendQueue_.empty()) {
        auto& packet = sendQueue_.front();
        auto sendSize = ::send(sock_, reinterpret_cast<const char*>(&packet), sizeof(Packet), 0);
        if (sendSize == SOCKET_ERROR) {
            std::cerr << "send failed\n";
            break;
        }

        sendQueue_.pop_front();
    }
}

std::forward_list<std::uint16_t> IDPool::idList_;