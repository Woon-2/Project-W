#ifndef GAME_SESSION_HPP
#define GAME_SESSION_HPP

#include "Session.hpp"
#include "SendBuffer.hpp"
#include "GameSessionManager.hpp"
#include "Service.hpp"

class GameSession : public PacketSession {
public:
	GameSession( ) {}
	virtual ~GameSession( ) {
		std::cout << "GameSession " << getId( ) << " destructed.\n";
	}

	virtual void onConnected( ) override {
		std::cout << "GameSession " << getId( ) << " connected.\n";
		GameSessionManager::add( std::static_pointer_cast<GameSession>( shared_from_this( ) ) );

		auto packet = Packet{
			.header = {
				.size = sizeof( PacketHeader ) + sizeof( SCAssignIdPacket ),
				.id = static_cast<uint16>( PacketType::scAssignId )
			},
			.scAssignId = {
				.playerId = getId( )
			}
		};

		auto sendBuffer = std::make_shared<SendBuffer>( sizeof( Packet ) );
		sendBuffer->copyData( &packet, sizeof( Packet ) );
		send( sendBuffer );

		auto enterPacket = Packet{
			.header{
				.size = sizeof( PacketHeader ) + sizeof( SCEnterPacket ),
				.id = static_cast<uint16>( PacketType::scEnter )
			},
		};
		enterPacket.scEnter.playerCount = getService( )->getSessionCount( );

		int32 index = 0;
		for( const auto& session : GameSessionManager::getSessions( ) ) {
			enterPacket.scEnter.pIds[ index ] = session->getId( );
			enterPacket.scEnter.x[ index ] = session->x( );
			enterPacket.scEnter.y[ index ] = session->y( );
			enterPacket.scEnter.z[ index ] = session->z( );
			++index;
		}

		sendBuffer = std::make_shared<SendBuffer>( sizeof( Packet ) );
		sendBuffer->copyData( &enterPacket, sizeof( Packet ) );
		GameSessionManager::broadcast( sendBuffer );
	}

	virtual void onDisconnected( ) override {
		GameSessionManager::remove( std::static_pointer_cast<GameSession>( shared_from_this( ) ) );
	}

	virtual int32 onRecvPacket( uint8* buffer, int32 len ) override {
		std::cout << "GameSession " << getId( ) << " received packet of length " << len << ".\n";
		auto packet = reinterpret_cast<Packet*>( buffer );
		switch ( static_cast<PacketType>( packet->header.id ) ) {
		case PacketType::csEnter:
			break;

		case PacketType::csLeave:
			break;

		case PacketType::csMove: {
			Packet sendPacket{ };
			sendPacket.scMove.playerId = getId( );
			if( packet->csMove.dir == direction::w ) {
				sendPacket.header.size = sizeof( PacketHeader ) + sizeof( SCMovePacket );
				sendPacket.header.id = static_cast<uint16>( PacketType::scMove );
				sendPacket.scMove.x = x_;
				sendPacket.scMove.y = y_;
				sendPacket.scMove.z = z_ + 0.01f;
			}
			else if( packet->csMove.dir == direction::a ) {
				sendPacket.header.size = sizeof( PacketHeader ) + sizeof( SCMovePacket );
				sendPacket.header.id = static_cast<uint16>( PacketType::scMove );
				sendPacket.scMove.x = x_ - 0.01f;
				sendPacket.scMove.y = y_;
				sendPacket.scMove.z = z_;
			}
			else if( packet->csMove.dir == direction::s ) {
				sendPacket.header.size = sizeof( PacketHeader ) + sizeof( SCMovePacket );
				sendPacket.header.id = static_cast<uint16>( PacketType::scMove );
				sendPacket.scMove.x = x_;
				sendPacket.scMove.y = y_;
				sendPacket.scMove.z = z_ - 0.01f;
			}
			else if( packet->csMove.dir == direction::d ) {
				sendPacket.header.size = sizeof( PacketHeader ) + sizeof( SCMovePacket );
				sendPacket.header.id = static_cast<uint16>( PacketType::scMove );
				sendPacket.scMove.x = x_ + 0.01f;
				sendPacket.scMove.y = y_;
				sendPacket.scMove.z = z_;
			}

			x_ = sendPacket.scMove.x;
			y_ = sendPacket.scMove.y;
			z_ = sendPacket.scMove.z;

			auto sendBuffer = std::make_shared<SendBuffer>( sizeof( Packet ) );
			sendBuffer->copyData( &sendPacket, sizeof( Packet ) );
			GameSessionManager::broadcast( sendBuffer );
		}
			break;
		}

		return len;
	}

	virtual void onSend( int32 len ) override {
		// std::cout << "GameSession sent " << len << " bytes.\n";
	}

	float x( ) const { return x_; }
	float y( ) const { return y_; }
	float z( ) const { return z_; }

private:
	float x_{ 0.f };
	float y_{ 0.f };
	float z_{ 0.f };
};

#endif // GAME_SESSION_HPP