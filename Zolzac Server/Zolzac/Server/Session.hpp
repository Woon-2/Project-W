#ifndef __SESSION_HPP
#define __SESSION_HPP

#define ECS_SERVER

#include "netInclude.hpp"
#include "OverlappedEx.hpp"
#include "ecs.hpp"

#include <optional>
#include <forward_list>
#include <set>
#include <deque>
#include <atomic>

// IDPool ============================================
class IDPool {
public:
    static void initList( );
    static std::optional<std::int16_t> allocID( );
    static void deallocID( std::int16_t id );

private:
    static std::forward_list<std::uint16_t> idList_;
};
//====================================================

// NetExProcessorBase =================================================
class NetExProcessorBase {
public:
    NetExProcessorBase( ecs::Entity::ID entityID )
        : entityID_( entityID ), netId_( -1u ) {
        if ( auto allocatedId = IDPool::allocID( ); !allocatedId ) {
            std::cerr << "Failed to allocate netId\n";
        }
        else {
            netId_ = allocatedId.value( );
        }
    }

    virtual ~NetExProcessorBase( ) {
        if ( netId_ != -1u ) {
            IDPool::deallocID( netId_ );
        }
    }

    virtual void generatePackets( class Session& session ) = 0;
    virtual void processPacket( const Packet& packet ) = 0;
	
    ecs::Entity::ID entityID( ) const noexcept {
        return entityID_;
    }

    std::uint32_t netId( ) const noexcept {
        return netId_;
    }

    void addCategory( NetExCategory category ) {
        categories_.insert( category );
    }
    bool hasCategory( NetExCategory category ) const {
        return categories_.contains( category );
    }

private:
    std::set<NetExCategory> categories_;
    ecs::Entity::ID entityID_;
    std::uint32_t netId_;
};
//=====================================================================

// NetEx ==============================================================
class NetEx : public ecs::Component {
public:
    ENABLE_COMPONENT( NetEx );

    NetEx( const ecs::Entity& entity, std::unique_ptr<NetExProcessorBase>&& pNetExProcessor ) NOEXCEPT
        : ecs::Component( entity ), pProcessor_( std::move( pNetExProcessor ) ) { }

    void generatePackets( Session& session ) {
        pProcessor_->generatePackets( session );
    }
    void processPacket( const Packet& packet ) {
        pProcessor_->processPacket( packet );
    }

    std::uint32_t netId( ) const {
        return pProcessor_->netId( );
    }

    NetExProcessorBase* getProcessor( ) noexcept {
        return pProcessor_.get( );
    }

    void addCategory( NetExCategory category ) {
        pProcessor_->addCategory( category );
    }

    bool hasCategory( NetExCategory category ) const {
        return pProcessor_->hasCategory( category );
    }

private:
    std::unique_ptr<NetExProcessorBase> pProcessor_;
};
//=====================================================================

class SNetExSystem;
// Session ============================================================
class Session {
public:
	Session( ) : recvOver_( IO_OP::IO_RECV ) {
		std::cout << "Session default constructor called\n";
		exit( -1 );
	}

	~Session( ) {
		::closesocket( clientSocket_ );
	}

	Session( SOCKET socket, std::int16_t id )
		: clientSocket_{ socket }, id_{ id }, recvOver_( IO_OP::IO_RECV ),
		recvBytesRemain_{ 0 }, sendQueue_( ), recvBuffer_( ) {
		recvOver_.wsaBufs_.resize( 1 );
		doRecv( );
	}

	Session( const Session& ) = delete;
	Session& operator=( const Session& ) = delete;

	Session( Session&& ) noexcept;
	Session& operator=( Session&& ) noexcept;

	void doRecv( );
    void doSend( );
	void interpretData( DWORD bytesTransferred );

    void setNetSystem( SNetExSystem* netSystem );
    void enqueuePacket( const Packet& packet ) {
		sendQueue_.push_back( packet );
    }

    bool getAcceptFlag( ) const {
		return completedAccept.load( );
    }
    void setAcceptFlag( ) {
        completedAccept.store( true );
    }

private:
	SOCKET clientSocket_;
	std::int16_t id_;

    OverlappedEx recvOver_;
    std::array<char, bufferSize> recvBuffer_;
	std::uint16_t recvBytesRemain_;

    std::deque<Packet> sendQueue_;

    SNetExSystem* netSystem_ = nullptr;
	std::atomic_bool completedAccept{ false };

	void processPacket( const Packet& packet );
};
//=====================================================================

#endif	// __SESSION_HPP