#include "pch.hpp"
#include "global.hpp"
#include "ServerSession.hpp"
#include "SendBuffer.hpp"
#include "online/onlineGame.hpp"

extern std::unique_ptr<IGame> pGame;
extern moodycamel::ConcurrentQueue<Online::Message> messageQueue;

void ServerSession::onConnected( ) {
	std::cout << "[Client] Connected to server.\n";

	pGame = std::make_unique<Online::Game>( );
	auto onlineGame = static_cast<Online::Game*>( pGame.get( ) );
	onlineGame->setupStage( );
	onlineGame->setServerSession( std::static_pointer_cast<ServerSession>( shared_from_this( ) ) );

	player_ = onlineGame->getPlayer( );

	gReady.store( true );
}

void ServerSession::onDisconnected( ) {
	std::cout << "[Client] Disconnected from server.\n";
}

int32 ServerSession::onRecvPacket( uint8* buffer, int32 len ) {
	//std::cout << "Client " << player_->getId( ) << '\n';
	auto packet = reinterpret_cast<Packet*>( buffer );

	switch ( static_cast<PacketType>( packet->header.id ) ) {
	case PacketType::scAssignId: {
		player_->setId( packet->scAssignId.playerId );
		static_cast<Online::Game*>( pGame.get( ) )->addPlayer( player_ );
		break;
	}

	case PacketType::scEnter: {
		auto playerCount = packet->scEnter.playerCount;
		for ( std::int32_t i = 0; i < playerCount; ++i ) {
			auto pId = packet->scEnter.pIds[ i ];

			if ( static_cast<Online::Game*>( pGame.get( ) )->findPlayer( pId ) ) {
				continue;
			}

			auto x = packet->scEnter.x[ i ];
			auto z = packet->scEnter.z[ i ];
			static_cast<Online::Game*>( pGame.get( ) )->createPlayer( pId, x, 0.f, z );
		}
		break;
	}

	case PacketType::scLeave: {
		auto pId = packet->scLeave.playerId;
		//std::lock_guard<std::mutex> lock( gMtx );
		static_cast<Online::Game*>( pGame.get( ) )->removePlayer( pId );
		break;
	}

	case PacketType::scMove: {
		auto message = Online::Message{
			.type = Online::MsgType::PlayerMove,
			.playerId = packet->scMove.playerId,
			.x = packet->scMove.x,
			.z = packet->scMove.z
		};
		messageQueue.enqueue(message);
		break;
	}
	}
	return len;
}