#ifndef __netLow_H
#define __netLow_H

#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <string_view>
#include <ranges>
#include <algorithm>
#include <type_traits>
#include <utility>
#include <optional>
#include <functional>
#include <chrono>
#include <optional>

#include <cstdint>

#include "Woon2Exception.hpp"
#include <system_error>

using namespace std::literals;

#define NET_NOEXCEPT noexcept
#define NET_EXCEPT(error, desc) net::Exception(__LINE__, __FILE__, error, desc)
#define NET_LAST_EXCEPT(desc) NET_EXCEPT(WSAGetLastError(), desc)

namespace net {

void initNet();
void relNet();

class Exception : public Woon2Exception {
public:
    Exception(int line, const char* file, int error, std::string_view desc) NET_NOEXCEPT;

    const char* type() const noexcept override {
        return "Network Exception";
    }

    int error() const NET_NOEXCEPT {
        return error_;
    }

private:
    int error_;
};


class Ipv4Addr {
public:
    Ipv4Addr(std::string_view ip = "127.0.0.1"sv)
        : presen_(ip), addr_() {
        if (inet_pton(AF_INET, ip.data(), &addr_) != 1) {
            throw NET_LAST_EXCEPT("Failed to convert IPv4 address"sv);
        }
    }
    
    Ipv4Addr(std::uint32_t addr)
        : presen_(15u, '\0'), addr_(addr) {
        if (inet_ntop(AF_INET, &addr_, presen_.data(), presen_.size()) == nullptr) {
            throw NET_LAST_EXCEPT("Failed to convert IPv4 address"sv);
        }
    }

    Ipv4Addr(const sockaddr& addr) NET_NOEXCEPT
        : presen_(), addr_(reinterpret_cast<const sockaddr_in*>(&addr)->sin_addr.s_addr) {}

    Ipv4Addr(const sockaddr_in& addr) NET_NOEXCEPT
        : addr_(addr.sin_addr.s_addr) {}

    std::string_view presen() const NET_NOEXCEPT {
        return presen_;
    }

    std::uint32_t get() const NET_NOEXCEPT {
        return addr_;
    }

private:
    std::string presen_;
    std::uint32_t addr_;
};

class Port {
public:
    Port(std::uint16_t hostPort = 0) NET_NOEXCEPT : port_(htons(hostPort)) {}

    std::uint16_t get() const NET_NOEXCEPT {
        return port_;
    }

private:
    std::uint16_t port_;
};

class SockAddr {
private:
    union UAddr {
        sockaddr addr;
        sockaddr_in addr_in;

        UAddr() NET_NOEXCEPT = default;

        UAddr(const Ipv4Addr& ip, Port port) NET_NOEXCEPT
            : addr_in{
                .sin_family = static_cast<decltype(sockaddr_in::sin_family)>(AF_INET),
                .sin_port = static_cast<decltype(sockaddr_in::sin_port)>(port.get()),
            } {
            addr_in.sin_addr.s_addr = ip.get();
        }

        UAddr(const sockaddr& addr) NET_NOEXCEPT
            : addr(addr) {}
    } uAddr_;

public:
    SockAddr() NET_NOEXCEPT = default;

    SockAddr(const Ipv4Addr& ip, Port port) NET_NOEXCEPT
        : uAddr_(ip, port) {}

    SockAddr(const sockaddr& addr) NET_NOEXCEPT
        : uAddr_(addr) {}

    UAddr& get() NET_NOEXCEPT {
        return uAddr_;
    }

    const UAddr& get() const NET_NOEXCEPT {
        return uAddr_;
    }

    std::size_t size() const NET_NOEXCEPT {
        return sizeof(uAddr_.addr);
    }
};

class AddrInfo {
public:
    struct Hint {
        int ai_family;
        int ai_socktype;
        int ai_protocol;
        int ai_flags;
    };

