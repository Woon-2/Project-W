#include "sNetEx.hpp"

#include "net/netLow.hpp"
#include "net/protocol.hpp"
#include "net/netInclude.hpp"
#include "net/session.hpp"

#include "game/level.hpp"
#include "game/physicsSystem.hpp"
#include "coord.hpp"
#include "resourcePath.hpp"

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
#include <chrono>
#include <random>

std::list<Session> gSessions;
std::uniform_real_distribution<float> gDist(-1.f, 1.f);
std::mt19937 gRng(std::random_device{}());

int main()
{
    using Clock = std::chrono::high_resolution_clock;
    using Seconds = std::chrono::duration<float>;
    static constexpr auto frameRate = 0.016f;
    static constexpr auto directionChangeRate = 2.f;
    float directionChangeCounter = 0.f;

    ecs::init( ecs::InitDesc{ .threadCnt = 1u, .entityPoolSize = 0x160u } );
    net::initNet();
    IDPool::initList();

    auto listenSocket = net::TcpSocket();
    
    listenSocket.bind(net::SockAddr(net::Ipv4Addr(), net::Port(PORT)));
    listenSocket.listen(SOMAXCONN);

    int flag = 1;
    listenSocket.setSockOpt(SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&flag), sizeof(flag));

    flag = true;
    listenSocket.setSockOpt(IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));

  
    int retVal{};

    auto levelEntity = gameEngine::LevelRegion(resourcePath/"LevelGraph.bin");
    
    auto netSystem = SNetExSystem();
    auto physicsSystem = PhysicsSystem();
    auto coordRoot = gameEngine::CoordRoot();

    auto entities = levelEntity.instantiateAllObjects(coordRoot.get());
    auto directions = std::vector<mu::Vec3>(entities.size() - 5u);
    for (auto& dir : directions) {
        dir = mu::Vec3(gDist(gRng), 0.f, gDist(gRng));
    }
    // add netEx, rigidBody
    
    for (std::size_t i = 0u; i < entities.size(); ++i) {
        auto& entity = entities[i];
        if (i < 5u) {
            entity.createComponent<NetEx>(std::make_unique<SNetExHelicopter>(entity.id().value()), static_cast<std::uint32_t>(i));
        }
        else {
            entity.createComponent<NetEx>(std::make_unique<SNetExAI>(entity.id().value(), static_cast<std::uint32_t>(i)), static_cast<std::uint32_t>(i));
        }
        entity.createComponent<RigidBody>();

        netSystem.addEntity(entity);
        physicsSystem.addEntity(entity);
        coordRoot.addEntity(entity);
    }

    auto lastTp = Clock::now();

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
                else {
                    netSystem.addSession(gSessions.back());
                }
            }
            else{
                auto session = std::find_if(gSessions.begin(), gSessions.end(), [&sock](const auto& s) {
                    return s.sock().nativeHandle() == sock->nativeHandle();
                });

                if (session != gSessions.end()) {
                    session->recvPackets();
                }
            }
        }

        // update game ===============
        auto tp = Clock::now();
        auto elapsed = std::chrono::duration_cast<Seconds>(tp - lastTp).count();

        if (elapsed < frameRate / 2.f) {
            std::this_thread::sleep_for(std::chrono::duration<float>(frameRate - elapsed));
            elapsed = frameRate;
        }

        netSystem.preUpdate();

        // physically update
        const float forceStep_ = 800.f;
        for (std::size_t i = 5u; i < entities.size(); ++i) {
            auto& entity = entities[i];
            auto& coord = entity.as<gameEngine::Coord>();
            auto& rigidBody = entity.as<RigidBody>();
            auto& netEx = entity.as<NetEx>();

            rigidBody.addForce(directions[i - 5u] * forceStep_ * elapsed);
        }

        physicsSystem.update(elapsed);

        for (std::size_t i = 5u; i < entities.size(); ++i) {
            auto& entity = entities[i];
            auto& coord = entity.as<gameEngine::Coord>();
            auto& rigidBody = entity.as<RigidBody>();
            coord.get() << mu::translate( rigidBody.deltaPosition() );
        }

        directionChangeCounter += elapsed;
        if (directionChangeCounter > directionChangeRate) {
            directionChangeCounter = 0.f;
            for (auto& dir : directions) {
                dir = mu::Vec3(gDist(gRng), 0.f, gDist(gRng));
            }
        }

        coordRoot.update();
        
        lastTp = tp;

        // fill broadcast queue
        netSystem.postUpdate();

        // ===========================

        for(const auto& sock : sendReadySocks) {
            auto session = std::find_if(gSessions.begin(), gSessions.end(), [&sock](const auto& s) {
                return s.sock().nativeHandle() == sock->nativeHandle();
            });

            if (session != gSessions.end()) {
                session->flushPackets();
            }
        }
    }

    net::relNet();
}