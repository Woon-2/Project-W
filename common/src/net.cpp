#ifndef __NET_H__
#define __NET_H__

#include "net.hpp"

namespace net {

Exception::Exception(int line, const char* file, int error, std::string_view desc) NET_NOEXCEPT
    : Woon2Exception(line, file), error_(error) {
    whatBuffer_ += "[Error: " + std::to_string(error_) + "] "
        + desc.data() + "\n\n";

    whatBuffer_ += std::system_category().message(error_);
}

void initNet() {
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw NET_LAST_EXCEPT("Failed to initialize Winsock!");
    }
    else {
        OutputDebugStringA("Winsock initialized!\n"s.c_str());
        OutputDebugStringA(("status: "s + wsaData.szSystemStatus + "\n"s).c_str());
        OutputDebugStringA(("description: "s + wsaData.szDescription + "\n"s).c_str());
    }
}

void relNet() {
    auto err = WSACleanup();
    if (err != 0) {
        throw NET_LAST_EXCEPT("Failed to clean up Winsock!");
    }
    else {
        OutputDebugStringA("Winsock cleaned up!\n"s.c_str());
    }
}

const AddrInfo getAddrInfo(std::string_view host, std::string_view serv, const AddrInfo::Hint& hint) {
    auto nativeHint = addrinfo{
        .ai_flags = hint.ai_flags,
        .ai_family = hint.ai_family,
        .ai_socktype = hint.ai_socktype,
        .ai_protocol = hint.ai_protocol
    };

    addrinfo* info = nullptr;
    if (getaddrinfo(host.data(), serv.data(), &nativeHint, &info) != 0) {
        throw NET_LAST_EXCEPT("Failed to get address info!");
    }
    return AddrInfo(info);
}

const AddrInfo getAddrInfo(std::string_view host, Port port, const AddrInfo::Hint& hint) {
    return getAddrInfo(host, std::to_string(port.get()), hint);
}

}   // namespace net

#endif // __NET_H__