#include "lspch.hpp"
#include "GameSession.hpp"
#include "LobbyRoom.hpp"
#include "PacketManager.hpp"
#include "GameSessionManager.hpp"
#include "IdPool.hpp"

GameSession::~GameSession() {
	IdPool::push( id() );
	std::cout << "Lobby client disconnected. id: " << id() << '\n';
}

void GameSession::onConnected() {
	std::cout << "Lobby client connected. id: " << id() << '\n';

	// 매니저가 접속 중 세션의 소유 ref를 보관한다.
	GameSessionManager::add( std::static_pointer_cast<GameSession>( shared_from_this() ) );
}

void GameSession::onDisconnected() {
	if ( myRoom_ ) {
		myRoom_->leave( this );
		myRoom_ = nullptr;
	}

	// 매니저 ref를 놓는다. pending I/O ref까지 모두 사라지면 ~GameSession이 실행되며
	// 풀로 반환된다(IdPool::push는 ~GameSession에서 수행).
	GameSessionManager::remove( static_cast<uint16>( id() ) );
}

void GameSession::processPacket( byte* buffer, int32 len ) {
	PacketManager::handlePacket( this, buffer, len );
}
