#include "net.hpp"

#include <string>
#include <string_view>
#include <iostream>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <list>
#include <cstdint>
#include <cstdlib>

inline constexpr auto maxPacketSize = 400u;

std::vector<net::TcpSocket> gUninitializedClients;
std::vector<net::TcpSocket> gSessionClients;
static std::uint32_t gId = 0u;

struct World {
    struct Obj {
        float x;
        float y;
        float z;
        float ovx;
        float ovy;
        float ovz;
        float os;
        bool active = false;
    } obj[10];
} gWorld;

enum class PACKET_TYPE : std::uint8_t {
    HELLO,
    UPDATE,
    LEAVE
};

#pragma pack(push, 1)

struct HelloClientPacket {
    PACKET_TYPE type;
};

struct HelloServerPacket {
    PACKET_TYPE type;
    std::uint32_t id;
    World world;
};

struct UpdateClientPacket {
    PACKET_TYPE type;
    World::Obj obj;
};

struct UpdateServerPacket {
    PACKET_TYPE type;
    World world;
};

struct LeavePacket {
    PACKET_TYPE type;
};

#pragma pack(pop)

std::map< net::TcpSocket*, std::uint32_t > gSock2WorldIdxMap;
std::list<std::uint32_t> gIdPool = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

std::uint32_t allocID() {
    auto id = gIdPool.front();
    gIdPool.pop_front();
    return id;
}

void freeID(std::uint32_t id) {
    gIdPool.push_back(id);
}

// gWorld.obj[ gSock2WorldIdxMap.at(client) ]

void init(net::TcpSocket& recvUninitSock) {
    // process
    auto id = allocID();

    gWorld.obj[id] = {
        .x = 0.0f,
        .y = 0.0f,
        .z = 0.0f,
        .ovx = 0.0f,
        .ovy = 0.0f,
        .ovz = 0.0f,
        .os = 1.0f,
        .active = true
    };

    auto packet = HelloServerPacket{
        .type = PACKET_TYPE::HELLO,
        .id = id,
        .world = gWorld
    };

    net::enNonb(recvUninitSock);
    recvUninitSock.sendUc(&packet, sizeof(packet));
    net::disNonb(recvUninitSock);

    gSessionClients.push_back(std::move(recvUninitSock));
    gSock2WorldIdxMap.try_emplace(&gSessionClients.back(), id);
    std::erase(gUninitializedClients, recvUninitSock);
}

int main() { 
    try {

    net::initNet();
    auto sock = net::TcpSocket();
    sock.bind(net::SockAddr(net::Ipv4Addr(), net::Port(55555u)));
    sock.listen(10u);

    auto log = std::vector< std::vector<std::string> >();

    while (sock.active()) {
        log.emplace_back();
        log.back().reserve(gUninitializedClients.size());

        if (!gIdPool.empty()) {
            net::enNonb(sock);
            auto client = sock.acceptUc();
            net::disNonb(sock);

            if (client.active()) {
                gUninitializedClients.push_back(std::move(client));
            }
        }

        auto uninitSocksView = std::vector<net::TcpSocket*>{};
        uninitSocksView.reserve(10u);

        std::ranges::transform( gUninitializedClients, std::back_inserter(uninitSocksView), [](auto& client) { return &client; } );

        auto readable = std::vector<net::TcpSocket*>{};
        readable.reserve(uninitSocksView.size());

        net::select<decltype(readable)>(uninitSocksView, {}, {}, readable, {}, {}, {});

        for (auto client : readable) {
            if (client == &sock) {
                continue;
            }

            char buffer[maxPacketSize];
            auto byteRecv = client->recv(buffer, maxPacketSize);

            if ( ( *reinterpret_cast<HelloClientPacket*>(&buffer) ).type == PACKET_TYPE::HELLO ) {
                init(*client);
            }

            log.back().push_back(std::string(buffer, byteRecv));

            std::cout << log.back().back() << '\n';
        }

        auto sessionSocksView = std::vector<net::TcpSocket*>{};
        sessionSocksView.reserve(10u);

        std::ranges::transform( gSessionClients, std::back_inserter(sessionSocksView), [](auto& client) { return &client; } );

        readable.clear();

        net::select<decltype(readable)>(sessionSocksView, {}, {}, readable, {}, {}, {});

        for (auto client : readable) {
            char buffer[maxPacketSize];
            auto byteRecv = client->recv(buffer, maxPacketSize);

            if ( ( *reinterpret_cast<UpdateClientPacket*>(&buffer) ).type == PACKET_TYPE::UPDATE ) {
                auto& obj = ( *reinterpret_cast<UpdateClientPacket*>(&buffer) ).obj;
                std::memcpy(&gWorld.obj[ gSock2WorldIdxMap.at(client) ], &obj, sizeof(obj));
            }

            log.back().push_back(std::string(buffer, byteRecv));

            std::cout << log.back().back() << '\n';
        }

        readable.clear();

        net::select<decltype(readable)>(sessionSocksView, {}, {}, readable, {}, {}, {});

        for (auto client : readable) {
            char buffer[maxPacketSize];
            auto byteRecv = client->recv(buffer, maxPacketSize);

            if ( ( *reinterpret_cast<UpdateClientPacket*>(&buffer) ).type == PACKET_TYPE::LEAVE ) {
                auto id = gSock2WorldIdxMap.at(client);
                gWorld.obj[id].active = false;
                
                freeID(id);

                gSock2WorldIdxMap.erase(client);
                gSessionClients.erase(std::find(gSessionClients.begin(), gSessionClients.end(), *client));
            }

            log.back().push_back(std::string(buffer, byteRecv));

            std::cout << log.back().back() << '\n';
        }
        
        auto packet = UpdateServerPacket{
            .type = PACKET_TYPE::UPDATE,
            .world = gWorld
        };

        for (auto& client : gSessionClients) {
            net::enNonb(client);
            client.sendUc(&packet, sizeof(packet));
            net::disNonb(client);
        }
    }

    } catch (const net::Exception& e) {
        std::cout << e.what() << '\n';
    }

    net::relNet();
    system("pause");
}