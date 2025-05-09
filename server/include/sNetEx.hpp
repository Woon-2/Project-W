#ifndef __SERVER_NETEX_HPP
#define __SERVER_NETEX_HPP

#include "Session.hpp"
#include "ecs.hpp"

#include "net/netEx.hpp"
#include "game/level.hpp"

#include <vector>
#include <map>
#include <optional>
#include <list>
#include <mutex>

class SNetExSystem : public ecs::System<NetEx> {
public:
    SNetExSystem() = default;

    SNetExSystem(std::vector<Session*>&& pSessions)
        : addSessionLock_(),
        netIdToNetEx_( ), pSessions_( std::move( pSessions ) ),
        pReservedSessions_(), freeHelicopters_() { }

    void addEntity(ecs::Entity& entity);
    void addSession(Session& session);

    void preUpdate( );
    void postUpdate();

    void doSend( ) {
        for ( auto pSession : pSessions_ ) {
			if ( pSession->getAcceptFlag( ) ) {
				pSession->doSend( );
			}
        }
    }

    NetEx* getNetEx( std::uint16_t netId ) {
        if ( netIdToNetEx_.contains( netId ) ) {
            return netIdToNetEx_[ netId ];
        }
		return nullptr;
    }

private:
    void processPacket(const Packet& packet);

    std::mutex addSessionLock_;
    std::map<std::uint32_t, NetEx*> netIdToNetEx_;
    std::vector<Session*> pSessions_;
    std::vector<Session*> pReservedSessions_;
    std::list<NetEx*> freeHelicopters_; 
};

class SNetExHelicopter : public NetExProcessorBase {
public:
    SNetExHelicopter(ecs::Entity::ID entityId)
        : NetExProcessorBase(entityId) {}

    void generatePackets(Session& session) override;
    void processPacket(const Packet& packet) override;

private:
    void handleCSWorld(const CSWorld& csWorld);

    std::optional<CSWorld> lastReceivedWorld_;
};

class SNetExAI : public NetExProcessorBase {
public:
    SNetExAI(ecs::Entity::ID entityId)
        : NetExProcessorBase(entityId) {}

    void generatePackets(Session& session) override;
    void processPacket(const Packet& packet) override;

private:
    void handleCSWorld(const CSWorld& csWorld);
};

#endif // __SERVER_NETEX_HPP