#include "net/protocol.hpp"
#include "net/netInclude.hpp"
#include "net/session.hpp"

#include "mathUtil.hpp"

#include <iostream>
#include <cstdint>
#include <vector>
#include <optional>
#include <limits>
#include <numeric>
#include <list>
#include <forward_list>
#include <map>
#include <utility>
#include <deque>
#include <algorithm>

RigidXform gXforms[maxConnection];

class IDPool {
public:
    static void initList() {
        idList_.resize(maxConnection);
        std::iota(idList_.begin(), idList_.end(), 0u);
    }

    static std::optional<std::uint16_t> allocID() {
        if(idList_.empty()){
            return std::nullopt;
        }

        auto id = idList_.front();
        idList_.pop_front();

        return id;
    }

    static void deallocID(std::uint16_t id) {
        idList_.push_front(id);
    }
    
    private:
    static std::forward_list<std::uint16_t> idList_;
};

std::forward_list<std::uint16_t> IDPool::idList_;
std::list<Session> gSessions;
std::deque<Packet> gBroadcastQueue;

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


//std::forward_list<std::uint16_t> IDPool::idList_(std::numeric_limits<std::uint16_t>::max());

void recvPacket(Session& session) {
    Packet packet{};
    auto recvSize = ::recv(session.sock(), reinterpret_cast<char*>(&packet), sizeof(Packet), 0);
    if (recvSize == SOCKET_ERROR) {
        std::cerr << "recv failed\n";
        return;
    }

    switch(packet.type) {
        case PacketType::CSHello:
            session.enqueuePacket(
                Packet{
                    .type = PacketType::SCAssign,
                    .size = sizeof(SCAssign),
                    .scAssign = {
                        .id = session.id()
                    }
                }
            );
            break;
            
        case PacketType::CSLeave:
            std::erase(gSessions, session);
            break;

        case PacketType::CSWorld:
            gXforms[session.id()] = packet.csWorld.xform;
            break;

        default:
            break;
    }
}

void sendPacket(Session& session) {
    for (const auto& packet : gBroadcastQueue) {
        session.enqueuePacket(packet);   
    }

    session.flushPackets();
}

void fillBroadcastQueue() {
    auto worldPacket =  Packet{
        .type = PacketType::SCWorld,
        .size = sizeof(SCWorld)
    };

    for (std::size_t i = 0; i < maxConnection; ++i) {
        worldPacket.scWorld.xforms[i] = gXforms[i];
    }

    gBroadcastQueue.push_back(worldPacket);
}

void initXforms() {
    // temporary
    gXforms[0].pos = {52.94f, 14.82f, 43.68f};
    gXforms[1].pos = {50.29f, 21.f, 56.59f};
    gXforms[2].pos = {30.22f, 21.5f, 52.51f};
    gXforms[3].pos = {41.4f, 16.f, 46.1f};
    gXforms[4].pos = {41.5f, 17.5f, 39.3f};
}

int main()
{
    WSADATA wsaData;
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 0;
    }

    auto listenSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "socket failed\n";
        return 0;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = ::htonl(INADDR_ANY);
    serverAddr.sin_port = ::htons(PORT);
    if (::bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "bind failed\n";
        return 0;
    }

    if (::listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed\n";
        return 0;
    }

    int flag = 1;
    setsockopt(listenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    fd_set readSet;
    fd_set writeSet;
    int retVal{};

    initXforms();
    IDPool::initList();

    while (true) {
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);

        FD_SET(listenSocket, &readSet);
        for (const auto& s : gSessions){
            FD_SET(s.sock(), &readSet);
            FD_SET(s.sock(), &writeSet);
        }

        retVal = ::select(0, &readSet, &writeSet, nullptr, nullptr);
        if (retVal == SOCKET_ERROR){
            std::cerr << "select failed\n";
            break;
        }

        if (FD_ISSET(listenSocket, &readSet)){
            sockaddr_in clientAddr{};
            int addrLen = sizeof(clientAddr);
            auto clientSocket = ::accept(listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "accept failed\n";
                break;
            }

            int flag = 1;
            setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

            if(auto id = IDPool::allocID()) {
                gSessions.emplace_back(clientSocket);
            }
            else {
                ::closesocket(clientSocket);
            }
        }

        for (auto& session : gSessions) {
            if (FD_ISSET(session.sock(), &readSet)) {
                recvPacket(session);
            }
        }

        fillBroadcastQueue();

        for (auto& session : gSessions) {
            if (FD_ISSET(session.sock(), &writeSet)) {
                sendPacket(session);
            }
        }

        gBroadcastQueue.clear();
    }
}