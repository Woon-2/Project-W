#include "pch.hpp"
#include "global.hpp"
#include "ServerSession.hpp"
#include "SendBuffer.hpp"
#include "online/onlineGame.hpp"

extern GFX gGfx;

void ServerSession::onConnected( ) {
	std::cout << "[Client] Connected to server.\n";

	pGame = std::make_unique<Online::Game>( );
	static_cast<Online::Game*>( pGame.get( ) )->setupStage( );
	gReady.store( true );
}

void ServerSession::onDisconnected( ) {
	std::cout << "[Client] Disconnected from server.\n";

	auto packet = Packet{
		.header = {
			.size = sizeof( PacketHeader ) + sizeof( CSLeavePacket ),
			.id = static_cast<std::uint16_t>( PacketType::csLeave )
		}
	};

	auto sendBuffer = std::make_shared<SendBuffer>( sizeof( Packet ) );
	sendBuffer->copyData( &packet, sizeof( Packet ) );
	send( sendBuffer );
}

int32 ServerSession::onRecvPacket( uint8* buffer, int32 len ) {
	auto packet = reinterpret_cast<Packet*>( buffer );
	switch ( static_cast<PacketType>( packet->header.id ) ) {
	case PacketType::scAssignId: {
		//player_ = gPlayer;
		player_->setId( packet->scAssignId.playerId );

		//std::lock_guard<std::mutex> lock( gMtx );
		gObjects[ player_->getId( ) ] = player_;
		break;
	}

	case PacketType::scEnter: {
		i32t playerCount = packet->scEnter.playerCount;
		for ( i32t i = 0; i < playerCount; ++i ) {
			bool found = false;
			for ( auto it = gObjects.begin( ); it != gObjects.end( ); ++it ) {
				if ( it->first == packet->scEnter.pIds[ i ] ) {
					found = true;
					break;
				}
			}

			if ( !found ) {
				auto newObject = std::make_shared<Object>( );
				newObject->setId( packet->scEnter.pIds[ i ] );
				newObject->setPos( mu::Vec3(
					packet->scEnter.x[ i ],
					packet->scEnter.y[ i ],
					packet->scEnter.z[ i ]
				) );
				newObject->setModel( gGfx.modelPlayer( ) );
				newObject->setScale( 0.15f );

				std::lock_guard<std::mutex> lock( gMtx );
				gObjects[ packet->scEnter.pIds[ i ] ] = newObject;
			}
		}
		break;
	}

	case PacketType::scLeave: {
		auto pId = packet->scLeave.playerId;
		std::lock_guard<std::mutex> lock( gMtx );
		gObjects.erase( pId );
		break;
	}

	case PacketType::scMove: {
		auto pId = packet->scMove.playerId;
		if ( pId == player_->getId( ) ) {
			player_->setPos( mu::Vec3( packet->scMove.x, packet->scMove.y, packet->scMove.z ) );
		}
		else {
			std::lock_guard<std::mutex> lock( gMtx );
			auto it = gObjects.find( pId );
			if ( it != gObjects.end( ) ) {
				it->second->setPos( mu::Vec3( packet->scMove.x, packet->scMove.y, packet->scMove.z ) );
			}
		}
		break;
	}
	}
	return len;
}