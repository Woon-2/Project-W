#include "net/session.hpp"

#include <iostream>

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