    AddrInfo(addrinfo* info) NET_NOEXCEPT
        : info_(info), ptr_(info) {}
    ~AddrInfo() {
        freeaddrinfo(info_);
    }

    AddrInfo& operator++() NET_NOEXCEPT {
        ptr_ = ptr_->ai_next;
        return *this;
    }

    AddrInfo operator++(int) NET_NOEXCEPT {
        AddrInfo temp = *this;
        ptr_ = ptr_->ai_next;
        return temp;
    }

    bool valid() const NET_NOEXCEPT {
        return ptr_->ai_addr;
    }

    bool advancable() const NET_NOEXCEPT {
        return ptr_->ai_next;
    }

    const SockAddr get() const NET_NOEXCEPT {
        return SockAddr(*ptr_->ai_addr);
    }

private:
    addrinfo* info_;
    addrinfo* ptr_;
};

const AddrInfo getAddrInfo(std::string_view host, std::string_view serv, const AddrInfo::Hint& hint);
const AddrInfo getAddrInfo(std::string_view host, Port port, const AddrInfo::Hint& hint);
inline const AddrInfo getAddrInfo(std::string_view host, std::string_view serv) {
    return getAddrInfo(host, serv, AddrInfo::Hint{});
}
inline const AddrInfo getAddrInfo(std::string_view host, Port port) {
    return getAddrInfo(host, port, AddrInfo::Hint{});
}

namespace detail {

class SocketBase {
protected:
    SOCKET sock_;
    bool open_;

public:
    SocketBase(SOCKET sock, bool open = true) NET_NOEXCEPT
        : sock_(sock), open_(open) {}

    ~SocketBase() {
        if (open_) {
            // shutdown(sock_, SD_BOTH);
            closesocket(sock_);
        }
    }

    SocketBase(const SocketBase&) = delete;
    SocketBase& operator=(const SocketBase&) = delete;

    SocketBase(SocketBase&& other) noexcept
        : sock_( std::exchange(other.sock_, INVALID_SOCKET) ),
        open_( std::exchange(other.open_, false) ) {}

    SocketBase& operator=(SocketBase&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        sock_ = std::exchange(other.sock_, INVALID_SOCKET);
        open_ = std::exchange(other.open_, false);

        return *this;
    }

    void bind(const SockAddr& addr) {
        if (::bind(sock_, &addr.get().addr, static_cast<int>(addr.size())) == SOCKET_ERROR) {
            throw NET_LAST_EXCEPT("Failed to bind UDP socket"sv);
        }
    }

    void bindUc(const SockAddr& addr) {
        ::bind(sock_, &addr.get().addr, static_cast<int>(addr.size()));
    }

    void setMode(long cmd, u_long* argp) {
        if (ioctlsocket(sock_, cmd, argp) == SOCKET_ERROR) {
            throw NET_LAST_EXCEPT("Failed to set socket mode"sv);
        }
    }

    void close() {
        if (closesocket(sock_) == SOCKET_ERROR) {
            throw NET_LAST_EXCEPT("Failed to close socket"sv);
        }
        open_ = false;
    }

    bool active() const NET_NOEXCEPT {
        return open_;
    }

    auto operator<=>(const SocketBase& rhs) const NET_NOEXCEPT {
        return sock_ <=> rhs.sock_;
    }

    bool operator==(const SocketBase& rhs) const NET_NOEXCEPT {
        return sock_ == rhs.sock_;
    }

    SOCKET nativeHandle() const NET_NOEXCEPT {
        return sock_;
    }
};

}   // namespace net::detail

class TcpSocket : public detail::SocketBase {
public:
    TcpSocket()
        : SocketBase(createNativeSocket()) {}

    TcpSocket(SOCKET sock, bool bOpen = true) NET_NOEXCEPT
        : SocketBase(sock, bOpen) {}

    void listen(std::size_t backlog = SOMAXCONN) {
        if (::listen(sock_, static_cast<int>(backlog)) == SOCKET_ERROR) {
            throw NET_LAST_EXCEPT("Failed to listen TCP socket"sv);
        }
    }

