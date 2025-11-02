#ifndef GAME_SESSION_HPP
#define GAME_SESSION_HPP

#include "Session.hpp"
#include "SendBuffer.hpp"
#include "GameSessionManager.hpp"

class GameSession : public PacketSession {
public:
	GameSession( ) {}
	virtual ~GameSession( ) {
		std::cout << "GameSession destructed.\n";
	}

	virtual void onConnected( ) override {
		GameSessionManager::add( std::static_pointer_cast<GameSession>( shared_from_this( ) ) );
	}

	virtual void onDisconnected( ) override {
		GameSessionManager::remove( std::static_pointer_cast<GameSession>( shared_from_this( ) ) );
	}

	virtual int32 onRecvPacket( uint8* buffer, int32 len ) override {
		auto header = reinterpret_cast<PacketHeader*>( buffer );
		std::cout << "Packet size: " << header->size << ", id: " << header->id << '\n';
		return len;
	}

	virtual void onSend( int32 len ) override {
		// std::cout << "GameSession sent " << len << " bytes.\n";
	}
};

#endif // GAME_SESSION_HPP