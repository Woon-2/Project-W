#include "net/netLow.hpp"
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

std::list<Session> gSessions;
std::deque<Packet> gBroadcastQueue;

//std::forward_list<std::uint16_t> IDPool::idList_(std::numeric_limits<std::uint16_t>::max());

void precessPacket(Session& session) {
    auto& recvQueue = session.getRecvQueue();
    
    while(!recvQueue.empty()) {
        auto packet = recvQueue.front();
        recvQueue.pop_front();

        switch(packet.type) {
            case PacketType::CSHello:
                std::cout << "Received Hello From Client!\n";
                session.enqueuePacket(
                    Packet{
                        .size = 16u + sizeof(SCAssign),
                        .type = PacketType::SCAssign,
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
}

void sendPacket(Session& session) {
    for (const auto& packet : gBroadcastQueue) {
        session.enqueuePacket(packet);   
    }

    session.flushPackets();
}

void fillBroadcastQueue() {
    auto worldPacket =  Packet{
        .size = 16 + sizeof(SCWorld),
        .type = PacketType::SCWorld
    };

    for (std::size_t i = 0; i < maxConnection; ++i) {
        worldPacket.scWorld.xforms[i] = gXforms[i];
    }

    gBroadcastQueue.push_back(worldPacket);
}

void initXforms() {
    // temporary
    gXforms[0].pos = {52.94f, 14.82f - 25.f, 43.68f};
    gXforms[1].pos = {50.29f, 21.f - 25.f, 56.59f};
    gXforms[2].pos = {30.22f, 21.5f - 25.f, 52.51f};
    gXforms[3].pos = {41.4f, 16.f - 25.f, 46.1f};
    gXforms[4].pos = {41.5f, 17.5f - 25.f, 39.3f};
}

int main()
{
    net::initNet();

    auto listenSocket = net::TcpSocket();
    
    listenSocket.bind(net::SockAddr(net::Ipv4Addr(), net::Port(PORT)));
    listenSocket.listen(SOMAXCONN);

    int flag = 1;
    listenSocket.setSockOpt(SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&flag), sizeof(flag));

    flag = true;
    listenSocket.setSockOpt(IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));

  
    int retVal{};

    initXforms();
    IDPool::initList();

    while (true) {
        auto recvRequestSocks = std::vector<net::TcpSocket*>();
        auto recvReadySocks = std::vector<net::TcpSocket*>();
        auto sendRequestSocks = std::vector<net::TcpSocket*>();
        auto sendReadySocks = std::vector<net::TcpSocket*>();

		recvRequestSocks.push_back( &listenSocket );

        for (auto& session : gSessions) {
            recvRequestSocks.push_back(&session.sock());
            sendRequestSocks.push_back(&session.sock());
        }

        net::select<decltype(recvRequestSocks)>(recvRequestSocks, sendRequestSocks, {}, recvReadySocks, sendReadySocks, {}, 100ms);

        for (auto& sock : recvReadySocks) {
            if (sock->nativeHandle() == listenSocket.nativeHandle()) {
                auto clientSocket = listenSocket.WSAAcceptUc();
                if (clientSocket.nativeHandle() == INVALID_SOCKET) {
                    std::cerr << "accept failed\n";
                    break;
                }
                else {
                    std::cout << "Client Connected\n";
                }

                int flag = 1;
                clientSocket.setSockOpt(IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));

                if (gSessions.emplace_back(std::move(clientSocket)).id() == -1) {
                    ::closesocket(clientSocket.nativeHandle());
                    gSessions.pop_back();
                }
            }
            else{
                auto session = std::find_if(gSessions.begin(), gSessions.end(), [&sock](const auto& s) {
                    return s.sock().nativeHandle() == sock->nativeHandle();
                });

                if (session != gSessions.end()) {
                    session->recvPackets();
                    precessPacket(*session);
                }
            }
        }

        fillBroadcastQueue();

        for(const auto& sock : sendReadySocks) {
            auto session = std::find_if(gSessions.begin(), gSessions.end(), [&sock](const auto& s) {
                return s.sock().nativeHandle() == sock->nativeHandle();
            });

            if (session != gSessions.end()) {
                sendPacket(*session);
            }
        }

        gBroadcastQueue.clear();
    }

    net::relNet();
}