    void listenUc(std::size_t backlog = SOMAXCONN) {
        ::listen(sock_, static_cast<int>(backlog));
    }

    void connect(const SockAddr& addr) {
        if (::connect(sock_, &addr.get().addr, static_cast<int>(addr.size())) == SOCKET_ERROR) {
            throw NET_LAST_EXCEPT("Failed to connect TCP socket"sv);
        }
    }

    void connectUc(const SockAddr& addr) {
        ::connect(sock_, &addr.get().addr, static_cast<int>(addr.size()));
    }

    template <std::ranges::contiguous_range R>
        requires std::same_as<std::ranges::range_value_t<R>, WSABUF>
    DWORD WSASend(R&& wsaBufs) {
        //std::data(wsaBufs); // 버퍼들 주소
        //std::size(wsaBufs); // 버퍼 개수
        DWORD sent{};
        if (::WSASend(sock_, std::data(wsaBufs), static_cast<DWORD>(std::size(wsaBufs)), &sent, 0, nullptr, nullptr)) {
            throw NET_LAST_EXCEPT("Failed to send data"sv);
        }
        return sent;
    }

    template <std::ranges::contiguous_range R>
        requires std::same_as<std::ranges::range_value_t<R>, WSABUF>
    std::optional<DWORD> WSASendUc(R&& wsaBufs) {
        DWORD sent{};
        if (::WSASend(sock_, std::data(wsaBufs), static_cast<DWORD>(std::size(wsaBufs)), &sent, 0, nullptr, nullptr)) {
            return {};
        }
        return sent;
    }

    template <std::ranges::contiguous_range R>
        requires std::same_as<std::ranges::range_value_t<R>, WSABUF>
    DWORD WSARecv(R&& wsaBufs) {
        DWORD recvSize{};
        DWORD flags{};
        if (::WSARecv(sock_, std::data(wsaBufs), static_cast<DWORD>(std::size(wsaBufs)), &recvSize, &flags, nullptr, nullptr)) {
            throw NET_LAST_EXCEPT("Failed to receive data"sv);
        }
        return recvSize;
    }

    template <std::ranges::contiguous_range R>
        requires std::same_as<std::ranges::range_value_t<R>, WSABUF>
    std::optional<DWORD> WSARecvUc(R&& wsaBufs) {
        DWORD recvSize{};
        DWORD flags{};
        if (::WSARecv(sock_, std::data(wsaBufs), static_cast<DWORD>(std::size(wsaBufs)), &recvSize, &flags, nullptr, nullptr)) {
            return {};
        }
        return recvSize;
    }

    TcpSocket WSAAccept(){
        auto tmp = SockAddr();
        return WSAAccept(tmp);
    }

    TcpSocket WSAAccept(SockAddr& addr) {
        int addrSize = static_cast<int>(addr.size());
        SOCKET client = ::WSAAccept(sock_, &addr.get().addr, &addrSize, nullptr, 0);
        if (client == INVALID_SOCKET) {
            throw NET_LAST_EXCEPT("Failed to accept TCP connection"sv);
        }
        return TcpSocket(client);
    }

    TcpSocket WSAAcceptUc(){
        auto tmp = SockAddr();
        return WSAAcceptUc(tmp);
    }

    TcpSocket WSAAcceptUc(SockAddr& addr) {
        int addrSize = static_cast<int>(addr.size());
        SOCKET client = ::WSAAccept(sock_, &addr.get().addr, &addrSize, nullptr, 0);
        if (client == INVALID_SOCKET) {
            return TcpSocket(INVALID_SOCKET, false);
        }
        return TcpSocket(client);
    }

    void open() {
        if (open_) {
            throw NET_EXCEPT(WSAEISCONN, "Socket is already open"sv);
        }

        sock_ = createNativeSocket();
        open_ = true;
    }

