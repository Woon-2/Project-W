#include "net/session.hpp"

#include "game/level.hpp"

#include <numeric>
#include <iostream>
#include <vector>

void IDPool::initList() {
    idList_.resize(0xFFFF);
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
    }
}

Session::Session(net::TcpSocket&& sock)
    : recvBuf_{}, sendQueue_{}, recvQueue_{}, sock_(std::move(sock)), id_(-1)
    , recvBytesRemain_(0), recvOffset_(0), readOffset_(0) {
    if (auto allocatedId = IDPool::allocID()) {
        id_ = allocatedId.value();
    }
}

Session::Session(Session&& rhs) noexcept
    : recvBuf_(std::move(rhs.recvBuf_))
    , sendQueue_(std::move(rhs.sendQueue_))
    , recvQueue_(std::move(rhs.recvQueue_))
    , sock_(std::move(rhs.sock_))
    , id_(std::exchange(rhs.id_, -1))
    , recvBytesRemain_(std::exchange(rhs.recvBytesRemain_, 0))
    , recvOffset_(std::exchange(rhs.recvOffset_, 0))
    , readOffset_(std::exchange(rhs.readOffset_, 0)) {}

Session& Session::operator=(Session&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    recvBuf_ = std::move(other.recvBuf_);
    sendQueue_ = std::move(other.sendQueue_);
    recvQueue_ = std::move(other.recvQueue_);
    sock_ = std::move(other.sock_);
    id_ = std::exchange(other.id_, -1);
    recvBytesRemain_ = std::exchange(other.recvBytesRemain_, 0);
    recvOffset_ = std::exchange(other.recvOffset_, 0);
    readOffset_ = std::exchange(other.readOffset_, 0);

    return *this;
}

void IDPool::deallocID(std::uint16_t id) {
    idList_.push_front(id);
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
        .len = recvBufSize,
        .buf = recvBuf_.data() + recvOffset_
    };

    auto recvSize = sock_.WSARecvUc(std::views::single(wsaBuf));

    if (!recvSize) { return; }

    while (recvSize.value() > 0) {
        if (recvBytesRemain_ > 0) {
            if (recvSize.value() >= recvBytesRemain_) {
                auto packet = Packet{};    
                std::memcpy(&packet, recvBuf_.data(), reinterpret_cast<Packet*>(recvBuf_.data())->size);
                
                recvQueue_.push_back(packet);
                readOffset_ = reinterpret_cast<Packet*>(recvBuf_.data())->size;
                recvOffset_ = 0;
                *recvSize -= recvBytesRemain_;
                recvBytesRemain_ = 0;
            }
            else {
                recvOffset_ += recvSize.value();
                recvBytesRemain_ -= recvSize.value();
                *recvSize = 0;
            }
        }
        else {
            const auto requiredSize = reinterpret_cast<Packet*>(recvBuf_.data() + readOffset_)->size;
            if (recvSize.value() >= requiredSize) {
                auto packet = Packet{};
                std::memcpy(&packet, recvBuf_.data() + readOffset_, requiredSize);
                
                recvQueue_.push_back(packet);
                *recvSize -= requiredSize;
                readOffset_ += requiredSize;
            }
            else {
                std::memcpy(recvBuf_.data(), recvBuf_.data() + readOffset_, recvSize.value());
                recvOffset_ = recvSize.value();
                recvBytesRemain_ = reinterpret_cast<Packet*>(recvBuf_.data())->size - recvSize.value();
                *recvSize = 0;
                readOffset_ = 0;
            }
        }
    }

    readOffset_ = 0;
}


// ['H', 'e', 'l', 'l', 'o', 'w', 'o', 'r', 'l', 'd']
//   0    1    2    3    4    5    6
// "Hel"
// offset: 3
// WSARecv: recvBuf.data() + offset

std::forward_list<std::uint16_t> IDPool::idList_;