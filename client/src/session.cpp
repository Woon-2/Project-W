#include "session.hpp"

#include "game/level.hpp"

#include <numeric>
#include <iostream>
#include <vector>

Session::Session(net::TcpSocket&& sock)
    : recvBuf_{}, sendQueue_{}, sock_(std::move(sock)), id_(0)
    , recvBytesRemain_(0), packetProcessor_(nullptr),
    entityId_(-1u) {}

Session::Session(Session&& rhs) noexcept
    : recvBuf_(std::move(rhs.recvBuf_))
    , sendQueue_(std::move(rhs.sendQueue_))
    , sock_(std::move(rhs.sock_))
    , id_(std::exchange(rhs.id_, -1))
    , recvBytesRemain_(std::exchange(rhs.recvBytesRemain_, 0))
    , packetProcessor_(rhs.packetProcessor_),
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
        .len = static_cast<ULONG>(recvBufSize - recvBytesRemain_),
        .buf = recvBuf_.data() + recvBytesRemain_
    };

    auto recvSize = sock_.WSARecvUc(std::views::single(wsaBuf));

    if (!recvSize) { return; }

    auto readOffset = 0u;
    recvBytesRemain_ += static_cast<std::uint32_t>( recvSize.value( ) );
    
    while ( recvBytesRemain_ >= sizeof( std::uint16_t ) ) {
        auto packetSize = reinterpret_cast<Packet*>( recvBuf_.data( ) + readOffset )->size;

        if ( recvBytesRemain_ < packetSize ) {
            break;
        }

        auto packet = *reinterpret_cast<Packet*>( recvBuf_.data( ) + readOffset );
        ( *packetProcessor_ )( packet, *this );
        readOffset += packetSize;
        recvBytesRemain_ -= packetSize;
    }

    if ( readOffset > 0 && recvBytesRemain_ > 0 ) {
        std::memcpy( recvBuf_.data( ), recvBuf_.data( ) + readOffset, recvBytesRemain_ );
    }
}