    void setSockOpt(int level, int optname, const char* optval, int optlen) {
        if (setsockopt(sock_, level, optname, optval, optlen) == SOCKET_ERROR) {
            throw NET_LAST_EXCEPT("Failed to set socket option"sv);
        }
    }

    auto operator<=>(const TcpSocket& rhs) const NET_NOEXCEPT {
        return this->SocketBase::operator<=>(rhs);
    }

    bool operator==(const TcpSocket& rhs) const NET_NOEXCEPT {
        return this->SocketBase::operator==(rhs);
    }

private:
    static SOCKET createNativeSocket() {
        auto ret = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
        if (ret == INVALID_SOCKET) {
            throw NET_LAST_EXCEPT("Failed to create TCP socket"sv);
        }
        return ret;
    }
};

inline void enNonb(TcpSocket& sock) {
    u_long mode = 1;
    sock.setMode(FIONBIO, &mode);
}

inline void disNonb(TcpSocket& sock) {
    u_long mode = 0;
    sock.setMode(FIONBIO, &mode);
}

namespace detail {
    template <std::ranges::range R>
    void fillFDSet(fd_set& set, const R& socks) {
        FD_ZERO(&set);
        for (auto& sock : socks) {
            FD_SET(sock->nativeHandle(), &set);
        }
    }

    template <std::ranges::range R>
    void retreiveFDSet(const fd_set& set, const R& baseRange, R& outRange) {
        std::ranges::copy_if(baseRange, std::back_inserter(outRange),
            [&set](const auto& sock) {
                return FD_ISSET(sock->nativeHandle(), &set);
            }
        );
    }

}   // namespace net::detail

template <std::ranges::range R>
[[maybe_unused]] std::size_t select( std::optional< std::reference_wrapper<const R> > inRead,
    std::optional< std::reference_wrapper<const R> > inWrite,
    std::optional< std::reference_wrapper<const R> > inExcept,
    std::optional< std::reference_wrapper<R> > outRead,
    std::optional< std::reference_wrapper<R> > outWrite,
    std::optional< std::reference_wrapper<R> > outExcept,
    std::optional< std::chrono::microseconds > timeout
) {
    fd_set readSet, writeSet, exceptSet;

    auto tv = timeval{};

    if (timeout) {
        tv.tv_sec = static_cast<long>( timeout->count() / 1'000'000 );
        tv.tv_usec = static_cast<long>( timeout->count() % 1'000'000 );
    }

    bool inReadValid = inRead && (inRead.value().get().begin() != inRead.value().get().end());
    bool inWriteValid = inWrite && (inWrite.value().get().begin() != inWrite.value().get().end());
    bool inExceptValid = inExcept && (inExcept.value().get().begin() != inExcept.value().get().end());

    if (!inReadValid && !inWriteValid && !inExceptValid) {
        return 0;
    }

    if (inReadValid) {
        detail::fillFDSet(readSet, inRead.value().get());
    }
    if (inWriteValid) {
        detail::fillFDSet(writeSet, inWrite.value().get());
    }
    if (inExceptValid) {
        detail::fillFDSet(exceptSet, inExcept.value().get());
    }

    auto ret = ::select( 0,
        inReadValid ? &readSet : nullptr,
        inWriteValid ? &writeSet : nullptr,
        inExceptValid ? &exceptSet : nullptr,
        timeout ? &tv : nullptr
    );

    if (ret == SOCKET_ERROR) {
        throw NET_LAST_EXCEPT("Failed to select sockets"sv);
    }

    if (inReadValid && outRead) {
        detail::retreiveFDSet(readSet, inRead.value().get(), outRead.value().get());
    }
    if (inWriteValid && outWrite) {
        detail::retreiveFDSet(writeSet, inWrite.value().get(), outWrite.value().get());
    }
    if (inExceptValid && outExcept) {
        detail::retreiveFDSet(exceptSet, inExcept.value().get(), outExcept.value().get());
    }

    return ret;
}

}   // namespace net

#endif // __net_H