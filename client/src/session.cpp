#include "session.hpp"

#include "game/level.hpp"

#include <numeric>
#include <iostream>
#include <vector>

Session::Session(net::TcpSocket&& sock)
    : recvBuf_{}, sendQueue_{}, sock_(std::move(sock)), id_(0)
    , recvBytesRemain_(0), recvOffset_(0), packetProcessor_(nullptr),
    entityId_(-1u) {}

Session::Session(Session&& rhs) noexcept
    : recvBuf_(std::move(rhs.recvBuf_))
    , sendQueue_(std::move(rhs.sendQueue_))
    , sock_(std::move(rhs.sock_))
    , id_(std::exchange(rhs.id_, -1))
    , recvBytesRemain_(std::exchange(rhs.recvBytesRemain_, 0))
    , recvOffset_(std::exchange(rhs.recvOffset_, 0)),
    packetProcessor_(rhs.packetProcessor_),
    entityId_(rhs.entityId_) {}

Session& Session::operator=(Session&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    recvBuf_ = std::move(other.recvBuf_);
    sendQueue_ = std::move(other.sendQueue_);
    sock_ = std::move(other.sock_);
    id_ = std::exchange(other.id_, -1);
    recvBytesRemain_ = std::exchange(other.recvBytesRemain_, 0);
    recvOffset_ = std::exchange(other.recvOffset_, 0);
    packetProcessor_ = other.packetProcessor_;
    entityId_ = other.entityId_;

    return *this;
}

void Session::flushPackets() {
    auto wsaBufs = std::vector<WSABUF>(sendQueue_.size());

    if (sendQueue_.empty()) {
        return;
    }

    for (std::size_t i = 0u; i < sendQueue_.size(); ++i) {
        auto& packet = sendQueue_[i];

        wsaBufs[i] = WSABUF{
            .len = packet.size,
            .buf = reinterpret_cast<char*>(&packet)
        };
    }

    if (!sock_.WSASendUc(wsaBufs)) {
        std::cerr << "Failed to send data\n";
    }
    sendQueue_.clear();
}

void Session::recvPackets() {
    auto wsaBuf = WSABUF{
        .len = static_cast<ULONG>(recvBufSize - recvOffset_),
        .buf = recvBuf_.data() + recvOffset_
    };

    auto recvSize = sock_.WSARecvUc(std::views::single(wsaBuf));

    if (!recvSize) { return; }

    std::cout << "Received " << recvSize.value() << " bytes\n";
    auto readOffset = 0u;

    // recvOffset_ + recvSize.value() represents
    // previously received bytes + currently received bytes
    while (recvOffset_ + recvSize.value() >= sizeof(std::uint16_t)) {  // at least it should be able to receive packet size
        const auto requiredSize = reinterpret_cast<const Packet*>(recvBuf_.data() + readOffset)->size;

        if (recvOffset_ + recvSize.value() < requiredSize) {
            // packet fragmentized, reassemble required.
            recvBytesRemain_ = requiredSize - (recvOffset_ + recvSize.value());
            std::memmove(recvBuf_.data() + recvOffset_, recvBuf_.data() + recvOffset_ + readOffset, recvSize.value());
            recvOffset_ += recvSize.value();
            return;
        }

        (*packetProcessor_)(*reinterpret_cast<Packet*>(recvBuf_.data() + readOffset), *this);

        readOffset += requiredSize;
        *recvSize -= (requiredSize - recvOffset_);
        recvOffset_ = 0u;
    }
}