#include "pch.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "SendBuffer.hpp"
#include "RoomManager.hpp"

void Room::enter( const SPGameSession& user ) {
	std::lock_guard<std::recursive_mutex> lock( mtx_ );
	if ( std::ranges::find( users_, user ) != users_.end( ) ) {
		return;
	}

	users_.push_back( user );
	std::cout << "user count " << users_.size( ) << '\n';

	auto packet = Packet{
		.header = {
			.size = sizeof( PacketHeader ) + sizeof( SCEnterPacket ),
			.id = static_cast<uint16>( PacketType::scEnter )
		},
		.scEnter = {
			.playerCount = static_cast<int32>( users_.size( ) )
		}
	};

	for ( auto i = 0; i < users_.size( ); ++i ) {
		packet.scEnter.pIds[ i ] = users_[ i ]->getId( );
		packet.scEnter.x[ i ] = users_[ i ]->x( );
		packet.scEnter.y[ i ] = users_[ i ]->y( );
		packet.scEnter.z[ i ] = users_[ i ]->z( );
	}

	int32 packetSize = sizeof( Packet );
	auto sendBuffer = std::make_shared<SendBuffer>( packetSize );
	sendBuffer->copyData( &packet, packetSize );
	broadcast( sendBuffer );
}

void Room::leave( const SPGameSession& user ) {
	std::lock_guard<std::recursive_mutex> lock( mtx_ );

	auto packet = Packet{
		.header = {
			.size = sizeof( PacketHeader ) + sizeof( SCLeavePacket ),
			.id = static_cast<uint16>( PacketType::scLeave )
		},
		.scLeave = {
			.playerId = user->getId( )
		}
	};

	int32 packetSize = sizeof( Packet );
	auto sendBuffer = std::make_shared<SendBuffer>( packetSize );
	sendBuffer->copyData( &packet, packetSize );
	broadcast( sendBuffer );

	std::erase_if( users_, [&]( const SPGameSession& u ) {
		return u == user;
	} );

	if ( empty( ) ) {
		RoomManager::removeRoom( roomId_ );
	}
}

void Room::broadcast( const SPSendBuffer& sendBuffer ) {
	std::lock_guard<std::recursive_mutex> lock( mtx_ );
	for ( auto& user : users_ ) {
		user->send( sendBuffer );
	}
}

bool Room::empty( ) {
	std::lock_guard<std::recursive_mutex> lock( mtx_ );
	return users_.empty( );
}
