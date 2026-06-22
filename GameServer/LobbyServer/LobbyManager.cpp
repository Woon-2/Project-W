#include "lspch.hpp"
#include "LobbyManager.hpp"
#include "LobbyRoom.hpp"
#include "ObjectPool.hpp"

void LobbyManager::configureRoomEndpoint(const NetworkEndpoint& endpoint) {
	roomServerEndpoint_ = endpoint;
}

std::shared_ptr<LobbyRoom> LobbyManager::createRoom() {
	std::unique_lock lock( mutex_ );

	std::string code;
	do {
		code = generateCode();
	} while ( rooms_.count( code ) );

	auto room = ObjectPool<LobbyRoom>::makeShared( code, roomServerEndpoint_ );
	rooms_[ code ] = room;

	return room;
}

std::shared_ptr<LobbyRoom> LobbyManager::findRoom( const std::string& code ) {
	std::shared_lock lock( mutex_ );
	auto it = rooms_.find( code );
	return it != rooms_.end() ? it->second : nullptr;
}

void LobbyManager::removeRoom( const std::string& code ) {
	std::unique_lock lock( mutex_ );

	// 맵에서 제거하면 shared_ptr ref가 줄고, 마지막 보유자(참가 경쟁 중인 스레드 등)가 놓는 순간
	// ObjectPool deleter가 ~LobbyRoom 실행 후 풀로 반환한다(여기서 직접 delete하지 않는다).
	rooms_.erase( code );
}

std::shared_mutex LobbyManager::mutex_;
std::unordered_map<std::string, std::shared_ptr<LobbyRoom>> LobbyManager::rooms_;
NetworkEndpoint LobbyManager::roomServerEndpoint_{};
