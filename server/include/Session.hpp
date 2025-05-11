#ifndef __SESSION_HPP
#define __SESSION_HPP

#include "stdafx.hpp"

#include "game/level.hpp"

#include "net/netInclude.hpp"
#include "OverlappedEx.hpp"

void errorDisplay( std::string_view where, int error );

using ATOMIC_SOCKET = std::atomic<SOCKET>;

class Session {
public:
    static constexpr std::size_t recvBufSize = 40960u;

    using PacketProcessor = void(*)(Packet& packet, Session& session);

	Session();
	~Session( );

	Session( const Session& ) = delete;
	Session& operator=( const Session& ) = delete;

	Session( Session&& ) noexcept;
	Session& operator=( Session&& ) noexcept;

    // thread-unsafe
    Session& init(SOCKET socket, i16t sessionId);
    bool close();
    bool valid() const {
        return clientSocket_.load() != INVALID_SOCKET;
    }
    bool accessReady() const {
        return valid() && completedAccept.load( );
    }

	void doRecv( );
    void doSend( );
    static void doBroadcast( pmr::vector<Session*> sessions );
	void interpretData( DWORD bytesTransferred );

    void enqueuePacket( const Packet& packet ) {
		sendQueue_.push_back( packet );
    }
    static void enqueueBroadcastPacket( const Packet& packet ) {
        sBroadcastQueue_.push( packet );
    }

    // do not call this function without serious consideration
    // this function is used to revert the last enqueue packet
    void revertEnqueuePacket( ) {
        sendQueue_.pop_back( );
    }

    bool getAcceptFlag( ) const {
		return completedAccept.load( );
    }
    void setAcceptFlag( ) {
        completedAccept.store( true );
    }

    void setEntityId( ecs::Entity::ID entityId ) {
        entityId_ = entityId;
    }
    ecs::Entity::ID getEntityId( ) const {
        return entityId_;
    }

    void setPacketProcessor( PacketProcessor packetProcessor ) {
        packetProcessor_ = packetProcessor;
    }
    PacketProcessor getPacketProcessor( ) const {
        return packetProcessor_;
    }

    u16t id( ) const {
        return id_;
    }

private:
    static ccQueue<Packet> sBroadcastQueue_;

    ATOMIC_SOCKET clientSocket_;
	i16t id_;

    ecs::Entity::ID entityId_;

    OverlappedEx recvOver_;
    pmr::vector<char> recvBuffer_;
	u16t recvBytesRemain_;

    std::deque<Packet> sendQueue_;

    PacketProcessor packetProcessor_;

	std::atomic_bool completedAccept;
};

// dummy model to store rotation coordinate system
class DummyModel : public ecs::Component {
public:
    ENABLE_COMPONENT( DummyModel )

    DummyModel( const ecs::Entity& entity, gameEngine::Coord& coordComp )
        : Component( entity ), coord_( ) {
        coord_.setParent( &coordComp.get( ) );
    }

    const gfx::coord::System& coord( ) const NOEXCEPT {
        return coord_;
    }

    gfx::coord::System& coord( ) NOEXCEPT {
        return coord_;
    }

private:
    gfx::coord::System coord_;
};

#endif	// __SESSION_HPP