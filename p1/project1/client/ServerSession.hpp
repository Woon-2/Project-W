#ifndef SERVER_SESSION_HPP
#define SERVER_SESSION_HPP

#include "pch.hpp"
#include "ServerSession.hpp"
#include "SessionManager.hpp"
#include "object.hpp"

class ServerSession : public PacketSession {
public:
	ServerSession( ) : player_( nullptr ) {}
	virtual ~ServerSession( ) { }

	virtual void onConnected( ) override {
		std::cout << "[Client] Connected to server.\n";
	}

	virtual void onDisconnected( ) override {
		std::cout << "[Client] Disconnected from server.\n";
	}

	virtual int32 onRecvPacket( uint8* buffer, int32 len ) override {
		auto packet = reinterpret_cast<Packet*>( buffer );
		switch ( static_cast<PacketType>( packet->header.id ) ) {
		case PacketType::scAssignId: {
			if ( player_ ) {
				player_->setId( packet->scAssignId.playerId );
			}
		}
			break;

		case PacketType::scEnter: {
			for( i32t pId : packet->scEnter.pIds ) {
				if( pId == player_->getId( ) ) {
					player_->setPos( mu::Vec3( packet->scEnter.x, packet->scEnter.y, packet->scEnter.z ) );
				}
				if ( pId != player_->getId( ) ) {
					//std::lock_guard<std::mutex> lock( gMtx );
					gObjects[ pId ] = std::make_shared<Object>( );
					gObjects[ pId ]->setPos( mu::Vec3( packet->scEnter.x, packet->scEnter.y, packet->scEnter.z ) );
				}
			}
		}
			break;

		case PacketType::scLeave:
			break;

		case PacketType::scMove: {
			auto pId = packet->scMove.playerId;
			if ( pId == player_->getId( ) ) {
				player_->setPos( mu::Vec3( packet->scMove.x, packet->scMove.y, packet->scMove.z ) );
			}
			else {
				//std::lock_guard<std::mutex> lock( gMtx );
				auto it = gObjects.find( pId );
				if ( it != gObjects.end( ) ) {
					it->second->setPos( mu::Vec3( packet->scMove.x, packet->scMove.y, packet->scMove.z ) );
				}
			}
		}
			break;
		}
		return len;
	}

	virtual void onSend( int32 len ) override {
		std::cout << "[Client] Sent " << len << " bytes to server.\n";
	}

	void setPlayer( const std::shared_ptr<Object>& player ) {
		player_ = player;
	}

	std::shared_ptr<Object> getPlayer( ) const {
		return player_;
	}

private:
	std::shared_ptr<Object> player_;
};

#endif // SERVER_SESSION_HPP